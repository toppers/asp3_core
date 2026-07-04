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
 *  ESP32-C6のハードウェア資源の定義
 *
 *  レジスタアドレスはesp-hal-3rdparty（asp3_esp_idf/hal submodule，
 *  components/soc/esp32c6/register/soc/）から採用．経緯・実機ログは
 *  docs/dev/esp32c6-target.md．
 *
 *  ■ 割込みコントローラについて（重要な訂正）
 *  Phase A第1マイルストーンの調査時点ではCLIC（Espressif独自の非標準
 *  CLIC）を使うと想定していたが，実際にはESP32-C6のHPコアは
 *  soc_caps.hで SOC_INT_CLIC_SUPPORTED を定義していない（CLIC対応は
 *  C5/C61/H4/P4rev2+/S31のみ）．C6は SOC_INT_PLIC_SUPPORTED を定義し，
 *  かつそのレジスタ実体（plic_reg.h）は「PLIC」と名付けられているが
 *  実際にはC3の割込みマトリクス（INTMTX）と同一のCPU割込み線制御方式
 *  （ENABLE／TYPE／CLEAR／EIP_STATUS／PRI×32／THRESHの単純なメモリ
 *  マップトレジスタ）である（Espressifの命名変更のみ．標準RISC-V
 *  PLIC＝claim/completeレジスタ方式ではない）。ソース→CPU割込み線の
 *  ルーティングもC3と全く同じ機構（INTMTX_BASE + 4*source への書込み）．
 *  mtvecモードもMTVEC_MODE_CSR=1＝C3と同じ標準RISC-Vベクタドモード．
 *  （esp-hal-3rdparty: riscv/interrupt_plic.c・
 *  hal/include/hal/interrupt_plic_ll.h・soc/esp32c6/register/soc/
 *  plic_reg.h・interrupt_matrix_reg.hで確認済み）
 *
 *  したがってintmtx_kernel_impl.hはC3のものをほぼそのまま流用でき，
 *  レジスタオフセットの差し替えのみで足りる（新規の割込み方式の実装
 *  ではない）．
 */

#ifndef TOPPERS_ESP32C6_H
#define TOPPERS_ESP32C6_H

/*
 *  メモリマップ（Direct Bootでの配置．リンカスクリプトと一致させること）
 *
 *  C3と異なり，C6はSOC_DROM_LOW＝SOC_IROM_LOW（共に0x42000000）＝
 *  IROM/DROMが分離していない．そのためC3のVMA==LMAトリック（IROM/DROM
 *  分離への対処）は不要で，.text／.rodataとも単一のFLASH領域に素直に
 *  配置できる（C3のリンカスクリプトより単純）．
 */
#define ESP32C6_IROM_BASE       0x42000000
#define ESP32C6_DROM_BASE       0x42000000  /* IROMと同一（C3と異なる点） */
#define ESP32C6_DRAM_BASE       0x40800000
#define ESP32C6_DRAM_SIZE       0x00080000  /* 512KB */

/*
 *  CPUクロック周波数（MHz．core_syssvc.hの性能カウンタ換算等で使用）
 *
 *  本マイルストーンではリセット既定クロックのまま動作確認した
 *  （PCR経由のPLL切替は未実施．周波数未計測につき，ひとまずXTAL
 *  そのまま＝40MHzと仮定．dlynse較正は別途実施）．
 */
#define CORE_CLK_MHZ            40

/*
 *  微少時間待ちのための定義（nsec単位．未較正の暫定値．dlynse較正は
 *  クロック確定後に実施する）
 */
#define SIL_DLY_TIM1    100
#define SIL_DLY_TIM2    100

/*
 *  ペリフェラルのベースアドレス（実機検証済み）
 */
#define ESP32C6_INTMTX_BASE     0x60010000  /* 割込みマトリクス（ソースルーティング） */
#define ESP32C6_PLIC_MX_BASE    0x20001000  /* CPU割込み線制御（Espressif呼称"PLIC"．
                                             実体はC3のINTMTX CPU側と同じ方式） */
#define ESP32C6_SYSTIMER_BASE   0x6000A000  /* システムタイマ */
#define ESP32C6_USBJTAG_BASE    0x6000F000  /* USB Serial/JTAGコントローラ */
#define ESP32C6_UART0_BASE      0x60000000  /* UART0（ネイティブUSBボードでは
                                             未配線．USBJTAGを使うこと） */
