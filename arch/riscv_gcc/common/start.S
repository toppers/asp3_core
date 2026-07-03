/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
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
 *  $Id: start.S 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *    カーネル用のスタートアップモジュール（RISC-V用）
 *
 *  TOPPERS/FMP3のRISC-V依存部をASP3（シングルプロセッサ）用に変換した
 *  もの．マスタ同期（start_sync/slave_wait）を削除し，ブートハート
 *  （TOPPERS_BOOT_HARTID）以外のハートのパーキングを追加している．
 */

#define TOPPERS_MACRO_ONLY
#define TOPPERS_ASM_MACRO
#define UINT_C(val)     (val) /* uint_t型の定数を作るマクロ */
#define ULONG_C(val)    (val) /* ulong_t型の定数を作るマクロ */
#define CAST(type, val) (val) /* 型キャストを行うマクロ */

#include "kernel_impl.h"
#include "target_asm.inc"

    ASECTION(.text.start)
    AALIGN(4)
    AGLOBAL(start)
ALABEL(start)
    /*
     *  Machine割込みの禁止
     *
     *  mie/mip CSRを実装しないチップ（ESP32-C3＝アクセスすると不正
     *  命令例外になる）は，チップ依存部でTOPPERS_OMIT_MIE_INITを定義
     *  してクリアを抑止する（割込みの禁止はmstatus.MIEで足りる）．
     */
    csrrc  t0, mstatus, MSTATUS_MIE
#ifndef TOPPERS_OMIT_MIE_INIT
    csrw   mie, 0
    csrw   mip, 0
#endif /* TOPPERS_OMIT_MIE_INIT */

    /*
     *  ブートハート以外のパーキング
     *
     *  QEMU(microchip-icicle-kit)では全ハート（E51×1＋U54×4）が同一
     *  エントリへジャンプしてくる．E51（hart0）はFPUを持たないため，
     *  FPUの初期化より前にハートの判定を行わなければならない．
     */
    csrr   t0, mhartid
    li     t1, TOPPERS_BOOT_HARTID
    bne    t0, t1, park_hart

    /*
     *  スタックポインタ(sp)の初期化
     */
    la     t0, istkpt
    lx     sp, 0(t0)

    /*
     *  グローバルポインタ(gp)の初期化
     */
    AO_PUSH
    AO_NORELAX      /* gp相対を使用しない */
    la gp, __global_pointer$
    AO_POP

#ifdef __riscv_flen
    /*
     *  FPUの初期化
     */
    li    t0, ~MSTATUS_FS          /* mstatus.FS を Initに */
    li    t1, MSTATUS_FS_INIT
    csrr  t2, mstatus
    and   t2, t2, t0
    or    t2, t2, t1
    csrw  mstatus, t2
    li    t0, FCSR_ROUNDING_MODE
    fscsr t0                 /* ラウンディングモード設定 */
#endif /* __riscv_flen */

    /*
     *  hardware_init_hookの呼出し
     *
     *  ターゲットハードウェアに依存して必要な初期化処理がある場合は，
     *  hardware_init_hookという関数を用意する．
     */
    jal  hardware_init_hook

#ifndef TOPPERS_OMIT_BSS_INIT
    /*
     *  bssセクションのクリア
     *
     *  BSS_STARTからBSS_ENDまでをゼロクリアする（sbssを含む．リンカ
     *  スクリプトで.sbss/.bssを__bss_start〜__bss_endに含めること）．
     */
    la    a0, __bss_start
    la    a1, __bss_end
    bgeu  a0, a1, start_3
ALABEL(start_2)
    sx    zero, (a0)
    addi  a0, a0, XREG_SIZE
    bltu  a0, a1, start_2
ALABEL(start_3)
#endif /* TOPPERS_OMIT_BSS_INIT */

#ifndef TOPPERS_OMIT_DATA_INIT
    /*
     *  dataセクションの初期化（ROM化対応）
     *
     *  __idata_startから__idata_endまでを，__data_start以降にコピーす
     *  る．
     */
    la    a0, __idata_start
    la    a1, __data_start
    la    a2, __idata_end
    bgeu  a0, a2, start_5
ALABEL(start_4)
    lx    t0, (a0)
    sx    t0, (a1)
    addi  a0, a0, XREG_SIZE
    addi  a1, a1, XREG_SIZE
    bltu  a0, a2, start_4
ALABEL(start_5)
#endif /* TOPPERS_OMIT_DATA_INIT */

    /*
     *  software_init_hookの呼出し
     *
     *  ソフトウェア環境（特にライブラリ）に依存して必要な初期化処理が
     *  ある場合は，software_init_hookという関数を用意する．
     */
    jal  software_init_hook

    /*
     *  カーネルを起動する
     */
    j    sta_ker

    /*
     *  ブートハート以外の待機ルーチン
     */
ALABEL(park_hart)
    wfi
    j    park_hart

/*
 *  デフォルトのhardware_init_hook（weak定義）
 */
    ATEXT
    AALIGN(2)
    AWEAK(hardware_init_hook)
ALABEL(hardware_init_hook)
    jr  ra

/*
 *  デフォルトのsoftware_init_hook（weak定義）
 */
    ATEXT
    AALIGN(2)
    AWEAK(software_init_hook)
ALABEL(software_init_hook)
    jr  ra
