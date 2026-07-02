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

（未着手。Phase 0 の調査結果と各Phaseの完了時に記載）

### 変更したファイル

| ファイル | 変更内容概要 |
|---|---|
|  |  |

### 追加したファイル

### 削除したファイル

### Git情報

- ベースコミット：
- 関連コミット範囲：
- ファイルリスト再現コマンド例：`git diff --stat upstream main -- arch/riscv_gcc/esp32c3 target/esp32c3_gcc`

### 検証結果

| テスト | 実施 | 結果 |
|---|---|---|
| POSIX | − |  |
| QEMU (esp32c3) | − |  |
| 実機 (ESP32-C3-DevKitC-02) | − |  |

### DIVERGENCE_MAP との関連

（kernel/ 等PRISTINE領域への変更は想定なし。発生した場合に記載）
