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
 *  $Id: core_support.S 382 2023-09-14 04:39:49Z ertl-honda $
 */

/*
 *    カーネルのコア依存部のアセンブリ言語部（RISC-V用）
 *
 *  TOPPERS/FMP3のRISC-V依存部をASP3（シングルプロセッサ）用に変換した
 *  もの．主な変換内容：
 *    - my_pcb／PCBオフセット参照を廃止し，グローバル変数
 *      （p_runtsk・p_schedtsk・excpt_nest_count・istkpt）を直接参照
 *    - dispatch_and_migrate／exit_and_migrate／force_unlock_spin を削除
 *    - start_dispatchをASP3流に（タスク1のスタックへ切替）
 *    - アイドル処理は専用スタックを使わない（dispatcher_1）
 *    - 割込み／例外フレームの破棄処理の符号誤りを修正（FMP3では負値で
 *      spを下げていたが，アイドル専用スタックがあったため顕在化せず）
 *    - 割込み入口のFPUレジスタ保存・復帰のfa3（f13）がfs3になっていた
 *      誤記を修正
 */

#define TOPPERS_MACRO_ONLY
#define TOPPERS_ASM_MACRO
#define UINT_C(val)     (val) /* uint_t型の定数を作るマクロ */
#define ULONG_C(val)    (val) /* ulong_t型の定数を作るマクロ */
#define CAST(type, val) (val) /* 型キャストを行うマクロ */

#include "kernel_impl.h"
#include "offset.h"
#include "target_asm.inc"

/*
 *  ディスパッチ時に保存する汎用レジスタ数
 */
#ifndef __riscv_32e
#define TNUM_RISCV_DISP_XREG  13
#else /* __riscv_32e */
#define TNUM_RISCV_DISP_XREG   3
#endif /* !__riscv_32e */

/*
 *  ディスパッチ時に保存する浮動小数点用汎用レジスタ数
 */
#define TNUM_RISCV_DISP_FREG  12

/*
 *  割込み/例外入口処理で保存する汎用レジスタ数
 */
#ifndef __riscv_32e
#define TNUM_RISCV_INT_XREG  19
#else /* __riscv_32e */
#define TNUM_RISCV_INT_XREG  13
#endif /* !__riscv_32e */

/*
 *  割込み/例外入口処理で保存する浮動小数点用汎用レジスタ数
 */
#define TNUM_RISCV_INT_FREG  20

/*
 *  割込みからのディスパッチで保存する汎用レジスタ数
 */
#ifndef __riscv_32e
#define TNUM_RISCV_INT_DISP_XREG  12
#else /* __riscv_32e */
#define TNUM_RISCV_INT_DISP_XREG   2
#endif /* !__riscv_32e */

/*
 *  割込みからのディスパッチで保存する浮動小数点用汎用レジスタ数
 */
#define TNUM_RISCV_INT_DISP_FREG  12

/*
 *  タスクディスパッチャ
 */
    ATEXT
    AALIGN(2)
    AGLOBAL(dispatch)
ALABEL(dispatch)
    /*
     *  このルーチンは，タスクコンテキスト・CPUロック状態・割込み優先度
     *  マスク全解除状態・ディスパッチ許可状態で呼び出される．
     */
     /*
      * ターゲット依存の追加レジスタの保存
      */
    save_additional_regs_disp
#ifdef __riscv_flen
    addi sp, sp, -(TNUM_RISCV_DISP_FREG * FREG_SIZE)
    sf   fs0,   0*FREG_SIZE(sp)  /* f8  */
    sf   fs1,   1*FREG_SIZE(sp)  /* f9  */
    sf   fs2,   2*FREG_SIZE(sp)  /* f18 */
    sf   fs3,   3*FREG_SIZE(sp)  /* f19 */
    sf   fs4,   4*FREG_SIZE(sp)  /* f20 */
    sf   fs5,   5*FREG_SIZE(sp)  /* f21 */
    sf   fs6,   6*FREG_SIZE(sp)  /* f22 */
    sf   fs7,   7*FREG_SIZE(sp)  /* f23 */
    sf   fs8,   8*FREG_SIZE(sp)  /* f24 */
    sf   fs9,   9*FREG_SIZE(sp)  /* f25 */
    sf   fs10, 10*FREG_SIZE(sp)  /* f26 */
    sf   fs11, 11*FREG_SIZE(sp)  /* f27 */
#endif /* __riscv_flen */
    addi sp, sp, -(TNUM_RISCV_DISP_XREG * XREG_SIZE)
    sx   ra,   0*XREG_SIZE(sp)  /* x1  */
    sx   s0,   1*XREG_SIZE(sp)  /* x8  */
    sx   s1,   2*XREG_SIZE(sp)  /* x9  */
#ifndef __riscv_32e
    sx   s2,   3*XREG_SIZE(sp)  /* x18 */
    sx   s3,   4*XREG_SIZE(sp)  /* x19 */
    sx   s4,   5*XREG_SIZE(sp)  /* x20 */
    sx   s5,   6*XREG_SIZE(sp)  /* x21 */
    sx   s6,   7*XREG_SIZE(sp)  /* x22 */
    sx   s7,   8*XREG_SIZE(sp)  /* x23 */
    sx   s8,   9*XREG_SIZE(sp)  /* x24 */
    sx   s9,  10*XREG_SIZE(sp)  /* x25 */
    sx   s10, 11*XREG_SIZE(sp)  /* x26 */
    sx   s11, 12*XREG_SIZE(sp)  /* x27 */
#endif /* !__riscv_32e */

    la      a0, p_runtsk
    lx      a0, 0(a0)             /* p_runtsk → a0 */
    sx      sp, TCB_sp(a0)        /* スタックポインタを保存 */
    la      a1, dispatch_r
    sx      a1, TCB_pc(a0)        /* 実行再開番地を保存 */
    j       dispatcher            /* a0にはp_runtskが格納されている */


ALABEL(dispatch_r)
    /*
     * 【この時点のレジスタ状態】
     *  a0：p_runtsk（タスク切換え後）
     */
    lx   ra,   0*XREG_SIZE(sp)  /* x1  */
    lx   s0,   1*XREG_SIZE(sp)  /* x8  */
    lx   s1,   2*XREG_SIZE(sp)  /* x9  */
#ifndef __riscv_32e
    lx   s2,   3*XREG_SIZE(sp)  /* x18 */
    lx   s3,   4*XREG_SIZE(sp)  /* x19 */
    lx   s4,   5*XREG_SIZE(sp)  /* x20 */
    lx   s5,   6*XREG_SIZE(sp)  /* x21 */
    lx   s6,   7*XREG_SIZE(sp)  /* x22 */
    lx   s7,   8*XREG_SIZE(sp)  /* x23 */
    lx   s8,   9*XREG_SIZE(sp)  /* x24 */
    lx   s9,  10*XREG_SIZE(sp)  /* x25 */
    lx   s10, 11*XREG_SIZE(sp)  /* x26 */
    lx   s11, 12*XREG_SIZE(sp)  /* x27 */
