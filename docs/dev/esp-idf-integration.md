# ESP-IDF統合（ESP32-C3）

## 項目

ESP-IDF統合（AGENTS.md §1 機能追加計画、優先度：中。第一目的「各社SDKとの協調動作」の**第5弾**＝Pico SDK・FSP・STM32・NXPに続く）

## 内容

Espressif ESP-IDF と TOPPERS/ASP3 を協調動作させる。対象は
**ESP32-C3**（RV32IMC シングルコア 160MHz・400KB SRAM・Wi-Fi 4／BLE 5）。

### 既存4統合と本質的に異なる点（最重要）

pico-sdk／FSP／STM32 HAL／MCUXpresso はいずれも本質的にRTOS非依存のHALであり、
「SDKドライバをそのまま使う」協調が成立した。**ESP-IDFはFreeRTOS前提のOSフレームワーク**であり：

- ドライバ層（`esp_driver_*`）自体がFreeRTOS API（queue/semaphore/task）を直接呼ぶ
- Wi-Fi／BTのバイナリblobもFreeRTOS上で動くことを仮定している

このためESP-IDFそのものとの協調は不可能で、Zephyr（hal_espressif）／NuttXが確立した
**esp-hal-3rdparty方式**＝下層のhal/soc層のみを使い、無線blobはOSアダプタ
（`wifi_osi_funcs_t` 等のosi関数群）のshimで載せる方式を採る。
実態は「SDK統合」というより「**esp-halベースのベアメタル移植＋FreeRTOS shim**」であり、
既存4統合より工数は大きい。逆にこれを通せば `ASP3_TARGET_DIR` 方式の一般性の最も強い実証になる。

### 本リポジトリにとってのメリット

1. **RISC-V archの完成度向上**：割り込みコントローラの3種目
   （Xh3irq〔RP2350〕・PLIC〔PolarFire〕に加えEspressif INTC＝割り込みマトリクス＋31本CPU割込み）
2. **QEMUでCIが回る**：Espressif公式QEMU forkに esp32c3 マシンがある
   （`qemu-system-riscv32 -M esp32c3`）
3. **普及度**：ボード数百円・入手性最強・無線内蔵。利用者層のリーチは4統合中最大
4. FreeRTOS互換shim（os_adapter on ASP3）はそれ自体が独立した成果物になる

### ターゲット選定（ESP32-C3 に決定）

| チップ | コア | 無線 | QEMU | 判断 |
|---|---|---|---|---|
| **ESP32-C3** | RV32IMC ×1・160MHz | Wi-Fi 4／BLE 5 | **公式fork対応** | **採用**。INTCが単純・QEMU/CI可・情報最多 |
| ESP32-C6 | RV32 HP×1＋LP×1 | Wi-Fi 6／802.15.4 | 要確認 | 将来の横展開先（単体ボードは普通にプログラム可能） |
| ESP32-P4 | RV32 HP×2＋LP×1 | **なし**（C6をSDIOコンパニオン外付け。ボード上のC6はesp-hostedファーム固定＝実質無線NIC） | − | 対象外。RISC-Vでマルチコアは P4 のみだが無線なし＝FMP3を持ち出す動機が弱く、asp3_core資産（非TECS・Python cfg・上流追従台帳）のFMP3再構築は目的に対し過大。必要になればASP3をHPコア片側で動かす（ESP-IDF unicoreモードの前例）かAMPで対応 |
| ESP32／S2／S3 | Xtensa | − | − | 対象外（ISA違い） |

## 実施プラン（2段階・NXP統合と同型）

### Phase 0：調査（着手時に確定させる事項）

1. **ブート方式**：候補2つ
   - **Direct Boot**（ESP32-C3 ECO3以降）：二段ブートローダ不要。フラッシュ先頭の
     マジックナンバーをROMが検出し、フラッシュをMMUでマップしてそのまま実行。
     ベアメタル前例あり（bare-metal Rust等）。**第一候補**
   - esptoolアプリイメージ形式＋ESP-IDF二段ブートローダ：ESP-IDF標準。Phase Bでは必要になる可能性
