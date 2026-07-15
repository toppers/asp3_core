/*
 *		移植検証テストのヘッダファイル
 *
 *  test_porting.c と test_porting.cfg の双方からインクルードされる．
 */

#ifndef TOPPERS_TEST_PORTING_CFG_H
#define TOPPERS_TEST_PORTING_CFG_H

#include <kernel.h>

/*
 *  ターゲット依存の定義（STACK_SIZE等）
 */
#include "target_test.h"

/*
 *  各タスクの優先度の定義
 */
#define MAIN_PRIORITY	10		/* メインタスクの優先度 */
#define HIGH_PRIORITY	9		/* 被験タスクの優先度（MAINより高） */

/*
 *  ターゲットに依存する可能性のある定数の定義
 */
#ifndef STACK_SIZE
#define	STACK_SIZE		4096	/* タスクのスタックサイズ */
#endif /* STACK_SIZE */

/*
 *  関数のプロトタイプ宣言
 */
#ifndef TOPPERS_MACRO_ONLY

extern void	main_task(EXINF exinf);
extern void	task2(EXINF exinf);
extern void	task3(EXINF exinf);
extern void	task4(EXINF exinf);
extern void	task5(EXINF exinf);
extern void	alarm1_handler(EXINF exinf);
extern void	alarm2_handler(EXINF exinf);

#endif /* TOPPERS_MACRO_ONLY */

#endif /* TOPPERS_TEST_PORTING_CFG_H */