#endif /* !__riscv_32e */
    addi sp, sp, (TNUM_RISCV_DISP_XREG * XREG_SIZE)
#ifdef __riscv_flen
    lf   fs0,   0*FREG_SIZE(sp)  /* f8 */
    lf   fs1,   1*FREG_SIZE(sp)  /* f9 */
    lf   fs2,   2*FREG_SIZE(sp)  /* f18 */
    lf   fs3,   3*FREG_SIZE(sp)  /* f19 */
    lf   fs4,   4*FREG_SIZE(sp)  /* f20 */
    lf   fs5,   5*FREG_SIZE(sp)  /* f21 */
    lf   fs6,   6*FREG_SIZE(sp)  /* f22 */
    lf   fs7,   7*FREG_SIZE(sp)  /* f23 */
    lf   fs8,   8*FREG_SIZE(sp)  /* f24 */
    lf   fs9,   9*FREG_SIZE(sp)  /* f25 */
    lf   fs10, 10*FREG_SIZE(sp)  /* f26 */
    lf   fs11, 11*FREG_SIZE(sp)  /* f27 */
    addi sp, sp, (TNUM_RISCV_DISP_FREG * FREG_SIZE)
#endif /* __riscv_flen */
     /*
      * ターゲット依存の追加レジスタの復帰
      */
    restore_additional_regs_disp
    jr   ra


/*
 *  ディスパッチャの動作開始
 */
    AGLOBAL(start_dispatch)
ALABEL(start_dispatch)
    /*
     *  このルーチンは，カーネル起動時に，非タスクコンテキストで，NMIを
     *  除くすべての割込みを禁止した状態（全割込みロック状態と同等）で
     *  呼び出される．
     *
     *  dispatcher_0へ分岐する前に，タスクコンテキスト・CPUロック状態・
     *  割込み優先度マスク全解除状態にし，使用するスタックを，IDが1のタ
     *  スクのスタック領域に切り換えなければならない．
     */

    /*
     *  各種のデバイス（特に割込みコントローラ）の設定が完了するのを待つ．
     */
    riscv_dmb

    /*
     *  タスクコンテキストに切り換える．
     */
    la      a0, excpt_nest_count
    sw      zero, 0(a0)           /* 例外ネストカウントを0に */

    /*
     *  IDが1のタスクのスタック領域に切り換える．
     */
    la      a0, tinib_table       /* tinib_table[0] → a0 */
    lx      t0, TINIB_stk(a0)
    lx      t1, TINIB_stksz(a0)
    add     t0, t0, t1
    mv      sp, t0

    /*
     *  CPUロック状態 & 全割込みロック解除状態として，
     *  ディスパッチャ本体へ分岐する．
     */
    lock_cpu_and_unlock_allint_asm t0
    j  dispatcher_0

/*
 *  現在のコンテキストを捨ててディスパッチ
 */
    AGLOBAL(exit_and_dispatch)
ALABEL(exit_and_dispatch)
    /*
     *  このルーチンは，タスクコンテキスト・CPUロック状態・割込み優先度
     *  マスク全解除状態・ディスパッチ許可状態で呼び出される．
     */
#ifdef LOG_DSP_ENTER
    la      a0, p_runtsk
    lx      a0, 0(a0)             /* p_runtsk → a0 */
#endif /* LOG_DSP_ENTER */
    /* ディスパッチャ本体（dispatcher）へ */

/*
 *  ディスパッチャ本体
 */
ALABEL(dispatcher)
#ifdef LOG_DSP_ENTER
    /*
     * 【この時点のレジスタ状態】
     *  a0：p_runtsk（タスク切換え前）
     */
    jal  ra, log_dsp_enter
#endif /* LOG_DSP_ENTER */

ALABEL(dispatcher_0)
    /*
     *  このルーチンは，タスクコンテキスト・CPUロック状態・割込み優先度
     *  マスク全解除状態・ディスパッチ許可状態で呼び出される．実行再開番
     *  地へもこの状態のまま分岐する．
     */
    la     s0, p_schedtsk
    lx     a0, 0(s0)               /* p_schedtsk → a0 → p_runtsk */
    la     s1, p_runtsk
    sx     a0, 0(s1)
    beqz   a0, dispatcher_1        /* p_runtskがNULLならdispatcher_1へ */
    lx     sp, TCB_sp(a0)
#ifdef LOG_DSP_LEAVE
    mv     s0, a0                  /* p_runtskをs0に保存 */
    jal    ra, log_dsp_leave
    mv     a0, s0
#endif /* LOG_DSP_LEAVE */
    lx     s1, TCB_pc(a0)          /* 実行再開番地を復帰 */
    jr     s1                      /* p_runtskをa0に入れた状態で分岐する */

    /*
     *  アイドル処理
     *
     *  割込みをすべて許可し，CPUロック解除状態にして割込みを待つ．
     *  アイドル処理用の専用スタックは使わない（この時点のスタックの
     *  まま割込みを待つ）．
     */
ALABEL(dispatcher_1)
    /*
     *  ターゲットによっては，省電力モード等に移行するため，標準の方法
     *  と異なる手順が必要な場合がある．そのようなターゲットでは，ター
     *  ゲット依存部でTOPPERS_CUSTOM_IDLEを定義し，アセンブラマクロとし
     *  て，toppers_asm_custom_idleを用意すればよい．
     */
ALABEL(dispatcher_2)
#ifdef TOPPERS_CUSTOM_IDLE
    toppers_asm_custom_idle
#else /* TOPPERS_CUSTOM_IDLE */
    unlock_cpu_asm t0
#endif /* TOPPERS_CUSTOM_IDLE */
    j  dispatcher_2            /* 割込み待ち */

/*
 *  カーネルの終了処理の呼出し
 *
 *  使用するスタックを非タスクコンテキスト用のスタック領域に切り替え，
 *  例外ネストカウントを1にして，exit_kernelを呼び出す．
 */
    AGLOBAL(call_exit_kernel)
ALABEL(call_exit_kernel)
    la  a0, istkpt
    lx  sp, 0(a0)             /* 非タスクコンテキスト用のスタックに */
    la  a0, excpt_nest_count
    li  a1, 1
    sw  a1, 0(a0)             /* 例外ネストカウントを1に */
    j   exit_kernel

/*
 *  タスクの実行開始時処理
 */
    ATEXT
    AALIGN(2)
    AGLOBAL(start_r)
    /*
     * 【この時点のレジスタ状態】
     *  a0：p_runtsk（タスク切換え後）
     *
     *  init_additional_regs_start_r（チップ依存，chip_asm.inc）は，タスク実行
     *  開始時の追加コンテキスト初期化フック（例：TLS初期化によるtp設定）．
     *  a0（p_runtsk）は不変とする契約．既定は空マクロ（他チップは無影響）．
     */