2. **QEMU起動経路**：Espressif QEMU forkは `-kernel` ELF直ロードではなく
   フラッシュイメージ（`-drive file=flash.bin,if=mtd,format=raw`）を要求する見込み。
   `esptool merge_bin` 等でのイメージ生成をCMakeポストビルドに組み込む
3. **devcontainer**：Espressif QEMU fork（上流QEMUとは別バイナリ）と esptool の追加
   （`docs/dev/devcontainer.md` 更新・イメージ再ビルド）
4. **HRTタイマ選定**：TIMG（54bit・APBクロックのプリスケーラで1MHz生成可）を第一候補、
   SYSTIMER（16MHz固定・52bit）を対抗として精度・ラップ処理を比較

### Phase A：ベアメタルESP32-C3ターゲットを asp3_core 本体に追加

SDK（ESP-IDF）非依存の自己完結ターゲット。QEMUでCIが回る形にする。

1. `arch/riscv_gcc` に chip: **esp32c3** を追加
   - RV32IMC（既存rv32imac＝Hazard3との差はA拡張なし→アトミック非依存を確認）
   - **Espressif INTC**：割り込みマトリクス（ペリフェラルソース→31本のCPU割込みへ動的割当て）
     ＋優先度1〜15。Xh3irq／PLICに続く3種目の割込み流儀として `chip.cmake`＋
     `chip_kernel.[ch]` 系で実装
2. `target/esp32c3_gcc` を追加
   - コンソール：UART0
   - HRT：Phase 0で選定したタイマ（目標1μs・`TCYC_HRTCNT`／ラップ処理含む）
   - リンカスクリプト・スタートアップ（Direct Boot前提。まずはSRAM実行〔400KBで十分〕から始め、
     フラッシュXIP〔キャッシュ/MMU設定〕は動作後に対応）
3. CMake：`presets.json`（`esp32c3-qemu`／実機 `esp32c3`）＋ツールチェーンは既存
   riscv用（rv32imc用の `-march` 調整）。フラッシュイメージ生成をポストビルドに追加
4. 検証（テスト実行順序どおり）：
   - QEMU：test_porting 6/6 → sample1 → testexec → **CIジョブ追加**（SDK生成依存なし＝NXP Phase Aと同じ利点）
   - 実機：**ESP32-C3-DevKitC-02**（内蔵USB Serial/JTAG経由。書込み esptool・デバッグ OpenOCD）で
     test_porting 6/6 → testexec → dlynse較正
5. OS Awareness・osdebug（riscv層の既存資産を流用）

### Phase B：ESP-IDF（esp-hal）統合（外側リポジトリ）

1. リポジトリ名：**`asp3_esp_idf`**。構成は asp3_mcuxsdk と同型
   （asp3_core submodule＋`ASP3_TARGET_DIR`＋`ASP3_LIBRARY_ONLY`）
2. **B-1：esp-hal統合**：esp-hal-3rdparty（Zephyr/NuttXが使う抜き出しhal/soc層）を
   submodule参照し、GPIO・UART・タイマ等をhal層API経由に置換。
   **ESP-IDFのドライバ層（`esp_driver_*`）はFreeRTOS依存のため使わない**
   （既存4統合の「SDKドライバをそのまま使う」とはここが違う）
3. **B-2：Wi-Fi os_adapter shim（本丸）**：Wi-Fi blobの要求するosi関数群
   （queue/semaphore/mutex/task/timer等の数十関数）をASP3プリミティブで実装
   - 動的メモリ：blobはmalloc相当を要求する。**カーネル外（アプリ/ライブラリ層）のヒープ**として
     整理し（禁則②はカーネル内の規定）、newlib malloc等の採用可否を設計時に判断
   - スコープはまず **Wi-Fi init〜scan〜AP接続まで**。TCP/IPスタック（lwIP no-RTOS／TINET等）は
     接続確認後に別途判断
   - blobのライセンス（Espressif配布条件）とTOPPERSライセンスの共存を確認（リンクのみなら問題ない見込み）
