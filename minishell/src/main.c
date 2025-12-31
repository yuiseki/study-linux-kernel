#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // isatty, STDIN_FILENO

#include "observe.h"

/*
 * 外部実装（すでに別ファイルに存在する想定）
 *
 * - exec_argv_redir:
 *     argv で与えた単発コマンドを実行する。
 *     out_path が非NULLなら、stdout を out_path にリダイレクトして実行する。
 *
 * - exec_pipeline_redir:
 *     argvv[0..n-1]（各要素は argv 配列）からなるパイプラインを実行する。
 *     out_path が非NULLなら、パイプライン全体の「最後のコマンドの stdout」を out_path にリダイレクトする。
 */
int exec_argv(char *const argv[]);
int exec_argv_redir(char *const argv[], const char *out_path);

int exec_pipeline(char ***argvv, int n);
int exec_pipeline_redir(char ***argvv, int n, const char *out_path);

/*
 * split_spaces() の返り値：
 * - argv: exec に渡す argv 配列（NULL終端）
 * - buf : argv の各要素が指す「元のバッファ先頭」。free すべき領域。
 *
 * 重要:
 *  - argv[i] は buf の内部（ヌル終端に置換して分割）を指すだけで、個別に free しない。
 *  - free するのは buf と argv 配列そのものだけ。
 */
typedef struct s_words
{
	char	**argv;  // exec に渡す配列（buf 内を指す）
	char	*buf;    // free すべき strdup の先頭
}	t_words;

/*
 * 超雑：空白（space/tab）区切りのみのトークナイズ。
 * - クォート、エスケープ、変数展開、連結、リダイレクト演算子などは未対応。
 * - 学習用の最小実装として「argv を作る」ことにだけ集中している。
 */
static t_words	split_spaces(const char *s)
{
	t_words	w = (t_words){0};
	size_t	i = 0, n = 0;
	char	*buf;
	char	**argv;

	if (!s)
		return w;

	buf = strdup(s);
	if (!buf)
		return w;

	// 単語数を数える（連続空白は飛ばす）
	for (i = 0; buf[i];)
	{
		while (buf[i] == ' ' || buf[i] == '\t')
			i++;
		if (!buf[i])
			break;
		n++;
		while (buf[i] && buf[i] != ' ' && buf[i] != '\t')
			i++;
	}

	argv = calloc(n + 1, sizeof(char *));
	if (!argv)
	{
		free(buf);
		return (t_words){0};
	}

	// 分割して argv に詰める（buf を破壊的に利用）
	size_t ai = 0;
	for (i = 0; buf[i];)
	{
		while (buf[i] == ' ' || buf[i] == '\t')
			buf[i++] = '\0';
		if (!buf[i])
			break;
		argv[ai++] = &buf[i];
		while (buf[i] && buf[i] != ' ' && buf[i] != '\t')
			i++;
	}

	argv[ai] = NULL;
	w.argv = argv;
	w.buf = buf;
	return w;
}

/*
 * split_spaces の後始末。
 * argv[i] は buf 内を指すだけなので、free するのは buf と argv だけ。
 */
static void	free_words(t_words w)
{
	free(w.buf);
	free(w.argv);
}

/*
 * 末尾の改行を落とす（getline 用）
 * - "\n" または "\r\n" を想定
 */
static void	chomp_newline(char *s)
{
	size_t len;

	if (!s)
		return;
	len = strlen(s);
	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
	{
		s[len - 1] = '\0';
		len--;
	}
}

/*
 * 空行判定（space/tab のみも空とみなす）
 */
static int	is_blank_line(const char *s)
{
	if (!s)
		return 1;
	while (*s)
	{
		if (*s != ' ' && *s != '\t')
			return 0;
		s++;
	}
	return 1;
}

/*
 * 入力 1 行を「実行できる形」にして実行する。
 *
 * やっていること（高レベル）:
 *  1) 入力を strdup して「破壊的に」パースできるようにする
 *  2) 行末の「> file」だけを抜き取る（この minishell の仕様）
 *  3) '|' で分割して各コマンド断片を split_spaces
 *  4) 単発なら exec_argv_redir、パイプラインなら exec_pipeline_redir
 *
 * 返り値:
 *  - 実行結果の exit status（exec_* が返す code）
 */