ALABEL(start_r)
    unlock_cpu_asm t0         /* CPUロック解除状態に */
    la  ra, ext_tsk           /* タスク本体からの戻り番地を設定 */
    init_additional_regs_start_r a0   /* チップ依存の追加コンテキスト初期化(a0=p_runtsk) */
    lx  a1, TCB_p_tinib(a0)   /* p_runtsk->p_tinib → a1 */
    lx  a0, TINIB_exinf(a1)   /* exinfをパラメータに */
    lx  a2, TINIB_task(a1)    /* タスク起動番地にジャンプ */
    jr  a2

/*
 *  割込みハンドラの出入口処理
 */
#ifndef OMIT_CORE_INT_ENTRY

    ATEXT
    AALIGN(4)
    AGLOBAL(core_int_entry)
ALABEL(core_int_entry)
#ifdef USE_RISCV_DIRECT_TRAP
    /*
     *  例外の場合は例外入口へ
     */
    csrw mscratch, t0
    csrr t0, mcause
    srli t0, t0, 31
    beqz t0, core_exc_entry
    csrr t0, mscratch
#endif /* USE_RISCV_DIRECT_TRAP */
     /*
      * ターゲット依存の追加レジスタの保存
      */
    save_additional_regs_int
#ifdef __riscv_flen
    addi sp, sp, -(TNUM_RISCV_INT_FREG * FREG_SIZE)
    sf   ft0,   0*FREG_SIZE(sp)  /* f0  */
    sf   ft1,   1*FREG_SIZE(sp)  /* f1  */
    sf   ft2,   2*FREG_SIZE(sp)  /* f2  */
    sf   ft3,   3*FREG_SIZE(sp)  /* f3  */
    sf   ft4,   4*FREG_SIZE(sp)  /* f4  */
    sf   ft5,   5*FREG_SIZE(sp)  /* f5  */
    sf   ft6,   6*FREG_SIZE(sp)  /* f6  */
    sf   ft7,   7*FREG_SIZE(sp)  /* f7  */
    sf   fa0,   8*FREG_SIZE(sp)  /* f10 */
    sf   fa1,   9*FREG_SIZE(sp)  /* f11 */
    sf   fa2,  10*FREG_SIZE(sp)  /* f12 */
    sf   fa3,  11*FREG_SIZE(sp)  /* f13 */
    sf   fa4,  12*FREG_SIZE(sp)  /* f14 */
    sf   fa5,  13*FREG_SIZE(sp)  /* f15 */
    sf   fa6,  14*FREG_SIZE(sp)  /* f16 */
    sf   fa7,  15*FREG_SIZE(sp)  /* f17 */
    sf   ft8,  16*FREG_SIZE(sp)  /* f28 */
    sf   ft9,  17*FREG_SIZE(sp)  /* f29 */
    sf   ft10, 18*FREG_SIZE(sp)  /* f30 */
    sf   ft11, 19*FREG_SIZE(sp)  /* f31 */
#endif /* __riscv_flen */
    addi sp, sp, -(TNUM_RISCV_INT_XREG * XREG_SIZE)
    sx   ra,  0*XREG_SIZE(sp)  /* x1  */
    sx   tp,  1*XREG_SIZE(sp)  /* x4  */
    sx   t0,  2*XREG_SIZE(sp)  /* x5  */
    sx   t1,  3*XREG_SIZE(sp)  /* x6  */
    sx   t2,  4*XREG_SIZE(sp)  /* x7  */
    sx   a0,  5*XREG_SIZE(sp)  /* x10 */
    sx   a1,  6*XREG_SIZE(sp)  /* x11 */
    sx   a2,  7*XREG_SIZE(sp)  /* x12 */
    sx   a3,  8*XREG_SIZE(sp)  /* x13 */
    sx   a4,  9*XREG_SIZE(sp)  /* x14 */
    sx   a5, 10*XREG_SIZE(sp)  /* x15 */
    csrr t0, mepc
    sx   t0, 11*XREG_SIZE(sp)
    csrr t0, mstatus
    sx   t0, 12*XREG_SIZE(sp)
#ifndef __riscv_32e
    sx   t3, 13*XREG_SIZE(sp)  /* x28 */
    sx   t4, 14*XREG_SIZE(sp)  /* x29 */
    sx   t5, 15*XREG_SIZE(sp)  /* x30 */
    sx   t6, 16*XREG_SIZE(sp)  /* x31 */
    sx   a6, 17*XREG_SIZE(sp)  /* x16 */
    sx   a7, 18*XREG_SIZE(sp)  /* x17 */
#endif /* !__riscv_32e */

    /*
     *  スタックポインタの調整
     */
    andi t0, sp, 0xf
    sub  sp, sp, t0
    addi sp, sp, -16  /* スタックポインタの調整値を保存 */
    sx   t0, 0(sp)

    /*
     *  例外ネストカウントをインクリメントする．割込みが非タスクコンテキ
     *  ストで発生した場合には，core_int_entry_1へ分岐する．
     */
    la      a0, excpt_nest_count
    lw      t0, 0(a0)
    addi    t1, t0, 1
    sw      t1, 0(a0)
    bnez    t0, core_int_entry_1  /* 割込みが非タスクコンテキストで発生 */
                                  /*            ならcore_int_entry_1に分岐 */

    /*
     *  非タスクコンテキスト用のスタックに切り換える．
     */
    mv      t0, sp                 /* この時点のスタックポインタをt0に */
    la      a0, istkpt
    lx      sp, 0(a0)              /* 非タスクコンテキスト用のスタックに */
    addi    sp, sp, -16            /* 切換え前のスタックポインタを保存 */
    sx      t0, 0(sp)

ALABEL(core_int_entry_1)
    /*
     *  割込み入口処理での割込みコントローラ操作
     *
     *  受け付けた割込みに応じて割込み優先度マスクをセットする
     *  以下の内容をレジスタに入れてリターンする
     *   a0:割込み番号
     *   a1:割込み前の優先度(内部表現)
     *   a2:割込みハンドラアドレス
     */
    jal  irc_begin_int

    /*
     *  Spurious割込みなら何もしない
     */
    beqz a0, core_int_entry_2

    /*
     *  割込み関連の情報を保存
     */
    addi sp, sp, -(4*XREG_SIZE) /* XLEN=64でも32でも16byte境界となる */
    sx   a0, 0*XREG_SIZE(sp)    /* 割込み番号保存 */
    sx   a1, 1*XREG_SIZE(sp)    /* 割込み前の優先度(内部表現)保存 */
    sx   a2, 2*XREG_SIZE(sp)    /* 割込みハンドラアドレス保存 */

    /*
     *  CPUロック解除状態 & 全割込みロック解除状態とする．
     */
    unlock_cpu_and_unlock_allint_asm  t0