4. 実機検証＋移植skill（porting-asp3-to-esp32 等）を外側リポジトリに

### リスク・未確定事項

- Direct BootのQEMU側サポート有無（不可ならesptoolイメージ形式へフォールバック）
- Espressif QEMU forkのバージョン固定・devcontainerイメージ肥大
- 無線blobのFreeRTOS仮定がosi shimで完全に切れるか（Zephyr/NuttXの前例はあるが
  ESP-IDFバージョンにより差がある）
- Wi-Fiを載せない場合ESP32を選ぶ意味が半減する＝Phase Bまでやり切って価値が出る項目

## 実施結果

### Phase 0（調査）結果（2026-07-02〜03）

1. **QEMU**：Espressif版QEMU 9.2.2（esp-develop-9.2.2-20260417・プレビルト）を
   `~/TOPPERS/ASP3CORE/tools/qemu-esp/` に導入。esp32c3マシン・ROM
   （esp32c3-rom.bin）同梱。フラッシュイメージ（`-drive file=...,if=mtd,format=raw`）
   から起動する（`-kernel` ELF直ロード非対応）。
2. **ブート方式＝Direct Boot採用を実証**：フラッシュ先頭マジック
   （0xAEDB041D×2）＋flash+8エントリの最小バイナリがQEMU同梱ROMで起動する
   ことをスパイクで確認。**二段ブートローダ・esptoolイメージ形式とも不要**
   （esptoolは実機書込み時のみ必要）。
3. **セミホスティング終了**：RV32では `a0=0x18(SYS_EXIT)`・`a1=0x20026` 直値で
   QEMUが終了コード0で終了することを確認（RV64のパラメタブロック形式と異なる）。
4. **HRTタイマ＝SYSTIMER採用**：16MHz固定（XTAL 40MHz÷2.5）・52bit・
   μs=count/16の完全シフト変換。TIMG案は棄却（QEMUでの再現性とIDF実績を優先）。
5. **参照資料**：`~/TOPPERS/ASP3CORE/ref-esp32c3/` にesp-idf（sparse checkout）・
   esp32c3-direct-boot-example・レジスタ早見表 `HW_NOTES.md` を配置（リポジトリ外）。

### Phase A（QEMU）結果（2026-07-03）

ESP-IDF非依存の自己完結ターゲット `esp32c3_gcc`（プリセット `esp32c3-qemu`）を追加。

**設計の要点**（他TOPPERS系RTOSでの同種移植の参考）：

- **割込みコントローラ（INTMTX＝割込みマトリクス）**：Xh3irq（rp2350）と同じ
  chip層契約（`irc_begin/end_int`・`trap_vector_table`）で3種目のRISC-V割込み
  流儀を実装。vectoredモードで `mcause&0x1f`＝CPU割込み線番号＝ASP3のINTNO
  （1〜31・ずらしなし）。優先度は1〜7（物理4bitだが公式規定は7まで）。
  ハードウェアの優先度自動昇格が無いため，入口でTHRESHレジスタを「受付け
  優先度+1」へソフト昇格・出口で復元（ESP-IDF vectors.Sと同じパターン）。
- **ras_int／clr_int／prb_int**：C3はXh3irqのmeifa相当を持たないため，
  ソフトウェアでアサートできるlevelソース**FROM_CPU_0〜3**を割込み線に
  多重マップして実現。タイマ割込みの強制（過去時刻set_event・raise_event）
  はFROM_CPU_0をSYSTIMERと同じ線に，テスト用INTNO1（=3）はFROM_CPU_1を割当て。
  prb_intはFROM_CPUレジスタ読み返し＋ソース生ステータスで判定。
