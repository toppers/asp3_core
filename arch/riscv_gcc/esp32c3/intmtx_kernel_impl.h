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
 *     カーネルの割込みコントローラ依存部（ESP32-C3 割込みマトリクス用）
 *
 *  このヘッダファイルは，target_kernel_impl.h（または，そこからインク
 *  ルードされるファイル）のみからインクルードされる．他のファイルから
 *  直接インクルードしてはならない．
 *
 *  ESP32-C3の割込みコントローラ（INTC・割込みマトリクス）は，62本の
 *  ペリフェラル割込みソースを31本のCPU割込み線（1〜31）へMAPレジスタ
 *  で動的に割り当てる方式．CPU割込み線ごとに許可・優先度・タイプ
 *  （level/edge）を持ち，メモリマップトレジスタで制御する
 *  （Xh3irq（カスタムCSR）・PLIC（claim/complete）に続く3種目の流儀）．
 *
 *  ■ 割込み番号の対応
 *  ASP3の割込み番号INTNO = CPU割込み線番号（1〜31）とする．
 *  CPU割込み線0は使用しない（INTNO=0は割込み入口処理のSpurious番兵）．
 *  vectoredモードのmtvecにより mcause&0x1f がそのままCPU割込み線番号
 *  になる．ペリフェラルソース→CPU割込み線の割り当て（ルーティング）
 *  はターゲット依存部が esp32c3_intmtx_route() で静的に行う．
 *
 *  ■ 割込み優先度
 *  CPU割込み線の優先度フィールドは物理4ビットだが，有効範囲は1〜7
 *  （ESP-IDF riscv/interrupt.hの規定．大きいほど高優先度）．ASP3の
 *  内部表現（INT_IPM：1〜7）をそのまま対応付ける．割込み優先度マスク
 *  はTHRESHレジスタ（優先度がTHRESH未満の割込みをマスク）で実現し，
 *  内部表現mのとき THRESH = m + 1 とする（優先度 <= m をブロック）．
 *  優先度0の割込み線は決して受け付けられない（THRESHの最小値が1）．
 *
 *  ■ 割込みのネスト
 *  ESP32-C3はハードウェアによる優先度の自動昇格を持たないため，
 *  割込み入口（chip_support.SのIRC操作）でTHRESHを「受け付けた割込み
 *  の優先度+1」へソフトウェアで昇格し，出口で復元する．
 *
 *  参照：ESP32-C3 TRM §8（Interrupt Matrix）／
 *        ref-esp32c3/HW_NOTES.md（レジスタ早見表）
 */

#ifndef TOPPERS_INTMTX_KERNEL_IMPL_H
#define TOPPERS_INTMTX_KERNEL_IMPL_H

#include <sil.h>
#include "esp32c3.h"

/*
 *  割込みマトリクスのレジスタ（アセンブリ言語からも参照する）
 *
 *  ソースnのMAPレジスタ：INTMTX_BASE + 4n（値＝CPU割込み線番号）
 *  CPU割込み線nの優先度：INTMTX_PRI_BASEOFF + 4n
 */
#define INTMTX_BASE           ESP32C3_INTMTX_BASE
#define INTMTX_STATUS0_OFF    0x0F8   /* ソース0〜31の生ステータス（RO） */
#define INTMTX_STATUS1_OFF    0x0FC   /* ソース32〜61の生ステータス（RO） */
#define INTMTX_ENABLE_OFF     0x104   /* CPU割込み線の許可（bit0〜31） */
#define INTMTX_TYPE_OFF       0x108   /* タイプ（0=level／1=edge） */
#define INTMTX_CLEAR_OFF      0x10C   /* edge型のペンディングクリア */
#define INTMTX_EIP_OFF        0x110   /* CPUへ到達している割込み線（RO） */
#define INTMTX_PRI_BASEOFF    0x114   /* 優先度（4bit×32本・4バイト毎） */
#define INTMTX_THRESH_OFF     0x194   /* 受付け閾値（未満をマスク） */

/*
 *  CPU割込み線の本数（1〜31を使用）
 */
#define INTMTX_TNUM_INT    UINT_C(31)

/*
 *  RISC-Vコアで共通な定義
 */
#include "core_kernel_impl.h"

#ifndef TOPPERS_MACRO_ONLY

/*
 *  割込みマトリクスの操作（intnoはASP3の割込み番号＝CPU割込み線番号）
 */

/*
 *  割込み禁止
 */
Inline void
intmtx_disable_int(INTNO intno)
{
	sil_clrw((void *)(INTMTX_BASE + INTMTX_ENABLE_OFF), 1U << intno);
}

