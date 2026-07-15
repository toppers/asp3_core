/*
 *		移植検証テスト（カーネル基本機能8項目・TAP形式）
 *
 *  新ターゲット移植時の最初の動作確認テスト．sample1（目視確認）・
 *  testexec全件（重い）の前段として，カーネル基本機能8項目を
 *  TAP形式で機械判定する．項目の並びは故障切り分けの順序になっている：
 *
 *    1. syslog_output          ブート〜UART（低レベル出力）の経路
 *    2. tick_timer_basic       高分解能タイマの歩進（割込み不要）
 *    3. task_create_activate   タスク起動とディスパッチャ
 *    4. semaphore_signal_wait  セマフォによるタスク間同期（カーネル本体）
 *    5. eventflag_set_wait     イベントフラグによるタスク間同期（同上）
 *    6. alarm_handler          アラームハンドラ＝タイマ割込みの経路
 *    7. isr_delayed_dispatch   割込みハンドラからの高優先度タスク起床
 *                              （割込み出口での遅延ディスパッチ経路）
 *    8. wake_from_idle         IDLE中の割込みによるタスク起床
 *                              （割込み出口でのidle復帰ディスパッチ経路）
 *
 *  2.までしか通らなければタイマ/ブート周り，5.まで通って6.で落ちれば
 *  タイマ割込み（割込みコントローラ設定）を疑う，という使い方をする．
 *
 *  項目2.と6.の待ちループは fch_hrt() による時間上限と繰返し回数上限の
 *  二重バウンドとし，タイマが停止していてもテストがハングしない．
 *
 *  項目7.と8.は，割込みハンドラの出口でディスパッチが必要になる2経路
 *  （CLIC搭載チップでは`mret`を経由しないと割込み優先度制御用のHW状態が
 *  固着し得る経路．docs/dev/porting-test.md参照）を意図的に踏む．項目6.
 *  までは実行中タスクへのmret復帰かタスクコンテキストからの自発的
 *  dispatch()のみで，この2経路を一度も通らない（構造的な検出漏れ）ため
 *  追加した：
 *   - 項目7．alarm2_handler（割込みコンテキスト）がsig_sem()で自分より
 *     高優先度のTASK5を起床させる＝「割込み出口での遅延ディスパッチ」
 *     （実行中の下位優先度タスクを保存し，割込みハンドラ側から見て
 *     ブロック中だった別タスクへ切り替える経路）を踏む．
 *   - 項目8．main_taskがdly_tsk()でIDLE状態を作り，その満了タイマ割込み
 *     がmain_task自身を起床させる＝「IDLEに割り込んだ割込みからの
 *     ディスパッチ復帰」を踏む．復帰後も歩進・割込み配送が続くことまで
 *     確認し，固着（全割込み永久ブロック）の非再発を機械判定する．
 */

#include <kernel.h>
#include <t_syslog.h>
#include "kernel_cfg.h"
#include "test_porting_cfg.h"
#include "tap.h"

/*
 *  待ちループの上限
 */
#define TICK_POLL_MAX	100000000UL		/* 項目2：歩進待ちの回数上限 */
#define ALM_TIMEOUT		1000000U		/* 項目6：アラーム待ち時間上限[μs] */
#define ALM_POLL_MAX	1000000000UL	/* 項目6：待ちの回数上限 */

#define ALM1_TIME		10000U			/* 項目6：アラーム時間[μs] */
#define ALM2_TIME		10000U			/* 項目7：アラーム時間[μs] */
#define DLY_TIME		10000U			/* 項目8：dly_tskの待ち時間[μs] */

/*
 *  被験タスク・ハンドラとの通信用フラグ
 */
static volatile bool_t	task2_ran;		/* 項目3 */
static volatile bool_t	task3_ran;		/* 項目4 */
static volatile bool_t	task4_ran;		/* 項目5 */
static volatile bool_t	alarm1_ran;		/* 項目6 */
static volatile bool_t	task5_ran;		/* 項目7 */

/*
 *  項目3：起動されたことを記録するだけのタスク
 */
void
task2(EXINF exinf)
{
	task2_ran = true;
	ext_tsk();
}

/*
 *  項目4：セマフォ待ち後に記録するタスク
 */
void
task3(EXINF exinf)
{
	if (wai_sem(SEM1) == E_OK) {
		task3_ran = true;
	}
	ext_tsk();
}

/*
 *  項目5：イベントフラグ待ち後に記録するタスク
 */
void
task4(EXINF exinf)
{
	FLGPTN	flgptn;

	if (wai_flg(FLG1, 0x01U, TWF_ORW, &flgptn) == E_OK) {
		task4_ran = true;
	}
	ext_tsk();
}

/*
 *  項目6：アラームハンドラ
 */
void
alarm1_handler(EXINF exinf)
{
	alarm1_ran = true;
}

/*
 *  項目7：セマフォ待ち後に記録するタスク（HIGH_PRIORITY．main_taskより
 *  高優先度）．main_taskが実行中の間にセマフォが返却され，割込みハンド
 *  ラ（alarm2_handler）の出口で本タスクへ遅延ディスパッチされることを
 *  期待する
 */
void
task5(EXINF exinf)
{
	if (wai_sem(SEM2) == E_OK) {
		task5_ran = true;
	}
	ext_tsk();
}

