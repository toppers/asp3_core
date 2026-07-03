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
 *  ESP32-C3のハードウェア資源の定義
 *
 *  レジスタアドレス・ビット定義はESP-IDFのレジスタヘッダ
 *  （components/soc/esp32c3/register/soc/）およびESP32-C3 TRMから
 *  抜粋・整理したもの（本プロジェクトで使用するものだけを定義する）．
 *
 *  UART0のレジスタ定義はesp32c3_uart.hにある．
 */

#ifndef TOPPERS_ESP32C3_H
#define TOPPERS_ESP32C3_H

/*
 *  メモリマップ（Direct Bootでの配置．リンカスクリプトと一致させること）
 *    IROM：フラッシュの命令バスマッピング（キャッシュ経由・XIP実行）
 *    DROM：フラッシュのデータバスマッピング（キャッシュ経由・.rodata）
 *    DRAM：内部SRAMのデータバス側（.data／.bss／スタック）
 */
#define ESP32C3_IROM_BASE       0x42000000
#define ESP32C3_DROM_BASE       0x3C000000
#define ESP32C3_DRAM_BASE       0x3FC80000
#define ESP32C3_DRAM_SIZE       0x00050000  /* 320KB（ROMの作業領域を避ける） */

/*
 *  CPUクロック周波数（MHz．core_syssvc.hの性能カウンタ換算等で使用）
 *
 *  Direct Boot経路（二段ブートローダなし）ではリセット既定のXTAL/2＝
 *  20MHzで起動するため，ターゲット依存部のhardware_init_hookでPLL
 *  （480MHz）系の160MHzへ切り替える（実機dlynse計測で確認済み）．
 */
#define CORE_CLK_MHZ            160

/*
 *  微少時間待ちのための定義（nsec単位）
 *
 *  実機（160MHz・XIP実行）のdlynse計測による較正値：呼出しオーバ
 *  ヘッド≈43ns・ループ1回=25ns（4サイクル@160MHz）．実際の所要時間を
 *  下回らない値に設定する．QEMUは実時間の較正に対応しないため，QEMU
 *  でのdlynseは対象外．
 */
#define SIL_DLY_TIM1    40
#define SIL_DLY_TIM2    25

/*
 *  ペリフェラルのベースアドレス
 */
#define ESP32C3_SYSTEM_BASE     0x600C0000  /* システムレジスタ */
#define ESP32C3_INTMTX_BASE     0x600C2000  /* 割込みマトリクス（INTC） */
#define ESP32C3_SYSTIMER_BASE   0x60023000  /* システムタイマ */
#define ESP32C3_UART0_BASE      0x60000000  /* UART0 */
#define ESP32C3_USBJTAG_BASE    0x60043000  /* USB Serial/JTAGコントローラ */
#define ESP32C3_RTCCNTL_BASE    0x60008000  /* RTCコントローラ（WDT含む） */
#define ESP32C3_TIMG0_BASE      0x6001F000  /* タイマグループ0（MWDT） */
#define ESP32C3_TIMG1_BASE      0x60020000  /* タイマグループ1（MWDT） */

/*
 *  SYSTEMレジスタ（CPUクロック設定）
 *
 *  CPU_PER_CONF：bit[1:0] CPUPERIOD_SEL（PLL選択時 0=80MHz／1=160MHz），
 *                bit2 PLL_FREQ_SEL（1=PLL 480MHz．リセット既定1）
 *  SYSCLK_CONF ：bit[11:10] SOC_CLK_SEL（0=XTAL／1=PLL），
 *                bit[9:0] PRE_DIV_CNT（XTAL選択時の分周-1．リセット
 *                既定1＝XTAL 40MHz/2＝20MHz）
 */
#define ESP32C3_SYSTEM_CPU_PER_CONF     (ESP32C3_SYSTEM_BASE + 0x008)
#define ESP32C3_SYSTEM_SYSCLK_CONF      (ESP32C3_SYSTEM_BASE + 0x058)
#define ESP32C3_CPU_PER_CONF_PLL_160M   0x00000005U  /* PLL_FREQ_SEL|SEL=1 */
#define ESP32C3_CPU_PER_CONF_CLK_MASK   0x00000007U
#define ESP32C3_SYSCLK_CONF_SEL_PLL     (1U << 10)
#define ESP32C3_SYSCLK_CONF_SEL_MASK    (3U << 10)

/*
 *  SYSTEMレジスタ（割込みの強制発生に使用）
 *
 *  CPU_INTR_FROM_CPU_n：bit0に1を書くとレベル割込みソース
 *  （ソース番号50〜53）がアサートされ，0を書くまで保持される．
 */
