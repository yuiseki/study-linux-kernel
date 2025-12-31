#define _GNU_SOURCE
#include "observe.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#define STRACE_PATH "/usr/bin/strace"
#define GREP_PATH   "/usr/bin/grep"
#define ENV_PATH    "/usr/bin/env"

static int mkdir_if_needed(const char *path, mode_t mode)
{
	if (mkdir(path, mode) == 0)
		return 0;
	if (errno == EEXIST)
		return 0;
	return -1;
}

static int mkdir_p_2level(const char *a, const char *b)
{
	// a/b を作る（a が無ければ作る）
	if (mkdir_if_needed(a, 0755) != 0)
		return -1;

	char buf[PATH_MAX];
	snprintf(buf, sizeof(buf), "%s/%s", a, b);
	if (mkdir_if_needed(buf, 0755) != 0)
		return -1;
	return 0;
}

static int ensure_log_root(char *out_root, size_t out_sz)
{
	// 1) ./logs/minishell
	if (mkdir_p_2level("./logs", "minishell") == 0)
	{
		snprintf(out_root, out_sz, "%s", "./logs/minishell");
		return 0;
	}
	// 2) ./tmp/minishell
	if (mkdir_p_2level("./tmp", "minishell") == 0)
	{
		snprintf(out_root, out_sz, "%s", "./tmp/minishell");
		return 0;
	}
	return -1;
}

static int make_run_dir(char *out_dir, size_t out_sz, const char *root)
{
	time_t now = time(NULL);
	struct tm tmv;
	localtime_r(&now, &tmv);

	char ts[32];
	strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tmv);

	// root/TS-PID
	snprintf(out_dir, out_sz, "%s/%s-%ld", root, ts, (long)getpid());
	if (mkdir(out_dir, 0755) != 0)
		return -1;
	return 0;
}