- **Direct Bootリンカスクリプト**：フラッシュがIROM(0x42000000)/DROM(0x3C000000)
  へ線形二重マップされるため「VMAオフセット＝フラッシュオフセット」を維持。
  LMAはDROM基準（**cfg pass1がシンボルVMAでsrec(LMA)を引くため.rodataは
  VMA==LMAが必須**）。マジック＋エントリ＋コードは単一.textセクションに
  まとめ（セクション内はVMA/LMAオフセットが常に一致），空.data対策の番兵
  LONG(0)を配置。.dataはstart.Sの既存機構でDROM→RAMコピー。
- **WDT無効化**：リセット後デフォルトで有効なMWDT0/1・RTC WDT・スーパーWDTを
  hardware_init_hookで無効化（しないと数秒でリブート）。
- **QEMU固有の知見**：
  - QEMUのesp32c3モデルは割込みを**RISC-V標準のmip/mie経由**で配送するため，
    mieを全ビット許可する必要がある（実機のINTCはmieを経由しない＝ESP-IDFは
    mieを触らないが，全許可は実機でも無害）。
  - INTR_STATUSレジスタ（ソース生ステータス）は未実装（読出し0）。
    prb_intのFROM_CPUレジスタ読み返しが実効的な判定になる。
- **UART0**：ROMブートローダの115200bps設定を継承（初期化コード無し）。
  SIOドライバはFIFOカウンタ（STATUS）とINT_ENA/INT_CLRで実装。

### 変更したファイル

| ファイル | 変更内容概要 |
|---|---|
| `CMakePresets.json` | `target/esp32c3_gcc/presets.json` のinclude追加 |
| `AGENTS.md` | §4にQEMU esp32c3のビルド・実行コマンド追加 |
| `.github/workflows/ci.yml` | esp32c3-qemuジョブ追加（Espressif QEMUをジョブ内DL・sample1スモーク・test_porting・testexecスモーク） |
| `DIVERGENCE_MAP.md`／`docs/porting/IMPL_INDEX.md`／`docs/building.md`／`docs/dev/README.md` | esp32c3の台帳・索引追記 |

### 追加したファイル

- `arch/riscv_gcc/esp32c3/`（チップ依存部）：
  `esp32c3.h`（MMIO定義）・`intmtx_kernel_impl.h`（INTMTXドライバ）・
  `chip_kernel_impl.[ch]`・`chip_support.S`（irc_*・trap_vector_table）・
  `chip.cmake`・`chip_kernel.py`・`esp32c3_uart.[ch]`／`chip_serial.[ch]`／
  `chip_serial.cfg`（非TECS SIO）・chip_*ボイラープレート（rename/sil/stddef/
  os_awareness等．rp2350雛形）
- `target/esp32c3_gcc/`（ターゲット依存部）：
  `flash_header.S`（Direct Bootマジック＋エントリ）・`esp32c3.ld`・
  `target_timer.[ch]`（SYSTIMER）・`target_kernel_impl.[ch]`（WDT無効化・
  ソースルーティング・セミホスティング終了）・`target.cmake`／`run.cmake`
  （フラッシュイメージ生成・QEMU run）・`presets.json`・cfg一式・
  target_*ボイラープレート（pico2_riscv雛形）

### 削除したファイル

なし

### Git情報

- ベースコミット：`9e62b6c`（docs(dev): add ESP-IDF integration plan）
- ブランチ：`feat/esp32c3`
- ファイルリスト再現コマンド例：`git diff --stat upstream main -- arch/riscv_gcc/esp32c3 target/esp32c3_gcc`

### 検証結果

| テスト | 実施 | 結果 |
|---|---|---|
| POSIX | ○ | 回帰なし（linuxプリセット・ctest） |
| QEMU (esp32c3)・sample1 | ○ | バナー＋task実行を確認 |
| QEMU (esp32c3)・test_porting | ○ | **6/6 passed** |
| QEMU (esp32c3)・testexec（36件） | ○ | **35/36 PASS**（cpuexc10=対象外SKIP扱いPASS・**dlynseのみNG＝QEMUが実時間を再現しないため計測不能の想定NG**（実機較正専用・他QEMUターゲットもCI対象外）） |
| 実機 (ESP32-C3-DevKitC-02) | − | 未実施（今後．asp_flash.binを`esptool write_flash 0x0`で書込み予定） |