/*
 *  割込み許可
 */
Inline void
intmtx_enable_int(INTNO intno)
{
	sil_orw((void *)(INTMTX_BASE + INTMTX_ENABLE_OFF), 1U << intno);
}

/*
 *  各CPU割込み線に割り当てたペリフェラルソースのビットマスク
 *  （INTR_STATUS_0／STATUS_1に対応する2ワード．esp32c3_intmtx_routeが
 *  更新する．prb_intの実現に用いる）
 */
extern uint32_t intmtx_srcmask[32][2];

/*
 *  各CPU割込み線に割り当てたFROM_CPUソースの番号（0〜3．割り当てが
 *  ないときは0xFF．ras_int／clr_intの実現に用いる）
 */
extern uint8_t intmtx_from_cpu[32];

/*
 *  割込みペンディングのチェック
 *
 *  EIPレジスタは許可・閾値通過後の状態しか示さない（禁止中・マスク中
 *  の割込み要求を観測できない）ため，割込み線に割り当てたソースの
 *  生ステータス（INTR_STATUS_0/1）を読む．FROM_CPUソースを割り当てた
 *  線は，FROM_CPUレジスタの読み返しでも判定する（QEMUのesp32c3モデル
 *  はINTR_STATUSレジスタを実装していないため，こちらが実効的な判定に
 *  なる）．
 */
Inline bool_t
intmtx_probe_int(INTNO intno)
{
	if ((intmtx_from_cpu[intno] != 0xFFU)
		&& (sil_rew_mem((void *)(ESP32C3_SYSTEM_CPU_INTR_FROM_CPU_0
				+ (uint32_t)intmtx_from_cpu[intno] * 4U)) != 0U)) {
		return(true);
	}
	return(((sil_rew_mem((void *)(INTMTX_BASE + INTMTX_STATUS0_OFF))
				& intmtx_srcmask[intno][0])
			| (sil_rew_mem((void *)(INTMTX_BASE + INTMTX_STATUS1_OFF))
				& intmtx_srcmask[intno][1])) != 0U);
}

/*
 *  割込みの要求（ras_int用．FROM_CPUソースが割り当てられた割込み線
 *  のみ．levelソースのため，割込みハンドラ側でのクリア（intno1_clear
 *  相当）またはclr_intまで要求が保持される）
 */
Inline void
intmtx_raise_int(INTNO intno)
{
	sil_wrw_mem((void *)(ESP32C3_SYSTEM_CPU_INTR_FROM_CPU_0
							+ (uint32_t)intmtx_from_cpu[intno] * 4U), 1U);
}

/*
 *  割込み要求のクリア（clr_int用）
 */
Inline void
intmtx_clear_int(INTNO intno)
{
	sil_wrw_mem((void *)(ESP32C3_SYSTEM_CPU_INTR_FROM_CPU_0
							+ (uint32_t)intmtx_from_cpu[intno] * 4U), 0U);
}

/*
 *  ras_int／clr_intが使用できる割込み番号か（FROM_CPUソースが割り
 *  当てられているか）のチェック
 */
Inline bool_t
intmtx_valid_raise(INTNO intno)
{
	return(intmtx_from_cpu[intno] != 0xFFU);
}

/*
 *  割込み要求ラインに対する割込み優先度の設定（priは内部表現1〜7）
 */
Inline void
intmtx_set_priority(INTNO intno, uint_t pri)
{
	sil_wrw_mem((void *)(INTMTX_BASE + INTMTX_PRI_BASEOFF + intno * 4U),
				(uint32_t)pri);
}

/*
 *  割込み優先度マスクの設定（mは内部表現0〜7．THRESH = m + 1）
 */
Inline void
intmtx_set_thresh(uint_t m)
{
	sil_wrw_mem((void *)(INTMTX_BASE + INTMTX_THRESH_OFF),
				(uint32_t)(m + 1U));
}

/*
 *  割込み優先度マスクの参照（内部表現で返す）
 */
Inline uint_t
intmtx_get_thresh(void)
{
	return((uint_t)sil_rew_mem((void *)(INTMTX_BASE + INTMTX_THRESH_OFF))
				- 1U);
}

/*
 *  ペリフェラル割込みソースのCPU割込み線への割り当て
 *  （ターゲット依存部の初期化で使用する．intmtx_srcmask／
 *  intmtx_from_cpuの更新を伴うためchip_kernel_impl.cに実体を置く）
 */
extern void esp32c3_intmtx_route(uint_t intsrc, INTNO intno);

/*
 *  割込みマトリクスの初期化
 */
extern void intmtx_initialize(void);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_INTMTX_KERNEL_IMPL_H */
