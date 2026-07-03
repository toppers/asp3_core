# IMPL_INDEX.md — 実装参照インデックス

> 新ターゲット移植時に「この機能の実装はどのターゲットを参照すればよいか」の索引。  
> `PORTING_GUIDE.md` の各Step実施時に併せて参照する。  
> **新ターゲットで新たな実装パターンを確立したら、この表に追記すること。**

---

## アーキテクチャ依存部（arch/）

| 機能 | 参照arch | ファイル | 備考 |
|---|---|---|---|
| Cortex-M コンテキストスイッチ | `arm_m_gcc/common` | `core_support.S` | PendSV使用 |
| Cortex-M 例外/割込みエントリ | `arm_m_gcc/common` | `core_support.S`, `core_kernel.h` | NVIC |
| Cortex-A コンテキストスイッチ | `arm64_gcc/common` | `core_support.S` | — |
| Cortex-A 例外ベクタ・EL設定 | `arm64_gcc/common` | `core_support.S` | EL1/EL3初期化 |
| RISC-V コンテキストスイッチ | `riscv_gcc/common` | `core_support.S` | mret/csr |
| RISC-V トラップエントリ | `riscv_gcc/common` | `core_support.S`・`polarfire_soc/chip_support.S` | mtvecベクタ＋mcause判定 |
| RISC-V PLIC割込み制御 | `riscv_gcc/common` | `plic_kernel_impl.[ch]`・`polarfire_soc/chip_support.S` | claim/complete・閾値マスク |
| ESP32-C3割込みマトリクス制御 | `riscv_gcc/esp32c3` | `intmtx_kernel_impl.h`・`chip_support.S` | ソースMAP・THRESHソフト昇格（ネスト）・FROM_CPUでras_int |
| RISC-V Machine Timer | `riscv_gcc/common` | `mtimer.[ch]` | CLINT mtime/mtimecmp（MTIはローカル割込み） |

---

## ターゲット依存部（target/）

| 実装したいもの | 参照ターゲット | ファイル | パターン |
|---|---|---|---|
| SysTickによるHRTタイマ | `mps2_an505_gcc` | `target_timer.c` | SysTickダウンカウンタ |
| NVIC割込みプライオリティ設定 | `mps2_an505_gcc` | `target_config.h` | prio_bits考慮 |
| セミホスティングシリアル | `mps2_an505_gcc` | `target_serial.c` | bkpt #0xAB / SYS_WRITEC |
| QEMUクリーン終了 | `syssvc/qemu_exit.c` | — | SYS_EXIT (0x18) |
| Pico SDK統合（タイマ/UART/ブート） | 外部 `asp3_pico_sdk` リポジトリ | （SDK統合版・`ASP3_TARGET_DIR`方式） | add_repeating_timer_us / uart_putc_raw / stdio_init |
| RP2350 RISC-V起動（IMAGE_DEF） | `pico2_riscv_gcc` | `image_def.S` | RISC-V EXE（0x1101）＋ENTRY_POINT item |
| ESP32-C3起動（Direct Boot） | `esp32c3_gcc` | `flash_header.S`・`esp32c3.ld` | マジック0xAEDB041D×2・flash+8エントリ・IROM/DROM二重マップXIP |
| SYSTIMERによるHRT | `esp32c3_gcc` | `target_timer.[ch]` | 16MHz固定・52bit・oneshotコンパレータ・FROM_CPU多重マップで割込み強制 |
| Hazard3 Xh3irq割込み制御 | `arch/riscv_gcc/rp2350` | `xh3irq_kernel_impl.h`・`chip_support.S` | meinext claim・優先度スタックをソフトpop |
| RV32実証（XLEN抽象） | `arch/riscv_gcc/rp2350` | `chip.cmake`・`common/core_kernel.h` | rv32imac/ilp32・STK_T分岐 |
| Xh3irq割込み状態（OS-awareness） | `arch/riscv_gcc/rp2350` | `chip_os_awareness.py` | 窓方式CSRをexec_progbufで読出し |
| 実機テストランナ（PICO2） | `scripts/ci` | `run_board_pico2.sh` | UARTキャプチャ→OpenOCD書込み→完走マーカ待ち |
| GICv3初期化 | `stm32mp257f_dk_arm64_gcc` | `target_kernel.c` | GICD/GICR/ICC |
| Cortex-A35 MMU/キャッシュ初期化 | `stm32mp257f_dk_arm64_gcc` | `target_kernel.c` | EL1ページテーブル |
| Cortex-A35 アーキテクチャタイマ | `stm32mp257f_dk_arm64_gcc` | `target_timer.c` | CNTP_TVAL_EL0 |
| Cortex-A35 UART | `stm32mp257f_dk_arm64_gcc` | `target_serial.c` | USART |
| POSIXホストシミュレーション | `linux_gcc` | 全ファイル | signal/timerエミュレーション |
| ZynqMP（Cortex-A53）EL3初期化 | `arch/arm64_gcc/zynqmp` | `chip_kernel_impl.c` | SCR/SMPEN/STG（QEMUはSTGスキップ） |
| Cadence UART（ARM64） | `arch/arm64_gcc/zynqmp` | `xuartps.[ch]`・`chip_serial.c` | 非TECS SIO |
| QEMU AArch64ベアメタル実行 | `zcu102_arm64_gcc` | `target.cmake`・`target_kernel_impl.c` | xlnx-zcu102,secure=on／SYS_EXIT終了／glibc系ツールチェーン対策 |
| MMUART（PolarFire SoC） | `arch/riscv_gcc/polarfire_soc` | `mmuart.[ch]`・`chip_serial.c` | 非TECS SIO（16550系） |
| QEMU RISC-Vベアメタル実行 | `polarfire_soc_kit_gcc` | `target.cmake`・`target_kernel_impl.c` | microchip-icicle-kit／-bios none／ハートパーキング／SYS_EXIT終了 |
| QEMU esp32c3実行（Espressif fork） | `esp32c3_gcc` | `target.cmake`・`run.cmake` | フラッシュイメージ（-drive if=mtd）起動・objcopyポストビルド・RV32セミホスティング終了 |
| USB Serial/JTAGコンソール | `riscv_gcc/esp32c3` | `esp32c3_usbjtag.[ch]`・`chip_serial.c` | ネイティブUSBボード用SIO（/dev/ttyACM*直結・ホスト未接続時は出力破棄） |
| ESP32-C3実機テストランナ | `scripts/ci` | `run_board_esp32c3.py` | esptool書込み→RTSリセット→マーカ待ちキャプチャ（pyserial） |
| Flexcomm USART（i.MX RT600） | `arch/arm_m_gcc/imxrt600` | `imxrt600_usart.[ch]`・`chip_serial.c` | 非TECS SIO（FRG分周＋FIFOTRIG割込み） |
| CTimerによるHRTタイマ | `mimxrt685evk_gcc` | `target_timer.c`・`target_timer.h` | 1MHzプリスケーラ＋MR0マッチ割込み（32bitアップカウンタ） |
| XIP実行（FlexSPI設定ブロック） | `mimxrt685evk_gcc` | `flash_config.c`・`mimxrt685.ld`・`target_kernel.py` | `.flash_conf`@0x400・ベクタ9=イメージタイプ(bit14)・ベクタテーブル@0x1000 |
| テーブル駆動のDATA/BSS初期化（XIP用） | `arch/arm_m_gcc/imxrt600` | `start_imxrt600.S`・`chip.cmake` | `__data_section_table`方式．chip.cmakeでcommon/start.Sと差し替え |
| PMIC（I2C）・FBB・PLLの起動時初期化 | `mimxrt685evk_gcc` | `target_kernel_impl.c` | hardware_init_hook（PCA9420設定・300MHz PLL・キャッシュ有効化） |