### DIVERGENCE_MAP との関連

kernel/・include/・arch/riscv_gcc/common/ 等のPRISTINE/EXTENDED領域への変更なし
（新規追加のみ）。DIVERGENCE_MAP.mdに `arch/riscv_gcc/esp32c3/`・
`target/esp32c3_gcc/` をNEWとして追記済み。

### Phase A（実機）結果（2026-07-03）

実機ボード（ESP32-C3 rev v0.4・内蔵フラッシュ4MB・ネイティブUSB接続＝
UARTブリッジなし・USBは303a:1001 USB Serial/JTAGとして列挙）で検証。

**実機で判明した事項（QEMUとの差分）**：

- **mie/mip CSRが存在しない**：実機C3のCPUはmie/mipを実装せず，アクセス
  すると不正命令例外になる（`csrwi mie,0`＝共通start.Sの4命令目で
  Guru Meditation panicとして発覚）。QEMUのmie全許可必須とは**正反対**。
  → 共通`start.S`に`TOPPERS_OMIT_MIE_INIT`ガードを追加（既定は従来
  どおり＝他RISC-Vターゲット不変）し，esp32c3のchip層で定義。
  chip_initializeのmie設定は`TOPPERS_USE_QEMU`時のみに変更。
- **Direct Bootは実機ROMで動作**（rev v0.4＝ECO3以降。ROMがマジックを
  検出しflash+8へジャンプすることを確認）。
- **CPUクロックはリセット既定のXTAL/2＝20MHzのまま起動**：Direct Boot
  では二段ブートローダのクロック設定が無いため。dlynse計測（ループ
  200ns/4サイクル）で発覚。ROMがブート時に有効化したBBPLL（480MHz）
  へ`SYSTEM_CPU_PER_CONF`／`SYSTEM_SYSCLK_CONF`の2レジスタで切り替え，
  **160MHz動作を実測確認**（ループ25ns=4サイクル@160MHz）。
  `CORE_CLK_MHZ=160`確定。
- **dlynse較正**：SIL_DLY_TIM1=40・TIM2=25（実測：呼出しオーバヘッド
  ≈43ns・ループ25ns）。

**USB Serial/JTAGコンソールの追加**：UARTブリッジを持たないネイティブ
USBボードでは，UART0の出力はホストに届かない。チップ内蔵のUSB Serial/
JTAGコントローラ（0x60043000・EP1 FIFO＋WR_DONEフラッシュ・割込み
ソース26）用のSIOドライバ`esp32c3_usbjtag.[ch]`を追加し，
`ESP32C3_CONSOLE`（uart0／usbjtag．既定＝QEMUはuart0・実機はusbjtag）
で切替可能にした。ホスト（端末）未接続時は送信FIFOが空かないため，
ポーリング出力（target_fput_log）はリトライ上限で出力を捨てる。

**実機テストランナ**：`scripts/ci/run_board_esp32c3.py`（esptool書込み
→pyserialのRTS操作でチップリセット→完走マーカまでキャプチャ）。
esptool終了時のリセットに任せると出力先頭を取りこぼすため，リセットは
ポートを開いた状態で自前で行う。プリセット`esp32c3`（実機用．run=
esptool書込み・`-DESP32C3_PORT=`でポート指定）も追加。

**検証結果（実機・160MHz）**：

| テスト | 結果 |
|---|---|
| test_porting | **6/6 passed** |
| testexec（36件） | **36/36 PASS**（cpuexc10=対象外SKIP扱いPASS・dlynse含む） |
| QEMU回帰 | test_porting 6/6維持（クロック切替・コンソール変更後） |

### Phase B-0／B-1（外側リポジトリ・esp-hal統合）結果（2026-07-03）

