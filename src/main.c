#include <stdio.h>
#include <string.h>

// プロトタイプ宣言
void keyboard();
void mouse();


/**
 * @fn
 * @brief 使い方表示
 *
 * @param[in] exe_name 実行可能ファイルのファイル名
 */
void usage(char *exe_name) {
	fprintf(stderr, "Usage: %s [keyboard|mouse] < /dev/input/eventN > /dev/hidgN\n", exe_name);
}


/**
 * @fn
 * @brief メイン処理
 *
 * @param[in] コマンドライン引数の数
 * @param[in] コマンドライン引数
 *
 * @return 0:正常終了、1:エラー終了
 */
int main(int argc, char *argv[]) {
	if (argc != 2) {
		usage(argv[0]);
		return 1;
	}

	int keyboard_flag = strncmp(argv[1], "keyboard", strlen("keyboard"));
       	int mouse_flag = strncmp(argv[1], "mouse", strlen("mouse"));

	if ((keyboard_flag != 0) && (mouse_flag != 0)) {
		usage(argv[0]);
		return 1;
	}

	if (keyboard_flag == 0) {
		keyboard();
	} else {
		mouse();
	}

	return 0;
}