#ifdef LOG_INH_ENTER
    /*
     *  ログ出力の呼出し
     */
    jal log_inh_enter
    lx  a0, 0*XREG_SIZE(sp)  /* 割込み番号復帰 */
    lx  a2, 2*XREG_SIZE(sp)  /* 割込みハンドラアドレス復帰 */
#endif /* LOG_INH_ENTER */

    /*
     *  割込みハンドラの呼出し
     */
    jalr a2

#ifdef LOG_INH_LEAVE
    /*
     *  ログ出力の呼出し
     */
    lx  a0, 0*XREG_SIZE(sp)  /* 割込み番号復帰 */
    jal ra, log_inh_leave
#endif /* LOG_INH_LEAVE */

    /*
     *  カーネル管理の割込みを禁止する．
     */
    lock_cpu_asm t0

    lx   a0, 0*XREG_SIZE(sp)    /* 割込み番号復帰 */
    lx   a1, 1*XREG_SIZE(sp)    /* 割込み前の優先度(内部表現)復帰 */
    addi sp, sp, (4*XREG_SIZE)

    /*
     *  割込みコントローラを操作する．
     */
ALABEL(core_int_entry_2)
    jal  irc_end_int

    /*
     *  例外ネストカウントをデクリメントする．
     */
    la     a0, excpt_nest_count
    lw     t0, 0(a0)
    addi   t0, t0, -1
    sw     t0, 0(a0)             /* 戻り先が非タスクコンテキスト */
    bnez   t0, core_int_entry_5  /*     ならcore_int_entry_5に分岐 */

    /*
     *  タスク用のスタックに戻す．
     */
    lx  sp, 0(sp)

    /*
     *  p_runtskがNULLか判定する．
     */
    la    a0, p_runtsk
    lx    a1, 0(a0)               /* p_runtsk → a1 */
    bnez  a1, core_int_entry_3    /* p_runtskがNULLでなければ */
                                  /*   core_int_entry_3に分岐 */

    /*
     *  IDLE処理へのリターン（IDLE処理に割り込んだ割込みからリターン）
     *  タスクのスタックに保存したスクラッチレジスタ等を捨てる．
     */
    lx   t0, 0(sp)   /* スタックポインタの調整を元に戻す */
    addi sp, sp, 16
    add  sp, sp, t0

    addi sp, sp, (TNUM_RISCV_INT_XREG * XREG_SIZE)
#ifdef __riscv_flen
    addi sp, sp, (TNUM_RISCV_INT_FREG * FREG_SIZE)
#endif /* __riscv_flen */
    j    dispatcher_0

ALABEL(core_int_entry_3)
    /*
     * 【この時点のレジスタ状態】
     *  a1：p_runtsk
     */

    /*
     *  ディスパッチが必要か判定する．
     */
    la   a0, p_schedtsk
    lx   t0, 0(a0)                /* p_schedtsk → t0 */
    beq  t0, a1, core_int_entry_4 /* p_runtskとp_schedtskが同じなら */
                                  /*             core_int_entry_4へ */

    /*
     *  コンテキストを保存する．
     *
     * 【この時点のレジスタ状態】
     *  a1：p_runtsk
     */
     /*
      * ターゲット依存の追加レジスタの保存
      */
    save_additional_regs_int_disp
#ifdef __riscv_flen
    addi sp, sp, -(TNUM_RISCV_INT_DISP_FREG * FREG_SIZE)
    sf   fs0,   0*FREG_SIZE(sp)  /* f8 */
    sf   fs1,   1*FREG_SIZE(sp)  /* f9 */
    sf   fs2,   2*FREG_SIZE(sp)  /* f18 */
    sf   fs3,   3*FREG_SIZE(sp)  /* f19 */
    sf   fs4,   4*FREG_SIZE(sp)  /* f20 */
    sf   fs5,   5*FREG_SIZE(sp)  /* f21 */
    sf   fs6,   6*FREG_SIZE(sp)  /* f22 */
    sf   fs7,   7*FREG_SIZE(sp)  /* f23 */
    sf   fs8,   8*FREG_SIZE(sp)  /* f24 */
    sf   fs9,   9*FREG_SIZE(sp)  /* f25 */
    sf   fs10, 10*FREG_SIZE(sp)  /* f26 */
    sf   fs11, 11*FREG_SIZE(sp)  /* f27 */
#endif /* __riscv_flen */
    addi sp, sp, -(TNUM_RISCV_INT_DISP_XREG * XREG_SIZE)
    sx   s0,   0*XREG_SIZE(sp)  /*  x8 */
    sx   s1,   1*XREG_SIZE(sp)  /*  x9 */
#ifndef __riscv_32e
    sx   s2,   2*XREG_SIZE(sp)  /* x18 */
    sx   s3,   3*XREG_SIZE(sp)  /* x19 */
    sx   s4,   4*XREG_SIZE(sp)  /* x20 */
    sx   s5,   5*XREG_SIZE(sp)  /* x21 */
    sx   s6,   6*XREG_SIZE(sp)  /* x22 */
    sx   s7,   7*XREG_SIZE(sp)  /* x23 */
    sx   s8,   8*XREG_SIZE(sp)  /* x24 */
    sx   s9,   9*XREG_SIZE(sp)  /* x25 */
    sx   s10, 10*XREG_SIZE(sp)  /* x26 */
    sx   s11, 11*XREG_SIZE(sp)  /* x27 */
#endif /* !__riscv_32e */

    mv a0, a1
    sx sp, TCB_sp(a0)  /* スタックポインタを保存 */
    la t0, ret_int_r   /* 実行再開番地を保存 */
    sx t0, TCB_pc(a0)
    j  dispatcher      /* a0にはp_runtskが格納されている */

ALABEL(ret_int_r)
    /*
     *  コンテキストを復帰する．
     *
     * 【この時点のレジスタ状態】
     *  a0：p_runtsk（タスク切換え後）
     */
    lx   s0,   0*XREG_SIZE(sp)  /* x8  */
    lx   s1,   1*XREG_SIZE(sp)  /* x9  */
#ifndef __riscv_32e
    lx   s2,   2*XREG_SIZE(sp)  /* x18 */
    lx   s3,   3*XREG_SIZE(sp)  /* x19 */
    lx   s4,   4*XREG_SIZE(sp)  /* x20 */
    lx   s5,   5*XREG_SIZE(sp)  /* x21 */
    lx   s6,   6*XREG_SIZE(sp)  /* x22 */
    lx   s7,   7*XREG_SIZE(sp)  /* x23 */
    lx   s8,   8*XREG_SIZE(sp)  /* x24 */
    lx   s9,   9*XREG_SIZE(sp)  /* x25 */
    lx   s10, 10*XREG_SIZE(sp)  /* x26 */
    lx   s11, 11*XREG_SIZE(sp)  /* x27 */
#endif /* !__riscv_32e */
    addi sp, sp, (TNUM_RISCV_INT_DISP_XREG * XREG_SIZE)
