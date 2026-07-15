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
 *  $Id: arm.h 341 2023-04-19 13:42:56Z ertl-honda $
 */

/*
 *    RISC-Vコアサポートモジュール
 */

#ifndef TOPPERS_RISCV_H
#define TOPPERS_RISCV_H

#include <t_stddef.h>

/*
 *  mstatus
 */
#define MSTATUS_MIE   UINT_C(0x00000008)
#define MSTATUS_MPIE  UINT_C(0x00000080)
#define MSTATUS_MPP_M UINT_C(0x00001800)   /* MPP = Mモード(bits 12:11 = 0b11) */
#define MSTATUS_FS    UINT_C(0x00006000)

#define MSTATUS_FS_INIT   UINT_C(0x00002000)
#define MSTATUS_FS_CLEAN  UINT_C(0x00004000)
#define MSTATUS_FS_DIRTY  UINT_C(0x00006000)

/*
 *  mie
 */
#define MIE_MSIE  UINT_C(0x008)
#define MIE_MTIE  UINT_C(0x080)
#define MIE_MEIE  UINT_C(0x800)

/*
 *  mtvec
 */
#define MTVEC_MODE_MASK     UINT_C(0x3)
#define MTVEC_MODE_DIRECT   UINT_C(0x0)
#define MTVEC_MODE_VECTORD  UINT_C(0x1)

/*
 *  mcause
 */
#define MCAUSE_MSI  UINT_C(3)
#define MCAUSE_MTI  UINT_C(7)
#define MCAUSE_MEI  UINT_C(11)

/*
 *  fcsr
 */
#define FCSR_RNE_MODE  UINT_C(0x00000000)
#define FCSR_RTZ_MODE  UINT_C(0x00000020)
#define FCSR_RDN_MODE  UINT_C(0x00000040)
#define FCSR_RUP_MODE  UINT_C(0x00000060)
#define FCSR_RMM_MODE  UINT_C(0x00000080)

/*
 *  RISC-Vコアの特殊命令のインライン関数定義
 */
#ifndef TOPPERS_MACRO_ONLY
#include "riscv_insn.h"
#endif /*  TOPPERS_MACRO_ONLY */

#endif /* TOPPERS_RISCV_H */