static int	run_command_line(const char *input)
{
	char	*line;
	int		code = 0;

	if (!input || is_blank_line(input))
		return 0;

	line = strdup(input);
	if (!line)
	{
		fprintf(stderr, "minishell: out of memory\n");
		return 1;
	}

	/*
	 * 方針: 「最後だけリダイレクト」
	 * - 行末の '>' だけを見る（strrchr で最後の '>' を取る）
	 * - ここは学習を進めるための割り切り仕様
	 */
	char *out_path = NULL;
	char *gt = strrchr(line, '>');
	if (gt)
	{
		*gt = '\0';   // コマンド部分をここで切る
		gt++;
		while (*gt == ' ' || *gt == '\t')
			gt++;

		if (!*gt)
		{
			fprintf(stderr, "minishell: redirection: missing file\n");
			free(line);
			return 2;
		}

		out_path = gt; // out_path は line の内部を指す
		while (*gt && *gt != ' ' && *gt != '\t')
			gt++;
		*gt = '\0';    // ファイル名を終端
	}

	// '|' の数を数える
	int pipes = 0;
	for (int i = 0; line[i]; i++)
		if (line[i] == '|')
			pipes++;
	int ncmd = pipes + 1;

	// コマンド配列（各要素は split_spaces の結果）
	char ***argvv = calloc((size_t)ncmd, sizeof(char **));
	t_words *words = calloc((size_t)ncmd, sizeof(t_words));
	if (!argvv || !words)
	{
		free(argvv);
		free(words);
		free(line);
		fprintf(stderr, "minishell: out of memory\n");
		return 1;
	}

	/*
	 * line を '|' で分割して各セグメントを split
	 * - line はすでに strdup 済みなので、破壊してOK
	 */
	int idx = 0;
	char *seg = line;
	for (int i = 0; ; i++)
	{
		if (line[i] == '|' || line[i] == '\0')
		{
			char saved = line[i];
			line[i] = '\0';

			words[idx] = split_spaces(seg);
			if (!words[idx].argv || !words[idx].argv[0])
			{
				// 空コマンドはエラー扱い
				fprintf(stderr, "minishell: parse error near '|'\n");
				code = 2;
				line[i] = saved;
				break;
			}
			argvv[idx] = words[idx].argv;
			idx++;

			if (saved == '\0')
				break;

			seg = &line[i + 1];
			line[i] = saved;
		}
	}

	if (code == 0)
	{
		if (ncmd == 1)
		{
			// 単発コマンドでも「最後だけ >」を適用する
			code = exec_argv_redir(argvv[0], out_path);
		}
		else
		{
			// パイプライン全体の「最後だけ >」を適用する
			code = exec_pipeline_redir(argvv, ncmd, out_path);
		}
	}

	// 後始末
	for (int i = 0; i < idx; i++)
		free_words(words[i]);
	free(argvv);
	free(words);
	free(line);
	return code;
}

/*
 * REPL builtin:
 *   :trace on|off
 *   :trace pipe|all
 *   :trace        (status表示)
 */
static int	handle_repl_builtin(const char *line, int *trace_enabled, t_trace_mode *mode)
{
	if (strncmp(line, ":trace", 6) != 0)
		return 0;

	const char *p = line + 6;
	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '\0')
	{
		printf("trace: %s (%s)\n",
			(*trace_enabled ? "on" : "off"),
			(*mode == TRACE_PIPE ? "pipe" : "all"));
		return 1;
	}

	if (strcmp(p, "on") == 0)
	{
		*trace_enabled = 1;
		printf("trace: on\n");
		return 1;
	}
	if (strcmp(p, "off") == 0)
	{
		*trace_enabled = 0;
		printf("trace: off\n");
		return 1;
	}
	if (strcmp(p, "pipe") == 0)
	{
		*mode = TRACE_PIPE;
		printf("trace mode: pipe\n");
		return 1;
	}
	if (strcmp(p, "all") == 0)
	{
		*mode = TRACE_ALL;
		printf("trace mode: all\n");
		return 1;
	}

	fprintf(stderr, "usage: :trace [on|off|pipe|all]\n");
	return 1;
}

/*
 * 対話モード（REPL）
 * - ./minishell    -> REPL
 * - ./minishell "..." -> 一発実行
 *
 * 仕様（いまは最小）:
 * - 空行は無視
 * - "exit" または "quit" で終了
 * - TTY のときだけプロンプトを出す
 *
 * 返り値:
 * - 最後に実行したコマンドの exit status（習慣的にそうする）
 */
static int	repl_loop(const char *argv0)
{
	char	*line = NULL;
	size_t	cap = 0;
	int		last_status = 0;
	int		interactive = isatty(STDIN_FILENO);

	int trace_enabled = 0;
	t_trace_mode mode = TRACE_PIPE;

	while (1)
	{
		if (interactive)
		{
			// プロンプトは stdout に出す（一般的なシェル挙動）
			printf("minishell$ ");
			fflush(stdout);
		}

		ssize_t nread = getline(&line, &cap, stdin);
		if (nread < 0)
			break;

		chomp_newline(line);
		if (is_blank_line(line))
			continue;

		// 最小 builtin（学習用の割り切り）
		if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)
			break;

		if (handle_repl_builtin(line, &trace_enabled, &mode))
			continue;

		if (!trace_enabled)
		{
			last_status = run_command_line(line);
		}
		else
		{
			// trace on のときだけ "一発実行 minishell" を strace で包む（スクリプト無し）
			last_status = observe_run_traced(argv0, line, mode);
		}
	}

	free(line);
	return last_status;
}

int	main(int argc, char **argv)
{
	/*
	 * 使い分け:
	 * - argc == 1: REPL
	 * - argc == 2: 一発実行（観測ツールから呼ぶのにも便利）
	 */
	if (argc == 1)
		return repl_loop(argv[0]);

	if (argc == 2)
		return run_command_line(argv[1]);

	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s            # REPL\n", argv[0]);
	fprintf(stderr, "  %s '<line>'   # run once\n", argv[0]);
	return 2;
}
