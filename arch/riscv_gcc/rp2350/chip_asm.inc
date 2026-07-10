/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，本ソフトウェアをTOPPERSライセンス（条件は他のソー
 *  スファイルの先頭コメントを参照）の下で利用することを許諾する．本ソ
 *  フトウェアは無保証で提供される．
 */

/*
 *    アセンブリ言語用マクロ定義（RP2350 RISC-V（Hazard3）GNU開発環境用）
 */

#ifndef TOPPERS_CHIP_ASM_INC
#define TOPPERS_CHIP_ASM_INC

/*
 *  CPU例外番号(exccode)の取得（非CLIC標準版：mcauseそのまま）
 *  共通部core_support.Sがexc_table索引前に呼ぶ．CLIC搭載チップは下位12bit
 *  へマスクするが，本チップ（非CLIC）のmcauseは例外コードのみのため素通し．
 */
.macro core_get_exccode_asm reg
    csrr  \reg, mcause
.endm

/*
 *  ディスパッチ時の追加コンテキストの保存
 */
.macro save_additional_regs_disp
.endm

/*
 *  ディスパッチ時に保存した追加コンテキストの復帰
 */
.macro restore_additional_regs_disp
.endm

/*
 *  割込み・例外ハンドラ入口処理での追加コンテキストの保存
 */
.macro save_additional_regs_int
.endm

/*
 *  割込み・例外ハンドラ入口処理で保存した追加コンテキストの復帰
 */
.macro restore_additional_regs_int
.endm

/*
 *  割込み・例外ハンドラからのディスパッチ時の追加コンテキストの保存
 */
.macro save_additional_regs_int_disp
.endm

/*
 *  割込み・例外ハンドラからのディスパッチ時に保存した追加コンテキストの復帰
 */
.macro restore_additional_regs_int_disp
.endm

/*
 *  タスク実行開始時(start_r)の追加コンテキスト初期化フック．RP2350は既定
 *  不要(空)．(TLS初期化等に使うチップ依存フック点．a0=p_runtskは不変と
 *  する契約)
 */
.macro init_additional_regs_start_r tcb_reg
.endm

/*
 *  コア依存部で共通な定義
 */
#include "core_asm.inc"

#endif /* TOPPERS_CHIP_ASM_INC */
