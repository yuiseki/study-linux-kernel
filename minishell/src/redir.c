#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// stdout を file にリダイレクトする（O_TRUNC）
int	redir_stdout_trunc(const char *path)
{
	int	fd;

	if (!path || !*path)
		return (-1);

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0)
		return (-1);

	if (dup2(fd, STDOUT_FILENO) < 0)
	{
		close(fd);
		return (-1);
	}
	close(fd);
	return (0);
}