#define ESP32C6_TIMG0_BASE      0x60008000  /* タイマグループ0（MWDT） */
#define ESP32C6_TIMG1_BASE      0x60009000  /* タイマグループ1（MWDT） */
#define ESP32C6_LP_WDT_BASE     0x600B1C00  /* 低電力ドメインWDT（C3のRTC_CNTL相当） */
#define ESP32C6_PCR_BASE        0x60096000  /* Peripheral Clock and Reset */
#define ESP32C6_INTPRI_BASE     0x600C5000  /* ソフトウェア割込み（FROM_CPU_n）等．
                                             C3のSYSTEM_BASEのFROM_CPU相当部分 */

/*
 *  ソフトウェア割込み（ras_int／clr_int用．C3のSYSTEM_CPU_INTR_FROM_CPU_n
 *  相当．INTPRIペリフェラル内）
 */
#define ESP32C6_INTPRI_CPU_INTR_FROM_CPU_0  (ESP32C6_INTPRI_BASE + 0x90)
#define ESP32C6_INTPRI_CPU_INTR_FROM_CPU_1  (ESP32C6_INTPRI_BASE + 0x94)
#define ESP32C6_INTPRI_CPU_INTR_FROM_CPU_2  (ESP32C6_INTPRI_BASE + 0x98)
#define ESP32C6_INTPRI_CPU_INTR_FROM_CPU_3  (ESP32C6_INTPRI_BASE + 0x9c)

/*
 *  PCRレジスタ（CPUクロック設定．次マイルストーンで使用）
 *
 *  PCR_SYSCLK_CONF：bit[17:16] SOC_CLK_SEL（0=XTAL/1=SPLL/2=FOSC）
 *  PCR_CPU_FREQ_CONF：bit[7:0] CPU_LS_DIV_NUM・bit[15:8] CPU_HS_DIV_NUM・
 *                     bit16 CPU_HS_120M_FORCE
 *
 *  実機未検証（SPLL起動シーケンスも含め要調査．esp32c6.h時点では
 *  レジスタアドレスの記録のみ）．
 */
#define ESP32C6_PCR_SYSCLK_CONF         (ESP32C6_PCR_BASE + 0x110)
#define ESP32C6_PCR_CPU_FREQ_CONF       (ESP32C6_PCR_BASE + 0x118)
#define ESP32C6_PCR_SYSCLK_CONF_SEL_MASK  (3U << 16)
#define ESP32C6_PCR_SYSCLK_CONF_SEL_SPLL  (1U << 16)
#define ESP32C6_PCR_CPU_FREQ_CONF_HS_120M_FORCE  (1U << 16)

/*
 *  割込みソース番号（割込みマトリクスへの入力．esp-hal-3rdpartyの
 *  soc/esp32c6/include/soc/interrupts.h＝ETS_*_INTR_SOURCE enumの
 *  実数値．実機での動作確認要）
 */
#define ESP32C6_INTSRC_UART0             43
#define ESP32C6_INTSRC_USB_SERIAL_JTAG   48
#define ESP32C6_INTSRC_SYSTIMER_TARGET0  57
#define ESP32C6_INTSRC_FROM_CPU_0        22
#define ESP32C6_INTSRC_FROM_CPU_1        23
#define ESP32C6_INTSRC_FROM_CPU_2        24
#define ESP32C6_INTSRC_FROM_CPU_3        25
#define ESP32C6_TNUM_INTSRC              77

/*
 *  SYSTIMERレジスタ（unit0＋target0のみ使用．C3と同一レイアウト＝
 *  ベースアドレスのみ異なる．クロックはXTAL 40MHz÷2.5＝16MHz固定・
 *  52bitカウンタの想定．実機での動作確認要）
 */
#define ESP32C6_SYSTIMER_CONF           (ESP32C6_SYSTIMER_BASE + 0x00)
#define ESP32C6_SYSTIMER_UNIT0_OP       (ESP32C6_SYSTIMER_BASE + 0x04)
#define ESP32C6_SYSTIMER_TARGET0_HI     (ESP32C6_SYSTIMER_BASE + 0x1C)
#define ESP32C6_SYSTIMER_TARGET0_LO     (ESP32C6_SYSTIMER_BASE + 0x20)
#define ESP32C6_SYSTIMER_TARGET0_CONF   (ESP32C6_SYSTIMER_BASE + 0x34)
#define ESP32C6_SYSTIMER_UNIT0_VALUE_HI (ESP32C6_SYSTIMER_BASE + 0x40)
#define ESP32C6_SYSTIMER_UNIT0_VALUE_LO (ESP32C6_SYSTIMER_BASE + 0x44)
#define ESP32C6_SYSTIMER_COMP0_LOAD     (ESP32C6_SYSTIMER_BASE + 0x50)
#define ESP32C6_SYSTIMER_INT_ENA        (ESP32C6_SYSTIMER_BASE + 0x64)
#define ESP32C6_SYSTIMER_INT_RAW        (ESP32C6_SYSTIMER_BASE + 0x68)
#define ESP32C6_SYSTIMER_INT_CLR        (ESP32C6_SYSTIMER_BASE + 0x6C)
#define ESP32C6_SYSTIMER_INT_ST         (ESP32C6_SYSTIMER_BASE + 0x70)

