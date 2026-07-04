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
 *  ESP32-C6のハードウェア資源の定義（実機rev v0.2で検証済みの範囲のみ．
 *  Phase A第1マイルストーン＝boot＋WDT無効化＋polled USB Serial/JTAG
 *  コンソールのみ．割込み（CLIC）・PLL切替（PCR）は未実装＝別途）
 *
 *  レジスタアドレスはesp-hal-3rdparty（asp3_esp_idf/hal submodule，
 *  components/soc/esp32c6/register/soc/）から採用．経緯・実機ログは
 *  docs/dev/esp32c6-target.md．
 */

#ifndef TOPPERS_ESP32C6_H
#define TOPPERS_ESP32C6_H

/*
 *  メモリマップ（Direct Bootでの配置．リンカスクリプトと一致させること）
 *
 *  C3と異なり，C6はSOC_DROM_LOW＝SOC_IROM_LOW（共に0x42000000）＝
 *  IROM/DROMが分離していない．そのためVMA==LMAトリックは不要で，
 *  .text／.rodataとも単一のFLASH領域に素直に配置できる（C3のリンカ
 *  スクリプトより単純）．
 */
#define ESP32C6_IROM_BASE       0x42000000
#define ESP32C6_DROM_BASE       0x42000000  /* IROMと同一（C3と異なる点） */
#define ESP32C6_DRAM_BASE       0x40800000
#define ESP32C6_DRAM_SIZE       0x00080000  /* 512KB */

/*
 *  ペリフェラルのベースアドレス（実機検証済み）
 */
#define ESP32C6_USBJTAG_BASE    0x6000F000  /* USB Serial/JTAGコントローラ */
#define ESP32C6_UART0_BASE      0x60000000  /* UART0（ネイティブUSBボードでは
                                             未配線．USBJTAGを使うこと） */
#define ESP32C6_TIMG0_BASE      0x60008000  /* タイマグループ0（MWDT） */
#define ESP32C6_TIMG1_BASE      0x60009000  /* タイマグループ1（MWDT） */
#define ESP32C6_LP_WDT_BASE     0x600B1C00  /* 低電力ドメインWDT（C3のRTC_CNTL相当） */
#define ESP32C6_PCR_BASE        0x60096000  /* Peripheral Clock and Reset（未使用．
                                             CPU PLLクロック切替は次マイルストーン） */

/*
 *  USB Serial/JTAGレジスタ（C3と同一レイアウト．ベースアドレスのみ
 *  異なる＝esp32c3_usbjtag.hのESP32C3_USBJTAG_*と同じオフセット）
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

#define ESP32C6_LP_WDT_CONFIG0          (ESP32C6_LP_WDT_BASE + 0x00)
#define ESP32C6_LP_WDT_WPROTECT         (ESP32C6_LP_WDT_BASE + 0x18)
#define ESP32C6_LP_WDT_WDT_WKEY         0x50D83AA1U
#define ESP32C6_LP_WDT_SWD_CONFIG       (ESP32C6_LP_WDT_BASE + 0x1c)
#define ESP32C6_LP_WDT_SWD_WPROTECT     (ESP32C6_LP_WDT_BASE + 0x20)
#define ESP32C6_LP_WDT_SWD_WKEY         0x8F1D312AU
#define ESP32C6_LP_WDT_SWD_AUTO_FEED_EN (1U << 18)

/*
 *  ---- 以下，未実装・未検証（次マイルストーン以降） ----
 *
 *  CPUクロック：リセット既定のXTALクロックのまま動作確認した
 *  （dlynse較正等は未実施）．PLLへの切替はPCR（DR_REG_PCR_BASE＝
 *  0x60096000．PCR_SYSCLK_CONF_REG=+0x110・PCR_CPU_FREQ_CONF_REG=
 *  +0x118）で行う想定だが，SPLL自体の起動シーケンスも含め要調査．
 *
 *  割込み：CLIC（C3のINTMTXとは別方式．MTVT_CSR=0x307・
 *  MINTTHRESH_CSR=0x347・MINTSTATUS_CSR=0xFB1，ハードウェアベクタ
 *  ディスパッチ）．本ファイルでは一切定義しない．
 */

#endif /* TOPPERS_ESP32C6_H */