#ifdef __riscv_flen
    lf   fs0,   0*FREG_SIZE(sp)  /* f8  */
    lf   fs1,   1*FREG_SIZE(sp)  /* f9  */
    lf   fs2,   2*FREG_SIZE(sp)  /* f18 */
    lf   fs3,   3*FREG_SIZE(sp)  /* f19 */
    lf   fs4,   4*FREG_SIZE(sp)  /* f20 */
    lf   fs5,   5*FREG_SIZE(sp)  /* f21 */
    lf   fs6,   6*FREG_SIZE(sp)  /* f22 */
    lf   fs7,   7*FREG_SIZE(sp)  /* f23 */
    lf   fs8,   8*FREG_SIZE(sp)  /* f24 */
    lf   fs9,   9*FREG_SIZE(sp)  /* f25 */
    lf   fs10, 10*FREG_SIZE(sp)  /* f26 */
    lf   fs11, 11*FREG_SIZE(sp)  /* f27 */
    addi sp, sp, (TNUM_RISCV_INT_DISP_FREG * FREG_SIZE)
#endif /* __riscv_flen */
     /*
      * ターゲット依存の追加レジスタの復帰
      */
    restore_additional_regs_int_disp

ALABEL(core_int_entry_4)

    /*
     *  割込み処理からのリターン
     *
     *  割込み処理からのリターンにより，CPUロック解除状態に遷移するよ
     *  うにする必要がある．
     */
ALABEL(core_int_entry_5)
    /*
     *  割込み処理からのリターンにより
     *  CPUロック解除状態になるように設定
     */
    ret_int_prepare_unlock_cpu_asm t0

    lx   t0, 0(sp)   /* スタックポインタの調整を元に戻す */
    addi sp, sp, 16
    add  sp, sp, t0

    lx   t0, 11*XREG_SIZE(sp)
    csrw mepc,  t0
    lx   t0, 12*XREG_SIZE(sp)
    csrw mstatus, t0
    lx   ra,  0*XREG_SIZE(sp)  /* x1  */
    lx   tp,  1*XREG_SIZE(sp)  /* x4  */
    lx   t0,  2*XREG_SIZE(sp)  /* x5  */
    lx   t1,  3*XREG_SIZE(sp)  /* x6  */
    lx   t2,  4*XREG_SIZE(sp)  /* x7  */
    lx   a0,  5*XREG_SIZE(sp)  /* x10 */
    lx   a1,  6*XREG_SIZE(sp)  /* x11 */
    lx   a2,  7*XREG_SIZE(sp)  /* x12 */
    lx   a3,  8*XREG_SIZE(sp)  /* x13 */
    lx   a4,  9*XREG_SIZE(sp)  /* x14 */
    lx   a5, 10*XREG_SIZE(sp)  /* x15 */
#ifndef __riscv_32e
    lx   t3, 13*XREG_SIZE(sp)  /* x28 */
    lx   t4, 14*XREG_SIZE(sp)  /* x29 */
    lx   t5, 15*XREG_SIZE(sp)  /* x30 */
    lx   t6, 16*XREG_SIZE(sp)  /* x31 */
    lx   a6, 17*XREG_SIZE(sp)  /* x16 */
    lx   a7, 18*XREG_SIZE(sp)  /* x17 */
#endif /* !__riscv_32e */
    addi sp, sp, (TNUM_RISCV_INT_XREG * XREG_SIZE)
#ifdef __riscv_flen
    lf   ft0,   0*FREG_SIZE(sp)  /* f0  */
    lf   ft1,   1*FREG_SIZE(sp)  /* f1  */
    lf   ft2,   2*FREG_SIZE(sp)  /* f2  */
    lf   ft3,   3*FREG_SIZE(sp)  /* f3  */
    lf   ft4,   4*FREG_SIZE(sp)  /* f4  */
    lf   ft5,   5*FREG_SIZE(sp)  /* f5  */
    lf   ft6,   6*FREG_SIZE(sp)  /* f6  */
    lf   ft7,   7*FREG_SIZE(sp)  /* f7  */
    lf   fa0,   8*FREG_SIZE(sp)  /* f10 */
    lf   fa1,   9*FREG_SIZE(sp)  /* f11 */
    lf   fa2,  10*FREG_SIZE(sp)  /* f12 */
    lf   fa3,  11*FREG_SIZE(sp)  /* f13 */
    lf   fa4,  12*FREG_SIZE(sp)  /* f14 */
    lf   fa5,  13*FREG_SIZE(sp)  /* f15 */
    lf   fa6,  14*FREG_SIZE(sp)  /* f16 */
    lf   fa7,  15*FREG_SIZE(sp)  /* f17 */
    lf   ft8,  16*FREG_SIZE(sp)  /* f28 */
    lf   ft9,  17*FREG_SIZE(sp)  /* f29 */
    lf   ft10, 18*FREG_SIZE(sp)  /* f30 */
    lf   ft11, 19*FREG_SIZE(sp)  /* f31 */
    addi sp, sp, (TNUM_RISCV_INT_FREG * FREG_SIZE)
#endif /* __riscv_flen */
     /*
      * ターゲット依存の追加レジスタの復帰
      */
    restore_additional_regs_int
    mret

#endif /* OMIT_CORE_INT_ENTRY */

/*
 *  CPU例外ハンドラの出入口処理
 */
    AALIGN(4)
    AGLOBAL(core_exc_entry)
ALABEL(core_exc_entry)
#ifdef USE_RISCV_DIRECT_TRAP
    csrr t0, mscratch
#endif /* USE_RISCV_DIRECT_TRAP */
     /*
      * ターゲット依存の追加レジスタの保存
      */
    save_additional_regs_int
#ifdef __riscv_flen
    addi sp, sp, -(TNUM_RISCV_INT_FREG * FREG_SIZE)
    sf   ft0,   0*FREG_SIZE(sp)  /* f0  */
    sf   ft1,   1*FREG_SIZE(sp)  /* f1  */
    sf   ft2,   2*FREG_SIZE(sp)  /* f2  */
    sf   ft3,   3*FREG_SIZE(sp)  /* f3  */
    sf   ft4,   4*FREG_SIZE(sp)  /* f4  */
    sf   ft5,   5*FREG_SIZE(sp)  /* f5  */
    sf   ft6,   6*FREG_SIZE(sp)  /* f6  */
    sf   ft7,   7*FREG_SIZE(sp)  /* f7  */
    sf   fa0,   8*FREG_SIZE(sp)  /* f10 */
    sf   fa1,   9*FREG_SIZE(sp)  /* f11 */
    sf   fa2,  10*FREG_SIZE(sp)  /* f12 */
    sf   fa3,  11*FREG_SIZE(sp)  /* f13 */
    sf   fa4,  12*FREG_SIZE(sp)  /* f14 */
    sf   fa5,  13*FREG_SIZE(sp)  /* f15 */
    sf   fa6,  14*FREG_SIZE(sp)  /* f16 */
    sf   fa7,  15*FREG_SIZE(sp)  /* f17 */
    sf   ft8,  16*FREG_SIZE(sp)  /* f28 */
    sf   ft9,  17*FREG_SIZE(sp)  /* f29 */
    sf   ft10, 18*FREG_SIZE(sp)  /* f30 */
    sf   ft11, 19*FREG_SIZE(sp)  /* f31 */