---

## デバッグ支援（gdb OS-awareness）

エンジンは `scripts/gdb_os_aware/os_awareness.py`（全ターゲット共通）。
ターゲット依存部は target→chip→core の3層（ソース階層に対応）で、新ターゲット
追加時は `target_os_awareness.py`（＋必要なら chip/core 層）を実装する。
経緯・検証は `docs/dev/os-awareness.md`。

| 実装したいもの | 参照 | ファイル | パターン |
|---|---|---|---|
| GICv2 割込み状態（arm64） | `arch/arm64_gcc/common` | `core_os_awareness.py` | GICD_ISENABLER/ISPENDR読出し・primap=MSB詰め |
| GICv2 割込み状態（arm） | `arch/arm_gcc/common` | `core_os_awareness.py` | 同上（32bit） |
| NVIC 割込み状態（arm_m） | `arch/arm_m_gcc/common` | `core_os_awareness.py` | ISER/ISPR・SysTickはSYST_CSR/ICSR・primap=LSB |
| PLIC 割込み状態（riscv） | `arch/riscv_gcc/common` | `core_os_awareness.py` | IEM/IPEND読出し（base/cidxはchip層から）・primap=LSB |
| chip層（GICDベース等の定数） | `arch/arm64_gcc/zynqmp` ほか | `chip_os_awareness.py` | ベースアドレスを持ちcore層を呼ぶ |
| target層（再エクスポート） | `target/zcu102_arm64_gcc` ほか | `target_os_awareness.py` | chip（無ければcore）のAPIを公開 |
| デバイス読出しの安全ガード | `target/zcu102_arm64_gcc` | `target_os_awareness.py` | QEMU不具合時に環境変数でオプトイン |
| マルチクラスタQEMUへのgdb接続 | `target/polarfire_soc_kit_gcc` | `target.cmake`（ASP3_OSDEBUG_GDB_CONNECT） | inferior 2へattach後にfile |

---

## システムサービス（syssvc／TECSレス版）

| 機能 | ファイル | 備考 |
|---|---|---|
| シリアルドライバ | `syssvc/serial.c` | `target_serial.c` を呼ぶ |
| syslog（構造化ログ） | `syssvc/syslog.c` | `T=,EV=` 形式出力 |
| ログタスク | `syssvc/logtask.c` | — |
| バナー表示 | `syssvc/banner.c` | 起動時表示 |
| QEMU終了 | `syssvc/qemu_exit.c` | セミホスティングSYS_EXIT |

---

## タイマ実装パターンの選択指針

| 条件 | 推奨パターン | 参照 |
|---|---|---|
| Cortex-M でSysTickが空いている | SysTick直接制御 | `mps2_an505_gcc` |
| SDKがタイマAPIを提供（Pico等） | SDKラッパー経由 | 外部 `asp3_pico_sdk` リポジトリ |
| Cortex-A | アーキテクチャタイマ（CNTP） | `stm32mp257f_dk_arm64_gcc` |
| RISC-V | CLINT mtime/mtimecmp | `arch/riscv_gcc/common`（mtimer） |

---

## シリアル実装パターンの選択指針

| 条件 | 推奨パターン | 参照 |
|---|---|---|
| QEMU・デバッガ接続あり | セミホスティング | `mps2_an505_gcc` |
| SDKがUART APIを提供 | SDKラッパー経由 | 外部 `asp3_pico_sdk` リポジトリ |
| メモリマップドUART直接 | レジスタポーリング | `stm32mp257f_dk_arm64_gcc` |
