#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

int redir_stdout_trunc(const char *path);

static void	die_perror(const char *msg)
{
	perror(msg);
	exit(1);
}

int exec_argv_redir(char *const argv[], const char *out_path)
{
	pid_t	pid;
	int		status = 0;

	if (!argv || !argv[0])
		return 0;

	pid = fork();
	if (pid < 0)
		die_perror("fork");

	if (pid == 0)
	{
		// 子でだけ stdout リダイレクト（親のstdoutを汚さない）
		if (out_path)
		{
			if (redir_stdout_trunc(out_path) < 0)
				_exit(1);
		}

		// 子プロセス：argv[0] をPATH解決して実行
		execvp(argv[0], (char *const *)argv);
		// exec 失敗時のみここに来る
		fprintf(stderr, "minishell: exec failed: %s\n", argv[0]);
		_exit(127);
	}

	// 親：子の終了を待つ
	if (waitpid(pid, &status, 0) < 0)
		return 1;

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);
	return 1;
}

int exec_argv(char *const argv[])
{
	return exec_argv_redir(argv, NULL);
}
