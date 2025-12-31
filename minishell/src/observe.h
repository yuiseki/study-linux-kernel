#ifndef OBSERVE_H
#define OBSERVE_H

typedef enum e_trace_mode
{
	TRACE_PIPE = 0, // パイプ/リダイレクト中心にフォーカス（ノイズ除去あり）
	TRACE_ALL  = 1  // 広め（必要なら後で調整）
}	t_trace_mode;

/*
 * trace を “外部コマンドとしての strace/grep” で行う。
 * - minishell_path: 実行中の minishell 実体パス（例: "./minishell"）
 * - line: 実行したいコマンドライン（例: "echo hi | wc -c > /tmp/out"）
 * - mode: TRACE_PIPE / TRACE_ALL
 *
 * 戻り値:
 *   - traced な minishell（一発実行）の exit status を返す（通常のシェルと同じ）
 */
int observe_run_traced(const char *minishell_path, const char *line, t_trace_mode mode);

#endif
