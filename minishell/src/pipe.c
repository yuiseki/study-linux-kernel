#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int redir_stdout_trunc(const char *path);

static int	wait_all(pid_t *pids, int n)
{
	int	status;
	int	last_status = 0;
	int	i;

	for (i = 0; i < n; i++)
	{
		if (waitpid(pids[i], &status, 0) < 0)
			continue;
		last_status = status;
	}
	if (WIFEXITED(last_status))
		return WEXITSTATUS(last_status);
	if (WIFSIGNALED(last_status))
		return 128 + WTERMSIG(last_status);
	return 1;
}

static void	die_perror(const char *msg)
{
	perror(msg);
	exit(1);
}

int	exec_pipeline_redir(char ***argvv, int n, const char *out_path)
{
	int		prev_read = -1;
	pid_t	*pids;
	int		i;

	if (!argvv || n <= 0)
		return 0;

	pids = calloc((size_t)n, sizeof(pid_t));
	if (!pids)
		die_perror("calloc");

	for (i = 0; i < n; i++)
	{
		int next_pipe[2] = {-1, -1};
		int is_last = (i == n - 1);

		if (!is_last)
		{
			if (pipe(next_pipe) < 0)
				die_perror("pipe");
		}

		pids[i] = fork();
		if (pids[i] < 0)
			die_perror("fork");

		if (pids[i] == 0)
		{
			// 子プロセス

			// stdin <- prev_read（最初以外）
			if (prev_read != -1)
			{
				if (dup2(prev_read, STDIN_FILENO) < 0)
					_exit(1);
			}

			// stdout -> next_pipe[1]（最後以外）
			if (!is_last)
			{
				if (dup2(next_pipe[1], STDOUT_FILENO) < 0)
					_exit(1);
			}
			else
			{
				// 最後のコマンドだけリダイレクト適用
				if (out_path)
				{
					if (redir_stdout_trunc(out_path) < 0)
						_exit(1);
				}
			}

			// 使わないfdを全部閉じる
			if (prev_read != -1)
				close(prev_read);
			if (!is_last)
			{
				close(next_pipe[0]);
				close(next_pipe[1]);
			}

			execvp(argvv[i][0], argvv[i]);
			fprintf(stderr, "minishell: exec failed: %s\n", argvv[i][0]);
			_exit(127);
		}

		// 親プロセス：次の段に向けてFDを更新
		if (prev_read != -1)
			close(prev_read);

		if (!is_last)
		{
			close(next_pipe[1]);      // 親は書き端不要
			prev_read = next_pipe[0]; // 次の段の stdin になる
		}
		else
		{
			prev_read = -1;
		}
	}

	if (prev_read != -1)
		close(prev_read);

	{
		int code = wait_all(pids, n);
		free(pids);
		return code;
	}
}

int	exec_pipeline(char ***argvv, int n)
{
	return exec_pipeline_redir(argvv, n, NULL);
}