/*
 *  項目7：アラームハンドラ（割込みコンテキスト）からセマフォを返却する．
 *  ASP3の sig_sem() はタスク／非タスクいずれのコンテキストからも呼べ，
 *  非タスクコンテキストからの起床はrequest_dispatch_retint()による
 *  「割込み出口での遅延ディスパッチ」に帰着する（sense_context()参照）
 */
void
alarm2_handler(EXINF exinf)
{
	(void) sig_sem(SEM2);
}

/*
 *  メインタスク
 */
void
main_task(EXINF exinf)
{
	ER			ercd;
	HRTCNT		hrt1, hrt2, hrt0;
	bool_t		blocked;
	unsigned long	i;
	bool_t		tick_after_wake;

	tap_diag("test_porting: kernel porting verification");
	tap_plan(8U);

	/*
	 *  1. syslog_output：ここまで出力できていれば，ブートと低レベル
	 *     出力（UART等）の経路は通っている
	 */
	tap_ok(true, "syslog_output");

	/*
	 *  2. tick_timer_basic：高分解能タイマが歩進すること
	 *     （タイマ割込みは使わない＝カウンタの動作のみを見る）
	 */
	hrt1 = fch_hrt();
	hrt2 = hrt1;
	for (i = 0UL; i < TICK_POLL_MAX && hrt2 == hrt1; i++) {
		hrt2 = fch_hrt();
	}
	tap_ok(hrt2 != hrt1, "tick_timer_basic");

	/*
	 *  3. task_create_activate：高優先度タスクの起動で即座に
	 *     ディスパッチされ，制御が戻るまでに実行済みになること
	 */
	ercd = act_tsk(TASK2);
	tap_ok(ercd == E_OK && task2_ran, "task_create_activate");

	/*
	 *  4. semaphore_signal_wait：高優先度タスクがセマフォ待ちで
	 *     ブロックし，sig_semで待ち解除されること
	 */
	ercd = act_tsk(TASK3);
	blocked = (ercd == E_OK && !task3_ran);		/* 待ちに入ったこと */
	if (blocked) {
		ercd = sig_sem(SEM1);
	}
	tap_ok(blocked && ercd == E_OK && task3_ran, "semaphore_signal_wait");

	/*
	 *  5. eventflag_set_wait：同様にイベントフラグで待ち解除されること
	 */
	ercd = act_tsk(TASK4);
	blocked = (ercd == E_OK && !task4_ran);		/* 待ちに入ったこと */
	if (blocked) {
		ercd = set_flg(FLG1, 0x01U);
	}
	tap_ok(blocked && ercd == E_OK && task4_ran, "eventflag_set_wait");

	/*
	 *  6. alarm_handler：アラームハンドラが起動すること
	 *     （高分解能タイマ割込みの経路の確認．時間上限と回数上限の
	 *     二重バウンドの待ちループで，割込みが来なくてもハングしない）
	 */
	ercd = sta_alm(ALM1, ALM1_TIME);
	hrt0 = fch_hrt();
	for (i = 0UL;
		 i < ALM_POLL_MAX && !alarm1_ran
			&& (HRTCNT)(fch_hrt() - hrt0) < ALM_TIMEOUT;
		 i++) {
	}
	tap_ok(ercd == E_OK && alarm1_ran, "alarm_handler");

	/*
	 *  7. isr_delayed_dispatch：main_task（下位優先度）が実行中のまま
	 *     割込みハンドラ（alarm2_handler）が高優先度のTASK5を起床させる．
	 *     割込み出口はp_runtsk(main_task)≠p_schedtsk(TASK5)となり，
	 *     「割込み出口での遅延ディスパッチ」経路（TASK5はwai_semで待って
	 *     いたため，切替先の再開番地はmretを経由しないdispatch_r）を通る．
	 *     この経路がmretを経ずに固着すると，以後のアラーム割込み（後続
	 *     項目・testexec等）が配送されなくなる
	 */
	ercd = act_tsk(TASK5);
	blocked = (ercd == E_OK);
	if (blocked) {
		ercd = sta_alm(ALM2, ALM2_TIME);
	}
	hrt0 = fch_hrt();
	for (i = 0UL;
		 i < ALM_POLL_MAX && !task5_ran
			&& (HRTCNT)(fch_hrt() - hrt0) < ALM_TIMEOUT;
		 i++) {
	}
	tap_ok(blocked && ercd == E_OK && task5_ran, "isr_delayed_dispatch");

	/*
	 *  8. wake_from_idle：この時点でTASK2〜5は全てDORMANT（他に実行可能
	 *     タスクなし）のため，main_taskがdly_tsk()で待ちに入るとCPUは
	 *     真にIDLE状態（p_runtsk==NULL）になる．dly_tskの満了タイマ割込み
	 *     がmain_task自身を起床させる＝「IDLEに割り込んだ割込みからの
	 *     ディスパッチ復帰」経路（wifi_scan実機で観測された72ms凍結と
	 *     同型）を通る．復帰後も高分解能タイマの歩進が続くことまで確認し，
	 *     固着（全割込み永久ブロック）の非再発を機械判定する
	 */
	ercd = dly_tsk(DLY_TIME);
	hrt1 = fch_hrt();
	hrt2 = hrt1;
	for (i = 0UL; i < TICK_POLL_MAX && hrt2 == hrt1; i++) {
		hrt2 = fch_hrt();
	}
	tick_after_wake = (hrt2 != hrt1);
	tap_ok(ercd == E_OK && tick_after_wake, "wake_from_idle");

	tap_done();
}