static int write_text_file(const char *path, const char *s)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return -1;
	ssize_t n = (ssize_t)strlen(s);
	if (write(fd, s, (size_t)n) != n)
	{
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

static void print_file_to_stdout(const char *path)
{
	FILE *fp = fopen(path, "r");
	if (!fp)
		return;
	char buf[4096];
	while (fgets(buf, sizeof(buf), fp))
		fputs(buf, stdout);
	fclose(fp);
}

static int wait_to_status(pid_t pid)
{
	int st = 0;
	if (waitpid(pid, &st, 0) < 0)
		return 1;

	if (WIFEXITED(st))
		return WEXITSTATUS(st);
	if (WIFSIGNALED(st))
		return 128 + WTERMSIG(st);
	return 1;
}

/*
 * strace を “外部コマンドとして” 実行する。
 * - traced 対象は: env -i PATH=/usr/bin:/bin minishell_path "<line>"
 * - -ff で PID ごとに trace_all.<pid> を吐かせる
 * - 最小 PID を root とみなして除外し、それ以外を trace.txt に合成
 */
static int run_strace(const char *dir, const char *minishell_path, const char *line, t_trace_mode mode)
{
	char trace_base[PATH_MAX + 64];
	char trace_txt[PATH_MAX + 64];
	char trace_tmp[PATH_MAX + 64];

	snprintf(trace_base, sizeof(trace_base), "%s/trace_all", dir);
	snprintf(trace_txt,  sizeof(trace_txt),  "%s/trace.txt", dir);
	snprintf(trace_tmp,  sizeof(trace_tmp),  "%s/trace.txt.tmp", dir);

	const char *pipe_trace =
		"trace=execve,clone,fork,vfork,wait4,waitid,exit_group,"
		"pipe,pipe2,dup2,openat,close,write";

	char *const argv_pipe[] = {
		(char *)STRACE_PATH,
		"-ff",
		"-qq",
		"-yy",
		"-tt",
		"-T",
		"-s", "128",
		"-e", (char *)pipe_trace,
		"-o", trace_base,
		(char *)ENV_PATH, "-i", "PATH=/usr/bin:/bin",
		(char *)minishell_path,
		(char *)line,
		NULL
	};

	char *const argv_all[] = {
		(char *)STRACE_PATH,
		"-ff",
		"-qq",
		"-yy",
		"-tt",
		"-T",
		"-s", "128",
		"-o", trace_base,
		(char *)ENV_PATH, "-i", "PATH=/usr/bin:/bin",
		(char *)minishell_path,
		(char *)line,
		NULL
	};

	char *const *argv = (mode == TRACE_PIPE) ? argv_pipe : argv_all;

	pid_t pid = fork();
	if (pid < 0)
		return 1;
	if (pid == 0)
	{
		execv(STRACE_PATH, (char *const *)argv);
		_exit(127);
	}

	int st = wait_to_status(pid);

	// --- trace_all.<pid> を列挙して root pid（最小 pid）を決める ---
	DIR *dp = opendir(dir);
	if (!dp)
		return (st != 0) ? st : 1;

	long root_pid = 0;

	struct dirent *de;
	while ((de = readdir(dp)) != NULL)
	{
		// "trace_all." から始まるファイルだけ
		const char *name = de->d_name;
		const char *prefix = "trace_all.";
		size_t plen = strlen(prefix);
		if (strncmp(name, prefix, plen) != 0)
			continue;

		const char *s = name + plen;
		if (!*s)
			continue;
		for (const char *t = s; *t; t++)
			if (!isdigit((unsigned char)*t))
				goto next_entry;

		long p = strtol(s, NULL, 10);
		if (p > 0 && (root_pid == 0 || p < root_pid))
			root_pid = p;

	next_entry:
		;
	}
	closedir(dp);

	if (root_pid <= 0)
		return (st != 0) ? st : 1;

	// --- root_pid 以外の trace_all.<pid> を trace.txt に合成（PIDプレフィックス付） ---
	dp = opendir(dir);
	if (!dp)
		return (st != 0) ? st : 1;

	FILE *out = fopen(trace_tmp, "w");
	if (!out)
	{
		closedir(dp);
		return (st != 0) ? st : 1;
	}

	char path[PATH_MAX + 128];
	char linebuf[8192];

	while ((de = readdir(dp)) != NULL)
	{
		const char *name = de->d_name;
		const char *prefix = "trace_all.";
		size_t plen = strlen(prefix);
		if (strncmp(name, prefix, plen) != 0)
			continue;

		const char *s = name + plen;
		if (!*s)
			continue;
		for (const char *t = s; *t; t++)
			if (!isdigit((unsigned char)*t))
				goto next_entry2;

		long p = strtol(s, NULL, 10);
		if (p <= 0 || p == root_pid)
			goto next_entry2;

		snprintf(path, sizeof(path), "%s/%s", dir, name);
		FILE *in = fopen(path, "r");
		if (!in)
			goto next_entry2;

		while (fgets(linebuf, sizeof(linebuf), in))
		{
			// 例: "13:17:00.762722 execve(...)" なので pid を前につけて揃える
			fprintf(out, "%ld %s", p, linebuf);
			if (linebuf[strlen(linebuf) - 1] != '\n')
				fputc('\n', out);
		}
		fclose(in);

	next_entry2:
		;
	}

	fclose(out);
	closedir(dp);

	(void)rename(trace_tmp, trace_txt);
	return st;
}

/*
 * focus 抽出:
 *   grep (include) trace.* | grep -v (exclude) > focus_pipe.txt
 *
 * “シェルスクリプト無し”でやるために、
 *   - pipe()
 *   - fork() x2
 *   - dup2()
 * で grep のパイプラインを minishell 自身が組み立てる。
 */
static int make_focus_pipe(const char *dir, t_trace_mode mode, char *out_path, size_t out_sz)
{
	char in_path[PATH_MAX];
	snprintf(in_path, sizeof(in_path), "%s/trace.txt", dir);

	snprintf(out_path, out_sz, "%s/focus_pipe.txt", dir);

	int infd = open(in_path, O_RDONLY);
	if (infd < 0)
	{
		// trace.txt が無いなら空を作って終わる
		int fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd >= 0) close(fd);
		return 0;
	}
	close(infd);

	// include: TRACE_ALL のときだけ使う（PIPE は strace 側で -e trace=... 済み）
	const char *inc_all =
		" (execve|pipe2?|dup2?|clone|fork|vfork|wait4|waitid|exit_group)\\("
		"| (openat|close|fcntl)\\("
		"| (read|write|readv|writev|pread64|pwrite64)\\("
		"| (chdir|getcwd)\\("
		"| (setpgid|setsid|tcsetpgrp|ioctl)\\("
		"| (sigaction|rt_sigaction|rt_sigprocmask|kill)\\(";

	const char *exc =
		"/etc/ld\\.so\\.cache|/usr/lib/|/lib/|locale-archive|/etc/locale|/etc/nsswitch\\.conf|/etc/passwd|/etc/group";

	int pfd[2];
	if (pipe(pfd) != 0)
		return -1;

	int outfd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (outfd < 0)
	{
		close(pfd[0]);
		close(pfd[1]);
		return -1;
	}

	// 子1: (TRACE_ALL) grep -E inc_all trace.txt  /  (TRACE_PIPE) cat trace.txt
	pid_t c1 = fork();
	if (c1 < 0)
		return -1;
	if (c1 == 0)
	{
		dup2(pfd[1], STDOUT_FILENO);
		close(pfd[0]);
		close(pfd[1]);
		close(outfd);

		if (mode == TRACE_ALL)
		{
			char *const argv[] = {(char *)GREP_PATH, "-E", (char *)inc_all, in_path, NULL};
			execv(GREP_PATH, argv);
			_exit(127);
		}
		else
		{
			// cat 相当（/bin/cat を呼ばずに、ここでは grep を使わず素直に exec したいなら /usr/bin/cat を許可してもOK）
			char *const argv[] = {(char *)GREP_PATH, ".*", in_path, NULL}; // “全行マッチ”で疑似cat
			execv(GREP_PATH, argv);
			_exit(127);
		}
	}

	// 子2: grep -v -E exc > focus
	pid_t c2 = fork();
	if (c2 < 0)
		return -1;
	if (c2 == 0)
	{
		dup2(pfd[0], STDIN_FILENO);
		dup2(outfd, STDOUT_FILENO);
		close(pfd[0]);
		close(pfd[1]);
		close(outfd);

		char *const argv[] = {(char *)GREP_PATH, "-v", "-E", (char *)exc, NULL};
		execv(GREP_PATH, argv);
		_exit(127);
	}

	close(pfd[0]);
	close(pfd[1]);
	close(outfd);

	int s1 = wait_to_status(c1);
	int s2 = wait_to_status(c2);
	if (s1 == 127 || s2 == 127)
		return -1;
	return 0;
}