#define ESP32C3_SYSTEM_CPU_INTR_FROM_CPU_0  (ESP32C3_SYSTEM_BASE + 0x028)
#define ESP32C3_SYSTEM_CPU_INTR_FROM_CPU_1  (ESP32C3_SYSTEM_BASE + 0x02C)
#define ESP32C3_SYSTEM_CPU_INTR_FROM_CPU_2  (ESP32C3_SYSTEM_BASE + 0x030)
#define ESP32C3_SYSTEM_CPU_INTR_FROM_CPU_3  (ESP32C3_SYSTEM_BASE + 0x034)

/*
 *  割込みソース番号（割込みマトリクスへの入力．interrupts.hより）
 */
#define ESP32C3_INTSRC_UART0             21
#define ESP32C3_INTSRC_USB_SERIAL_JTAG   26
#define ESP32C3_INTSRC_SYSTIMER_TARGET0  37
#define ESP32C3_INTSRC_FROM_CPU_0        50
#define ESP32C3_INTSRC_FROM_CPU_1        51
#define ESP32C3_INTSRC_FROM_CPU_2        52
#define ESP32C3_INTSRC_FROM_CPU_3        53
#define ESP32C3_TNUM_INTSRC              62

/*
 *  SYSTIMERレジスタ（unit0＋target0のみ使用．クロックは
 *  XTAL 40MHz÷2.5＝16MHz固定・52bitカウンタ）
 */
#define ESP32C3_SYSTIMER_CONF           (ESP32C3_SYSTIMER_BASE + 0x00)
#define ESP32C3_SYSTIMER_UNIT0_OP       (ESP32C3_SYSTIMER_BASE + 0x04)
#define ESP32C3_SYSTIMER_TARGET0_HI     (ESP32C3_SYSTIMER_BASE + 0x1C)
#define ESP32C3_SYSTIMER_TARGET0_LO     (ESP32C3_SYSTIMER_BASE + 0x20)
#define ESP32C3_SYSTIMER_TARGET0_CONF   (ESP32C3_SYSTIMER_BASE + 0x34)
#define ESP32C3_SYSTIMER_UNIT0_VALUE_HI (ESP32C3_SYSTIMER_BASE + 0x40)
#define ESP32C3_SYSTIMER_UNIT0_VALUE_LO (ESP32C3_SYSTIMER_BASE + 0x44)
#define ESP32C3_SYSTIMER_COMP0_LOAD     (ESP32C3_SYSTIMER_BASE + 0x50)
#define ESP32C3_SYSTIMER_INT_ENA        (ESP32C3_SYSTIMER_BASE + 0x64)
#define ESP32C3_SYSTIMER_INT_RAW        (ESP32C3_SYSTIMER_BASE + 0x68)
#define ESP32C3_SYSTIMER_INT_CLR        (ESP32C3_SYSTIMER_BASE + 0x6C)
#define ESP32C3_SYSTIMER_INT_ST         (ESP32C3_SYSTIMER_BASE + 0x70)

#define ESP32C3_SYSTIMER_CONF_UNIT0_WORK_EN    (1U << 30)
#define ESP32C3_SYSTIMER_CONF_TARGET0_WORK_EN  (1U << 24)
#define ESP32C3_SYSTIMER_OP_UPDATE             (1U << 30)
#define ESP32C3_SYSTIMER_OP_VALUE_VALID        (1U << 29)
#define ESP32C3_SYSTIMER_TARGET0_PERIOD_MODE   (1U << 30)
#define ESP32C3_SYSTIMER_INT_TARGET0           (1U << 0)

/*
 *  SYSTIMERのクロック周波数（16MHz固定＝1μsあたり16カウント）
 */
#define ESP32C3_SYSTIMER_TICKS_PER_US   16U

/*
 *  ウォッチドッグタイマ（起動時に無効化する．リセット後デフォルトで
 *  MWDT（TIMG0）とスーパーWDT・RTC WDTが有効なため，無効化しないと
 *  数秒でリブートする）
 */
#define ESP32C3_TIMG_WDTCONFIG0(base)   ((base) + 0x48)
#define ESP32C3_TIMG_WDTWPROTECT(base)  ((base) + 0x64)
#define ESP32C3_TIMG_WDT_WKEY           0x50D83AA1U

#define ESP32C3_RTC_CNTL_WDTCONFIG0     (ESP32C3_RTCCNTL_BASE + 0x090)
#define ESP32C3_RTC_CNTL_WDTWPROTECT    (ESP32C3_RTCCNTL_BASE + 0x0A8)
#define ESP32C3_RTC_CNTL_WDT_WKEY       0x50D83AA1U
#define ESP32C3_RTC_CNTL_SWD_CONF       (ESP32C3_RTCCNTL_BASE + 0x0AC)
#define ESP32C3_RTC_CNTL_SWD_WPROTECT   (ESP32C3_RTCCNTL_BASE + 0x0B0)
#define ESP32C3_RTC_CNTL_SWD_WKEY       0x8F1D312AU
#define ESP32C3_RTC_CNTL_SWD_AUTO_FEED_EN  (1U << 31)

#endif /* TOPPERS_ESP32C3_H */