外側リポジトリ **[asp3_esp_idf](https://github.com/exshonda/asp3_esp_idf)**
（ローカル：`~/TOPPERS/ASP3CORE/asp3_esp_idf`）を作成し，
esp-hal-3rdparty統合（B-1）まで完了。

- **B-0（骨格）**：asp3_core submodule（feat/esp32c3）＋
  `asp3/target/esp32c3_espidf/`（外部ターゲット規約）＋パス解決ヘルパ。
  **ビルド方式はpico-sdk型**（asp3_core本体のCMakeを`ASP3_TARGET_DIR`で
  駆動）——ESP32-C3はASP3自前のDirect Bootで起動するため，mcuxsdk型
  （SDKスタートアップ＋main→sta_ker）の協調が不要なことによる。
- **B-1（esp-hal統合）**：
  - submodule `hal/`＝espressif/esp-hal-3rdparty本家を**NuttX検証済み
    コミットSHA（release/master.c系）に固定**（Zephyr方式＝フォーク維持
    ではなくNuttX方式。ASP3固有スタブは外側リポジトリに置きhal/無改変）
  - Kconfig非依存：sdkconfig.hはesp-hal同梱のNuttX用静的スタブを流用し，
    nuttx/config.h・assert.h・string.hは最小スタブ（hal_stub/）で供給
    （riscv64-unknown-elf-gccにnewlibヘッダが無い環境でも成立）
  - **LL層（static inlineのレジスタ薄層・RTOS非依存）でUSB Serial/JTAG
    コンソールとSYSTIMERタイマを再実装**し統合を実証（公開シンボル同一の
    ためasp3_core無改変・target.cmakeのREMOVE/APPENDで差替え）。
    ペリフェラル構造体はesp-halのesp32c3.peripherals.ldをINCLUDE
  - Wi-Fi blob submodule（esp32-wifi-lib≈2GB）はB-2までinitしない
  - 設計記録は asp3_esp_idf の `docs/hal-integration.md`
- **検証**：QEMU test_porting 6/6・実機test_porting 6/6・
  **実機testexec 36/36 PASS**（LL版ドライバ・外側リポジトリ経由）

### 残作業

- **Phase B-2a：Wi-Fi scan＝実機成功（完了・asp3_esp_idf@1686997）**。
  実機ESP32-C3で`esp_wifi_init→start→scan`が完動し，**周囲AP16〜17個の
  SSID/RSSI/chを実受信**（RF較正も機能）。shim全実装（静的プール・osi
  テーブルABI 0x8全エントリ・wpa_supplicant/mbedtls/PHYフル較正の
  ビルド統合）＋実機JTAGで解明した3つのDirect Boot起因ブロッカーの解決：
  ①モデムクロック未初期化（SYSTEM_WIFI_CLK_EN_REGをhardware_init_hookで
  セット）②coex os_adapter登録③ROM coexist_funcsのNULL回避（ダミー
  no-opテーブル）。詳細は asp3_esp_idf の `docs/wifi-shim.md`
- **Phase B-2b：WPA2 AP接続＝実機成功（完了・asp3_esp_idf@6956669）**。
  実機ESP32-C3がWPA2 APへL2接続成立（STA_CONNECTED）。WPA2 4-way
  ハンドシェイクタイムアウト（reason=15）を実機JTAGで解明した2つの
  Direct Boot起因shimバグ（①HW RNGレジスタアドレス誤りでSNonce全ゼロ
  ②PSA Crypto未初期化でPTK/MIC不一致）を修正。IP/DHCPはスコープ外
  （L2まで）。詳細は asp3_esp_idf の `docs/wifi-shim.md`
- OS Awareness（osdebug）の実機動作確認（chip_os_awareness.pyはMMIO
  読出しで実装済み・未検証。デバッガ接続はOpenOCD-esp32＝Espressif
  fork版OpenOCDが必要）
- devcontainerへのEspressif QEMU追加（現状CIはジョブ内ダウンロード）