int observe_run_traced(const char *minishell_path, const char *line, t_trace_mode mode)
{
	char root[PATH_MAX];
	if (ensure_log_root(root, sizeof(root)) != 0)
	{
		fprintf(stderr, "minishell(trace): cannot create ./logs/minishell or ./tmp/minishell\n");
		return 1;
	}

	char dir[PATH_MAX];
	if (make_run_dir(dir, sizeof(dir), root) != 0)
	{
		fprintf(stderr, "minishell(trace): cannot create run dir under %s\n", root);
		return 1;
	}

	// メタ情報を残す
	{
		char meta[PATH_MAX + 64];
		snprintf(meta, sizeof(meta), "%s/meta.txt", dir);

		char buf[4096];
		snprintf(buf, sizeof(buf),
			"minishell_path=%s\n"
			"line=%s\n"
			"mode=%s\n",
			minishell_path, line, (mode == TRACE_PIPE) ? "pipe" : "all");
		(void)write_text_file(meta, buf);
	}

	// 1) strace 実行
	int status = run_strace(dir, minishell_path, line, mode);

	// 2) focus 抽出
	char focus[PATH_MAX];
	if (make_focus_pipe(dir, mode, focus, sizeof(focus)) == 0)
	{
		printf("[trace] %s\n", focus);
		print_file_to_stdout(focus);
	}
	else
	{
		fprintf(stderr, "minishell(trace): focus extraction failed (dir=%s)\n", dir);
	}

	return status;
}