#endif /* __riscv_flen */
    addi sp, sp, -(TNUM_RISCV_INT_XREG * XREG_SIZE)
    sx   ra,  0*XREG_SIZE(sp)  /* x1  */
    sx   tp,  1*XREG_SIZE(sp)  /* x4  */
    sx   t0,  2*XREG_SIZE(sp)  /* x5  */
    sx   t1,  3*XREG_SIZE(sp)  /* x6  */
    sx   t2,  4*XREG_SIZE(sp)  /* x7  */
    sx   a0,  5*XREG_SIZE(sp)  /* x10 */
    sx   a1,  6*XREG_SIZE(sp)  /* x11 */
    sx   a2,  7*XREG_SIZE(sp)  /* x12 */
    sx   a3,  8*XREG_SIZE(sp)  /* x13 */
    sx   a4,  9*XREG_SIZE(sp)  /* x14 */
    sx   a5, 10*XREG_SIZE(sp)  /* x15 */
    csrr t0, mepc
    sx   t0, 11*XREG_SIZE(sp)
    csrr t0, mstatus
    sx   t0, 12*XREG_SIZE(sp)
#ifndef __riscv_32e
    sx   t3, 13*XREG_SIZE(sp)  /* x28 */
    sx   t4, 14*XREG_SIZE(sp)  /* x29 */
    sx   t5, 15*XREG_SIZE(sp)  /* x30 */
    sx   t6, 16*XREG_SIZE(sp)  /* x31 */
    sx   a6, 17*XREG_SIZE(sp)  /* x16 */
    sx   a7, 18*XREG_SIZE(sp)  /* x17 */
#endif /* !__riscv_32e */

    /*
     *  CPU例外が発生した状況の判断に用いるために，CPU例外発生前の割
     *  込み優先度マスクと例外ネストカウントをスタックに保存する．
     */
    jal irc_get_intpri

    la     a2, excpt_nest_count
    lw     a1, 0(a2)
    addi   sp, sp, -(2 * 4)
    sw     a0, 0*4(sp)  /* 割込み優先度マスクの保存 */
    sw     a1, 1*4(sp)  /* 例外ネストカウントの保存 */

    mv     a0, sp   /* この時点のスタックポインタのトップをa0に */

    /*
     *  スタックポインタの調整
     */
    andi t0, sp, 0xf
    sub  sp, sp, t0
    addi sp, sp, -16  /* スタックポインタの調整値を保存 */
    sx   t0, 0(sp)

    /*
     *  カーネル管理外のCPU例外か判定する
     *
     *  カーネル管理外のCPU例外は，カーネル実行中，全割込みロック状態，
     *  CPUロック状態，カーネル管理外の割込みハンドラ実行中に発生した
     *  CPU例外である．
     */
     is_kernel_exception_asm t0 t1  /* カーネル管理:none 0, カーネル管理外:0 */
     beqz t0, nk_exc_entry_1        /* カーネル管理外のCPU例外の処理へ */

    /*
     * 【この時点のレジスタ状態】
     *  a0：CPU例外の情報を記憶している領域の先頭番地
     *  a1：excpt_nest_countの値
     *  a2：&excpt_nest_count
     */

    /*
     *  例外ネストカウントをインクリメントする．
     */
    addi t1, a1, 1
    sw   t1, 0(a2)
    bnez a1, core_exc_entry_1  /* CPU例外が非タスクコンテキストで発生 */
                               /*        ならcore_exc_entry_1に分岐 */

    /*
     *  非タスクコンテキスト用のスタックに切り換える．
     */
    mv   t0, sp                 /* この時点のスタックポインタをt0に */
    la   a2, istkpt
    lx   sp, 0(a2)              /* 非タスクコンテキスト用のスタックに */
    addi sp, sp, -16            /* 切換え前のスタックポインタを保存 */
    sx   t0, 0(sp)

ALABEL(core_exc_entry_1)
    /*
     * 【この時点のレジスタ状態】
     *  a0：CPU例外の情報を記憶している領域の先頭番地
     */

    /*
     *  CPU例外の情報を記憶している領域の先頭番地を一旦保存
     */
    addi sp, sp, -(16*2)
    sx   a0, 0*XREG_SIZE(sp)

    /*
     *  （必要なら）割込みコントローラを操作する．
     *
     *  irc_begin_excは，irc_end_excで用いる情報を返す．
     */
    jal irc_begin_exc
    sx  a0, 1*XREG_SIZE(sp)   /* irc_begin_exc の戻り値を保存 */

    /*
     *  CPU例外番号の取得と保存
     */
    csrr a1, mcause           /* CPU例外番号を保存 */
    sx   a1, 2*XREG_SIZE(sp)

    /*
     *  CPUロック解除状態 & 全割込みロック解除状態とする．
     */
    unlock_cpu_and_unlock_allint_asm  t0

    /*
     *  ログ出力の呼出し
     */
#ifdef LOG_EXC_ENTER
    csrr a0, mcause   /* CPU例外番号をパラメータに渡す */
    jal log_exc_enter
    lx   a1, 2*XREG_SIZE(sp)  /* スタックからCPU例外番号を取得 */
#endif /* LOG_EXC_ENTER */

    /*
     *  CPU例外ハンドラの呼出し
     */
    lx   a0, 0*XREG_SIZE(sp)  /* CPU例外の情報を記憶している領域の先頭番地を第1引数に */

    la     t1, exc_table          /* CPU例外ハンドラテーブルの読込み */
    slli   t2, a1, INTPTR_T_SHIFT
    add    t2, t1, t2
    lx     a2, 0(t2)           /* CPU例外ハンドラの番地  */

    jalr   a2

    /*
     *  ログ出力の呼出し
     */
#ifdef LOG_EXC_LEAVE
    lx  a0, 2*XREG_SIZE(sp)    /* CPU例外番号を引数に */
    jal log_exc_leave
