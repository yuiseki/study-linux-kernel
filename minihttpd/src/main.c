#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LISTEN_PORT 8080
#define BACKLOG 10
#define READ_BUF_SIZE 4096

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

int main(void)
{
	int listen_fd;
	int ret;

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
