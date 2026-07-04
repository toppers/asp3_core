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
 *    アセンブリ言語用マクロ定義（ESP32-C6用）
 *
 *  pico2_riscv/rp2350版からの流用．内容差分なし（追加コンテキストなし）．
 */

#ifndef TOPPERS_CHIP_ASM_INC
#define TOPPERS_CHIP_ASM_INC

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
 *  コア依存部で共通な定義
 */
#include "core_asm.inc"

#endif /* TOPPERS_CHIP_ASM_INC */
