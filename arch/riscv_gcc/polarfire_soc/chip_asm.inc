/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2024 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこ
 *      と．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
 *      免責すること．
 *
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
 *  に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
 *  アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
 *  の責任を負わない．
 *
 *  $Id: core_asm.inc 64 2018-08-20 06:57:46Z ertl-honda $
 */

/*
 *    アセンブリ言語用マクロ定義（PolarFire SoC GNU開発環境用）
 *
 *  TOPPERS/FMP3のRISC-V依存部をASP3（シングルプロセッサ）用に変換した
 *  もの．自プロセッサIDの取得（my_pid/my_pidx）を削除し，PLICコンテキ
 *  ストINDEXはブートハート（hart1）に固定した．
 */

#ifndef TOPPERS_CHIP_ASM_INC
#define TOPPERS_CHIP_ASM_INC

/*
 *  ブートハートのPLICのコンテキストINDEXの取得（U54のみ）
 *   コンテキストINDEX = ((hartid - 1) * 2) + 1
 *
 *  chip_kernel_impl.h の get_my_plic_cidx()／PLIC_BOOT_CIDX と内容を
 *  統一すること．
 */
.macro my_cidx reg
    li   \reg, (((TOPPERS_BOOT_HARTID - 1) * 2) + 1)
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
 *  タスク実行開始時(start_r)の追加コンテキスト初期化フック．PolarFire SoC
 *  Kitは既定不要(空)．(TLS初期化等に使うチップ依存フック点．a0=p_runtsk
 *  は不変とする契約)
 */
.macro init_additional_regs_start_r tcb_reg
.endm

/*
 *  コア依存部で共通な定義
 */
#include "core_asm.inc"

#endif /* TOPPERS_CHIP_ASM_INC */