#define ESP32C6_SYSTIMER_CONF_UNIT0_WORK_EN    (1U << 30)
#define ESP32C6_SYSTIMER_CONF_TARGET0_WORK_EN  (1U << 24)
#define ESP32C6_SYSTIMER_OP_UPDATE             (1U << 30)
#define ESP32C6_SYSTIMER_OP_VALUE_VALID        (1U << 29)
#define ESP32C6_SYSTIMER_TARGET0_PERIOD_MODE   (1U << 30)
#define ESP32C6_SYSTIMER_INT_TARGET0           (1U << 0)
#define ESP32C6_SYSTIMER_TICKS_PER_US   16U

/*
 *  USB Serial/JTAGレジスタ（C3と同一レイアウト．ベースアドレスのみ
 *  異なる＝esp32c3_usbjtag.hのESP32C3_USBJTAG_*と同じオフセット．
 *  実機検証済み＝Phase A第1マイルストーン）
 */
#define ESP32C6_USBJTAG_EP1(base)		((uint32_t *)((base) + 0x00U))
#define ESP32C6_USBJTAG_EP1_CONF(base)	((uint32_t *)((base) + 0x04U))
#define ESP32C6_USBJTAG_EP1_CONF_WR_DONE		UINT_C(0x00000001)
#define ESP32C6_USBJTAG_EP1_CONF_IN_DATA_FREE	UINT_C(0x00000002)
#define ESP32C6_USBJTAG_EP1_CONF_OUT_DATA_AVAIL	UINT_C(0x00000004)

/*
 *  ウォッチドッグタイマ（実機検証済み．無効化しないと数秒以内に
 *  TG0_WDT_HPSYSリセットが掛かる＝実機で確認済み）
 *
 *  TIMG_WDT_WKEY・LP_WDT_WDT_WKEYはC3と同じ0x50D83AA1で解錠できる
 *  （esp-hal-3rdpartyのtimer_group_reg.hはdefault値としてこの値を
 *  記載．lp_wdt_reg.hは自動生成ドキュメント欠落＝"need_des"で
 *  default記載なしだったが，実機でC3と同じ値により無効化成功を確認）．
 *  SWD（スーパーWDT）もC3と同じ0x8F1D312Aで解錠成功を実機確認済み．
 */
#define ESP32C6_TIMG_WDTCONFIG0(base)   ((base) + 0x48)
#define ESP32C6_TIMG_WDTWPROTECT(base)  ((base) + 0x64)
#define ESP32C6_TIMG_WDT_WKEY           0x50D83AA1U

#define ESP32C6_RTC_CNTL_WDTCONFIG0     ESP32C6_LP_WDT_CONFIG0
#define ESP32C6_RTC_CNTL_WDTWPROTECT    ESP32C6_LP_WDT_WPROTECT
#define ESP32C6_RTC_CNTL_WDT_WKEY       ESP32C6_LP_WDT_WDT_WKEY
#define ESP32C6_RTC_CNTL_SWD_CONF       ESP32C6_LP_WDT_SWD_CONFIG
#define ESP32C6_RTC_CNTL_SWD_WPROTECT   ESP32C6_LP_WDT_SWD_WPROTECT
#define ESP32C6_RTC_CNTL_SWD_WKEY       ESP32C6_LP_WDT_SWD_WKEY
#define ESP32C6_RTC_CNTL_SWD_AUTO_FEED_EN  (1U << 18)

#define ESP32C6_LP_WDT_CONFIG0          (ESP32C6_LP_WDT_BASE + 0x00)
#define ESP32C6_LP_WDT_WPROTECT         (ESP32C6_LP_WDT_BASE + 0x18)
#define ESP32C6_LP_WDT_WDT_WKEY         0x50D83AA1U
#define ESP32C6_LP_WDT_SWD_CONFIG       (ESP32C6_LP_WDT_BASE + 0x1c)
#define ESP32C6_LP_WDT_SWD_WPROTECT     (ESP32C6_LP_WDT_BASE + 0x20)
#define ESP32C6_LP_WDT_SWD_WKEY         0x8F1D312AU
#define ESP32C6_LP_WDT_SWD_AUTO_FEED_EN (1U << 18)

#endif /* TOPPERS_ESP32C6_H */
