/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，本ソフトウェアをTOPPERSライセンス（条件は他のソー
 *  スファイルの先頭コメントを参照）の下で利用することを許諾する．本ソ
 *  フトウェアは無保証で提供される．
 */

/*
 *    カーネルのチップ依存部（ESP32-C3用）
 *
 *  RP2350 RISC-V依存部（Xh3irq）を割込みマトリクス（INTMTX）用に
 *  置き換えたもの．高分解能タイマはSYSTIMER（ターゲット依存部の
 *  target_timer.[ch]）を使用するため，本ファイルではタイマの初期化を
 *  行わない．
 */

#include "kernel_impl.h"
#include "interrupt.h"
#include <sil.h>

/*
 *  各CPU割込み線に割り当てたソースのビットマスクとFROM_CPUソース番号
 *  （esp32c3_intmtx_routeが更新する）
 */
uint32_t intmtx_srcmask[32][2];
uint8_t intmtx_from_cpu[32];

/*
 *  ペリフェラル割込みソースのCPU割込み線への割り当て
 */
void
esp32c3_intmtx_route(uint_t intsrc, INTNO intno)
{
	sil_wrw_mem((void *)(INTMTX_BASE + intsrc * 4U), (uint32_t)intno);
	intmtx_srcmask[intno][intsrc / 32U] |= 1U << (intsrc % 32U);
	if (ESP32C3_INTSRC_FROM_CPU_0 <= intsrc
			&& intsrc <= ESP32C3_INTSRC_FROM_CPU_3) {
		intmtx_from_cpu[intno] = (uint8_t)(intsrc
										- ESP32C3_INTSRC_FROM_CPU_0);
	}
}

/*
 *  割込みマトリクスの初期化
 */
void
intmtx_initialize(void)
{
	uint_t i;

	/*
	 *  全ペリフェラルソースの割り当てを解除する（CPU割込み線0へ
	 *  マップ＝線0は許可しないため実質無効）．ROMブートローダが
	 *  設定した割り当てをクリアする意味もある．
	 */
	for (i = 0U; i < ESP32C3_TNUM_INTSRC; i++) {
		sil_wrw_mem((void *)(INTMTX_BASE + i * 4U), 0U);
	}

	/*
	 *  ソース割り当ての管理テーブルの初期化
	 */
	for (i = 0U; i < 32U; i++) {
		intmtx_srcmask[i][0] = 0U;
		intmtx_srcmask[i][1] = 0U;
		intmtx_from_cpu[i] = 0xFFU;
	}

	/*
	 *  全CPU割込み線の禁止・タイプをlevelに設定・edgeペンディングの
	 *  クリア
	 */
	sil_wrw_mem((void *)(INTMTX_BASE + INTMTX_ENABLE_OFF), 0U);
	sil_wrw_mem((void *)(INTMTX_BASE + INTMTX_TYPE_OFF), 0U);
	sil_wrw_mem((void *)(INTMTX_BASE + INTMTX_CLEAR_OFF), ~0U);
	sil_wrw_mem((void *)(INTMTX_BASE + INTMTX_CLEAR_OFF), 0U);

	/*
	 *  すべてのCPU割込み線を優先度0（決して受け付けられない）に設定
	 */
	for (i = 0U; i <= INTMTX_TNUM_INT; i++) {
		sil_wrw_mem((void *)(INTMTX_BASE + INTMTX_PRI_BASEOFF + i * 4U), 0U);
	}

	/*
	 *  割込み優先度マスクを全解除（内部表現0＝THRESH 1）に設定
	 */
	intmtx_set_thresh(0U);
}

/*
 *  チップ依存の初期化
 */
void
chip_initialize(void)
{
	extern void *trap_vector_table;

	/*
	 *  Machine Trap-Vector Base の設定（VECTOREDモード．
	 *  mcause&0x1fがCPU割込み線番号としてテーブルを索引する）
	 */
	riscv_write_mtvec((ulong_t)&trap_vector_table | MTVEC_MODE_VECTORD);

	/*
	 *  割込みマトリクスの初期化
	 */
	intmtx_initialize();

	/*
	 *  mieのCPU割込み線ビット（1〜31）をすべて許可する．
	 *
	 *  実機のESP32-C3では割込みの許可・優先度制御は割込みマトリクス
	 *  側で行われmieは実質使用されない（ESP-IDFもmieを設定しない）が，
	 *  QEMUのesp32c3モデルはRISC-V標準のmip/mie経由で割込みを届ける
	 *  ため，mieのビットが立っていないと割込みが配送されない．
	 *  個別の許可・マスクはINTMTX（ENABLE／THRESH）で行うため，mieは
	 *  常時全許可でよい．
	 */
	Asm("csrw mie, %0" : : "r"(~0U));

	/*
	 *  コア依存の初期化
	 */
	core_initialize();
}

/*
 *  チップ依存の終了処理
 */
void
chip_terminate(void)
{
	/*
	 *  コア依存の終了処理
	 */
	core_terminate();
}

/*
 *  割込み要求ラインの属性の設定
 */
Inline void
intmtx_config_int(INTNO intno, ATR intatr, PRI intpri)
{
	assert(VALID_INTNO(intno));
	assert(TMIN_INTPRI <= intpri && intpri <= TMAX_INTPRI);

	/*
	 *  割込み優先度を設定
	 */
	intmtx_set_priority(intno, INT_IPM(intpri));

	/*
	 *  割込みを許可
	 */
	if ((intatr & TA_ENAINT) != 0U) {
		intmtx_enable_int(intno);
	}
}

/*
 *  割込み管理機能の初期化
 */
void
initialize_interrupt(void)
{
	uint_t			i;
	const INTINIB	*p_intinib;

	for (i = 0U; i < tnum_cfg_intno; i++) {
		p_intinib = &(intinib_table[i]);
		intmtx_config_int(p_intinib->intno, p_intinib->intatr,
						  p_intinib->intpri);
	}
}