#endif /* LOG_EXC_LEAVE */

    /*
     *  カーネル管理の割込みを禁止する．
     */
    lock_cpu_asm t0

    /*
     *  割込みコントローラを操作して，割込み優先度マスクを，CPU例外発
     *  生時の値に設定する．
     */
    lx  a0, 1*XREG_SIZE(sp)   /* irc_begin_exc の戻り値を引数に */
    jal irc_end_exc

    /*
     *  スタックを戻す
     */
    addi sp, sp, (16*2)

    /*
     *  例外ネストカウントをデクリメントする．
     */
    la     a0, excpt_nest_count
    lw     t0, 0(a0)
    addi   t0, t0, -1
    sw     t0, 0(a0)             /* 戻り先が非タスクコンテキスト */
    bnez   t0, core_exc_entry_4  /*     ならcore_exc_entry_4に分岐 */

    /*
     *  タスク用のスタックに戻す．
     */
    lx  sp, 0(sp)

    /*
     *  p_runtskがNULLか判定する．
     */
    la    a0, p_runtsk
    lx    a1, 0(a0)              /* p_runtsk → a1 */
    bnez  a1, core_exc_entry_2   /* p_runtskがNULLでなければ */
                                 /*   core_exc_entry_2に分岐 */

    /*
     *  IDLE処理へのリターン（IDLE処理に割り込んだCPU例外からリターン）
     *  タスクのスタックに保存したスクラッチレジスタ等を捨てる．
     */
    lx   t0, 0(sp)   /* スタックポインタの調整を元に戻す */
    addi sp, sp, 16
    add  sp, sp, t0

    addi sp, sp, (2 * 4)  /* 割込み優先度マスク, 例外ネストカウント保存領域分を戻す */
    addi sp, sp, (TNUM_RISCV_INT_XREG * XREG_SIZE)
#ifdef __riscv_flen
    addi sp, sp, (TNUM_RISCV_INT_FREG * FREG_SIZE)
#endif /* __riscv_flen */
    j    dispatcher_0

ALABEL(core_exc_entry_2)
    /*
     * 【この時点のレジスタ状態】
     *  a1：p_runtsk
     */
    /*
     *  ディスパッチが必要か判定する．
     */
    la   a0, p_schedtsk
    lx   t0, 0(a0)                /* p_schedtsk → t0 */
    beq  t0, a1, core_exc_entry_3 /* p_runtskとp_schedtskが同じなら */
                                  /*             core_exc_entry_3へ */

    /*
     *  コンテキストを保存する．
     *
     * 【この時点のレジスタ状態】
     *  a1：p_runtsk
     */
     /*
      * ターゲット依存の追加レジスタの保存
      */
    save_additional_regs_int_disp
#ifdef __riscv_flen
    addi sp, sp, -(TNUM_RISCV_INT_DISP_FREG * FREG_SIZE)
    sf   fs0,   0*FREG_SIZE(sp)  /* f8 */
    sf   fs1,   1*FREG_SIZE(sp)  /* f9 */
    sf   fs2,   2*FREG_SIZE(sp)  /* f18 */
    sf   fs3,   3*FREG_SIZE(sp)  /* f19 */
    sf   fs4,   4*FREG_SIZE(sp)  /* f20 */
    sf   fs5,   5*FREG_SIZE(sp)  /* f21 */
    sf   fs6,   6*FREG_SIZE(sp)  /* f22 */
    sf   fs7,   7*FREG_SIZE(sp)  /* f23 */
    sf   fs8,   8*FREG_SIZE(sp)  /* f24 */
    sf   fs9,   9*FREG_SIZE(sp)  /* f25 */
    sf   fs10, 10*FREG_SIZE(sp)  /* f26 */
    sf   fs11, 11*FREG_SIZE(sp)  /* f27 */
#endif /* __riscv_flen */
    addi sp, sp, -(TNUM_RISCV_INT_DISP_XREG * XREG_SIZE)
    sx   s0,   0*XREG_SIZE(sp)  /*  x8 */
    sx   s1,   1*XREG_SIZE(sp)  /*  x9 */
#ifndef __riscv_32e
    sx   s2,   2*XREG_SIZE(sp)  /* x18 */
    sx   s3,   3*XREG_SIZE(sp)  /* x19 */
    sx   s4,   4*XREG_SIZE(sp)  /* x20 */
    sx   s5,   5*XREG_SIZE(sp)  /* x21 */
    sx   s6,   6*XREG_SIZE(sp)  /* x22 */
    sx   s7,   7*XREG_SIZE(sp)  /* x23 */
    sx   s8,   8*XREG_SIZE(sp)  /* x24 */
    sx   s9,   9*XREG_SIZE(sp)  /* x25 */
    sx   s10, 10*XREG_SIZE(sp)  /* x26 */
    sx   s11, 11*XREG_SIZE(sp)  /* x27 */
#endif /* !__riscv_32e */

    mv a0, a1
    sx sp, TCB_sp(a0)  /* スタックポインタを保存 */
    la t0, ret_exc_r   /* 実行再開番地を保存 */
    sx t0, TCB_pc(a0)
    j  dispatcher      /* a0にはp_runtskが格納されている */

ALABEL(ret_exc_r)
    /*
     *  コンテキストを復帰する．
     *
     * 【この時点のレジスタ状態】
     *  a0：p_runtsk（タスク切換え後）
     */
    lx   s0,   0*XREG_SIZE(sp)  /* x8  */
    lx   s1,   1*XREG_SIZE(sp)  /* x9  */
#ifndef __riscv_32e
    lx   s2,   2*XREG_SIZE(sp)  /* x18 */
    lx   s3,   3*XREG_SIZE(sp)  /* x19 */
    lx   s4,   4*XREG_SIZE(sp)  /* x20 */
    lx   s5,   5*XREG_SIZE(sp)  /* x21 */
    lx   s6,   6*XREG_SIZE(sp)  /* x22 */
    lx   s7,   7*XREG_SIZE(sp)  /* x23 */
    lx   s8,   8*XREG_SIZE(sp)  /* x24 */
    lx   s9,   9*XREG_SIZE(sp)  /* x25 */
    lx   s10, 10*XREG_SIZE(sp)  /* x26 */
    lx   s11, 11*XREG_SIZE(sp)  /* x27 */
#endif /* !__riscv_32e */
    addi sp, sp, (TNUM_RISCV_INT_DISP_XREG * XREG_SIZE)
#ifdef __riscv_flen
    lf   fs0,   0*FREG_SIZE(sp)  /* f8  */
    lf   fs1,   1*FREG_SIZE(sp)  /* f9  */
    lf   fs2,   2*FREG_SIZE(sp)  /* f18 */
    lf   fs3,   3*FREG_SIZE(sp)  /* f19 */
    lf   fs4,   4*FREG_SIZE(sp)  /* f20 */
    lf   fs5,   5*FREG_SIZE(sp)  /* f21 */
    lf   fs6,   6*FREG_SIZE(sp)  /* f22 */
    lf   fs7,   7*FREG_SIZE(sp)  /* f23 */
    lf   fs8,   8*FREG_SIZE(sp)  /* f24 */
    lf   fs9,   9*FREG_SIZE(sp)  /* f25 */
    lf   fs10, 10*FREG_SIZE(sp)  /* f26 */
    lf   fs11, 11*FREG_SIZE(sp)  /* f27 */
    addi sp, sp, (TNUM_RISCV_INT_DISP_FREG * FREG_SIZE)
