#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LISTEN_PORT 8080
#define BACKLOG 10
#define READ_BUF_SIZE 4096
#define STRACE_PATH "/usr/bin/strace"
#define ENV_PATH    "/usr/bin/env"

static int setup_listen_socket(void)
{
	int fd;
	int yes = 1;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		perror("socket");
		return -1;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
	{
		perror("setsockopt");
		close(fd);
		return -1;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(LISTEN_PORT);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		close(fd);
		return -1;
	}
	if (listen(fd, BACKLOG) < 0)
	{
		perror("listen");
		close(fd);
		return -1;
	}
	return fd;
}

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
	if (mkdir_p_2level("./logs", "minihttpd") == 0)
	{
		snprintf(out_root, out_sz, "%s", "./logs/minihttpd");
		return 0;
	}
	if (mkdir_p_2level("./tmp", "minihttpd") == 0)
	{
		snprintf(out_root, out_sz, "%s", "./tmp/minihttpd");
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

	snprintf(out_dir, out_sz, "%s/%s-%ld", root, ts, (long)getpid());
	if (mkdir(out_dir, 0755) != 0)
		return -1;
	return 0;
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

static int run_traced(const char *self_path)
{
	char root[PATH_MAX];
	char dir[PATH_MAX];
	char trace_txt[PATH_MAX + 64];

	if (ensure_log_root(root, sizeof(root)) != 0)
	{
		fprintf(stderr, "minihttpd(trace): cannot create ./logs/minihttpd or ./tmp/minihttpd\n");
		return 1;
	}
	if (make_run_dir(dir, sizeof(dir), root) != 0)
	{
		fprintf(stderr, "minihttpd(trace): cannot create run dir under %s\n", root);
		return 1;
	}

	snprintf(trace_txt, sizeof(trace_txt), "%s/trace.txt", dir);

	const char *trace_set =
		"trace=socket,bind,listen,accept,accept4,read,write,close,fcntl";

	char *const argv[] = {
		(char *)STRACE_PATH,
		"-qq",
		"-yy",
		"-tt",
		"-T",
		"-s", "128",
		"-e", (char *)trace_set,
		(char *)ENV_PATH, "-i", "PATH=/usr/bin:/bin",
		(char *)self_path,
		(char *)"--no-trace",
		NULL
	};

	int pfd[2];
	if (pipe(pfd) != 0)
		return 1;

	pid_t pid = fork();
	if (pid < 0)
		return 1;
	if (pid == 0)
	{
		dup2(pfd[1], STDERR_FILENO);
		close(pfd[0]);
		close(pfd[1]);
		execv(STRACE_PATH, argv);
		_exit(127);
	}

	close(pfd[1]);
	FILE *out = fopen(trace_txt, "w");
	if (!out)
	{
		close(pfd[0]);
		return 1;
	}

	char buf[4096];
	ssize_t n;
	while ((n = read(pfd[0], buf, sizeof(buf))) > 0)
	{
		fwrite(buf, 1, (size_t)n, out);
		fwrite(buf, 1, (size_t)n, stderr);
	}
	fclose(out);
	close(pfd[0]);

	return wait_to_status(pid);
}

static int serve_once(int listen_fd)
{
	int client_fd;
	char buf[READ_BUF_SIZE];
	const char *body = "hello, world!\n";
	char resp[512];
	int body_len;
	int resp_len;
	ssize_t n;

	client_fd = accept(listen_fd, NULL, NULL);
	if (client_fd < 0)
	{
		perror("accept");
		return -1;
	}

	n = read(client_fd, buf, sizeof(buf) - 1);
	if (n < 0)
		perror("read");
	else if (n >= 0)
		buf[n] = '\0';

	body_len = (int)strlen(body);
	resp_len = snprintf(resp, sizeof(resp),
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"\r\n"
		"%s",
		body_len, body);

	if (resp_len < 0)
		resp_len = 0;
	if (write(client_fd, resp, (size_t)resp_len) < 0)
		perror("write");

	close(client_fd);
	return 0;
}

int main(int argc, char **argv)
{
	int listen_fd;
	int ret;
	int do_trace = 0;
	int no_trace = 0;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--trace") == 0)
			do_trace = 1;
		else if (strcmp(argv[i], "--no-trace") == 0)
			no_trace = 1;
	}
	if (do_trace && !no_trace)
		return run_traced(argv[0]);

	listen_fd = setup_listen_socket();
	if (listen_fd < 0)
		return 1;

	fprintf(stderr, "minihttpd: listening on http://localhost:%d\n", LISTEN_PORT);
	while (1)
	{
		ret = serve_once(listen_fd);
		if (ret < 0)
			break;
	}
	close(listen_fd);
	return (ret < 0);
}