#endif /* __riscv_flen */
     /*
      * ターゲット依存の追加レジスタの復帰
      */
    restore_additional_regs_int_disp

ALABEL(core_exc_entry_3)
    /*
     *  CPU例外処理からのリターン
     *
     *  CPU例外処理からのリターンにより，CPUロック解除状態に遷移するよ
     *  うにする必要がある．
     */
ALABEL(core_exc_entry_4)
    /*
     *  CPU例外処理からのリターンにより
     *  CPUロック解除状態になるように設定
     */
    ret_exc_prepare_unlock_cpu_asm t0

    lx   t0, 0(sp)   /* スタックポインタの調整を元に戻す */
    addi sp, sp, 16
    add  sp, sp, t0

    addi sp, sp, (2 * 4) /* 割込み優先度マスク, 例外ネストカウント保存領域分を戻す */

    lx   t0, 11*XREG_SIZE(sp)
    csrw mepc,  t0
    lx   t0, 12*XREG_SIZE(sp)
    csrw mstatus, t0
    lx   ra,  0*XREG_SIZE(sp)  /* x1  */
    lx   tp,  1*XREG_SIZE(sp)  /* x4  */
    lx   t0,  2*XREG_SIZE(sp)  /* x5  */
    lx   t1,  3*XREG_SIZE(sp)  /* x6  */
    lx   t2,  4*XREG_SIZE(sp)  /* x7  */
    lx   a0,  5*XREG_SIZE(sp)  /* x10 */
    lx   a1,  6*XREG_SIZE(sp)  /* x11 */
    lx   a2,  7*XREG_SIZE(sp)  /* x12 */
    lx   a3,  8*XREG_SIZE(sp)  /* x13 */
    lx   a4,  9*XREG_SIZE(sp)  /* x14 */
    lx   a5, 10*XREG_SIZE(sp)  /* x15 */
#ifndef __riscv_32e
    lx   t3, 13*XREG_SIZE(sp)  /* x28 */
    lx   t4, 14*XREG_SIZE(sp)  /* x29 */
    lx   t5, 15*XREG_SIZE(sp)  /* x30 */
    lx   t6, 16*XREG_SIZE(sp)  /* x31 */
    lx   a6, 17*XREG_SIZE(sp)  /* x16 */
    lx   a7, 18*XREG_SIZE(sp)  /* x17 */
#endif /* !__riscv_32e */
    addi sp, sp, (TNUM_RISCV_INT_XREG * XREG_SIZE)
#ifdef __riscv_flen
    lf   ft0,   0*FREG_SIZE(sp)  /* f0  */
    lf   ft1,   1*FREG_SIZE(sp)  /* f1  */
    lf   ft2,   2*FREG_SIZE(sp)  /* f2  */
    lf   ft3,   3*FREG_SIZE(sp)  /* f3  */
    lf   ft4,   4*FREG_SIZE(sp)  /* f4  */
    lf   ft5,   5*FREG_SIZE(sp)  /* f5  */
    lf   ft6,   6*FREG_SIZE(sp)  /* f6  */
    lf   ft7,   7*FREG_SIZE(sp)  /* f7  */
    lf   fa0,   8*FREG_SIZE(sp)  /* f10 */
    lf   fa1,   9*FREG_SIZE(sp)  /* f11 */
    lf   fa2,  10*FREG_SIZE(sp)  /* f12 */
    lf   fa3,  11*FREG_SIZE(sp)  /* f13 */
    lf   fa4,  12*FREG_SIZE(sp)  /* f14 */
    lf   fa5,  13*FREG_SIZE(sp)  /* f15 */
    lf   fa6,  14*FREG_SIZE(sp)  /* f16 */
    lf   fa7,  15*FREG_SIZE(sp)  /* f17 */
    lf   ft8,  16*FREG_SIZE(sp)  /* f28 */
    lf   ft9,  17*FREG_SIZE(sp)  /* f29 */
    lf   ft10, 18*FREG_SIZE(sp)  /* f30 */
    lf   ft11, 19*FREG_SIZE(sp)  /* f31 */
    addi sp, sp, (TNUM_RISCV_INT_FREG * FREG_SIZE)
#endif /* __riscv_flen */
     /*
      * ターゲット依存の追加レジスタの復帰
      */
    restore_additional_regs_int
    mret

/*
 *  カーネル管理外のCPU例外の出入口処理
 */
ALABEL(nk_exc_entry_1)
    /*
     * 【この時点のレジスタ状態】
     *  a0：CPU例外の情報を記憶している領域の先頭番地
     *  a1：excpt_nest_countの値
     *  a2：&excpt_nest_count
     */

    /*
     *  例外ネストカウントをインクリメントする．
     */
    addi    t1, a1, 1
    sw      t1, 0(a2)
    bnez    a1, nk_exc_entry_2  /* CPU例外が非タスクコンテキストで発生 */
                                /*        ならnk_exc_entry_2に分岐 */

    /*
     *  非タスクコンテキスト用のスタックに切り換える．
     */
    mv      t0, sp                 /* この時点のスタックポインタをt0に */
    la      a2, istkpt
    lx      sp, 0(a2)              /* 非タスクコンテキスト用のスタックに */
    addi    sp, sp, -16            /* 切換え前のスタックポインタを保存 */
    sx      t0, 0(sp)

ALABEL(nk_exc_entry_2)
    /*
     *  システム状態（コンテキストは除く）を，CPU例外発生時の状態へ
     */

    /*
     *  CPU例外ハンドラの呼出し
     */
    csrr   a1, mcause  /* CPU例外番号を第2引数に */
    la     t1, exc_table          /* CPU例外ハンドラテーブルの読込み */
    slli   t2, a1, INTPTR_T_SHIFT
    add    t2, t1, t2
    lx     a2, 0(t2)           /* CPU例外ハンドラの番地  */

    jalr   a2

    /*
     *  例外ネストカウントをデクリメントする．
     */
    la     a0, excpt_nest_count
    lw     t0, 0(a0)
    addi   t0, t0, -1
    sw     t0, 0(a0)             /* 戻り先が非タスクコンテキスト */
    bnez   t0, core_exc_entry_4  /*   ならcore_exc_entry_4に分岐 */

    /*
     *  タスク用のスタックに戻す．
     */
    lx  sp, 0(sp)

    j   core_exc_entry_4

/*
 * 微少時間待ち
 */
    AALIGN(4)
    ATEXT
    AGLOBAL(sil_dly_nse)
ALABEL(sil_dly_nse)
    addi a0, a0, -SIL_DLY_TIM1
    bgtz a0, sil_dly_nse1
    jr   ra
ALABEL(sil_dly_nse1)
    addi a0, a0, -SIL_DLY_TIM2
    bgtz a0, sil_dly_nse1
    jr   ra
