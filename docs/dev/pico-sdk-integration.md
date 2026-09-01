# Pico SDK統合

## 項目

Pico SDK統合（AGENTS.md §1 機能追加計画、優先度：中。第一目的「各社SDKとの協調動作」の第1弾）

## 内容

Raspberry Pi Pico SDK と TOPPERS/ASP3 を**協調動作**させる。これまでの
`raspberrypi_pico2_gcc`／`_riscv_gcc`（SDK非依存ベアメタル）と対になる
「**SDK同居**」構成を、**外側のSDKリポジトリ（asp3_pico_sdk）側で管理**する形で実現する。

### 意義（何が嬉しいか）

1. **第一目的の第1弾**：ASP3アプリから pico-sdk のドライバ資産
   （USB CDC・PIO・DMA・Flash・Pico W WiFi等）を直接呼べる。基盤整備
   （高優先13項目）はこのための土台だった。
2. **リポジトリ分離による上流追従の保護**：SDK固有の arch/target を
   asp3_core に入れず外側で持つことで、**asp3_core を純カーネルに保てる**
   （上流マージ・DIVERGENCE_MAP管理が軽くなる）。
3. **submodule構成への移行の実証**：`docs/FILE_PLAN.md` §2 の
   「既存3リポジトリ→asp3_coreをsubmodule参照」構想の最初の実例になる
   （FSP・STM32統合の雛形）。

### 移植元（実証済みの仕組み・確認済み）

`github.com/toppers/asp3_pico_sdk`：`asp3_pico_sdk.cmake`（`PICO_PLATFORM`→
`ASP3_TARGET` 解決・`asp3_set_pico_sdk_options()`）＋ `target/rp2350-arm-s_pico_sdk/`。
共存の要は **pico-sdkの割込み登録API（`irq_set_exclusive_handler`等8関数）を
リンカ `--wrap` でASP3管理へ誘導**＋ `PICO_RUNTIME_SKIP_INIT_*` でSDKの
ベクタ/IRQ優先度初期化を抑止する点（ASP3がNVICを掌握）。
**RISC-Vは asp3_pico_sdk でも未対応**（ARM-S版を先行）。

## 目標構成（本セッションで合意）

```
asp3_pico_sdk/                  ← 外側リポジトリ（後で _sample からリネーム）
├── asp3_core/                  ← submodule（純カーネル：kernel/ cfg/ syssvc/ arch共通部）
├── arch/                       ← SDK向けチップ依存部（arm_m/rp2350・riscv/rp2350）
├── target/                     ← SDK向けターゲット依存部（ASP3_TARGET_DIRで指定）
├── sample1/                    ← アプリ（ISAで分けない＝PICO_PLATFORMで切替）
│   ├── sample1.c / sample1.cfg
│   └── CMakeLists.txt
└── CMakeLists.txt              ← asp3_pico_sdk.cmake相当（PICO_PLATFORM→target解決）
```

### 設計判断（本セッションの検討結果）

- **arch/target（カーネル依存部）はISAごとに2つ必要**（arm_m/rp2350＋riscv/rp2350）。
  ISAが根本的に違う（割込み＝NVIC vs Xh3irq・ブート＝ARM/RISC-V IMAGE_DEF）。
  ※共通arch（`arch/arm_m_gcc/common`・`arch/riscv_gcc/common`）とcfgエンジン・
  カーネル本体は asp3_core 側（submodule）に残す＝`ASP3_ROOT_DIR` 参照のまま。
- **アプリ（sample1）はISAで分けない＝マージ**。同一ボード(RP2350)でソースは
  ISA非依存。差はツールチェーンと `PICO_PLATFORM` だけ（pico-sdk idiomに一致）。

## asp3_core側の作業（このリポジトリ）

外部リポジトリから arch/target を供給可能にする**最小拡張**：

1. **`ASP3_TARGET_DIR` 変数の導入**（`asp3_core.cmake`・後方互換）
   ```cmake
   if(NOT DEFINED ASP3_TARGET_DIR)
       set(ASP3_TARGET_DIR ${ASP3_ROOT_DIR}/target/${ASP3_TARGET})
   endif()
   # 存在確認とinclude を ASP3_TARGET_DIR 基準に
   ```
   未指定時は従来どおり（既存7ターゲットに影響なし＝CI green維持）
2. **外部target.cmakeの参照規約を明文化**：
   - 外部チップ依存部のincludeは `${CMAKE_CURRENT_LIST_DIR}` 相対で行う
   - 共通arch（`arch/<core>/common/arch.cmake`）・cfgテンプレート
     （core_*.py）は `${ASP3_ROOT_DIR}` 参照（submodule側）でよい
   - cfgテンプレート・Cソースは `ASP3_*_FILES` に**絶対パス**で供給（既存規約）
3. **検証用に最小の外部ターゲットでスモーク**（asp3_core単体CIを汚さず）：
   既存ターゲットを一時的に外部ディレクトリへコピーし `ASP3_TARGET_DIR` で
   ビルドが通ることを確認（POSIX or mps2で十分）
4. **PORTING_GUIDE.md に「外部（SDK）ターゲットの置き方」節を追加**
5. 記録：DIVERGENCE_MAP（asp3_core.cmake＝NEW改修）・README索引・本ファイル

> asp3_core側の変更はこの **CMakeエントリ1点の後方互換拡張のみ**。
> SDK固有の arch/target/sample の実装は **asp3_pico_sdk リポジトリ側**で行う
> （本計画のスコープはasp3_core側の受け入れ口整備まで。SDK側実装は別リポジトリ作業）。

## asp3_pico_sdk側の作業（別リポジトリ・参考）

本リポジトリのスコープ外だが、受け入れ口設計の妥当性確認のため記載。

### リポジトリ方針：**A案＝既存sampleを再利用**（新規作成しない）

既存2リポジトリを `../`（asp3_core の隣）にclone済み・実体確認（2026-06-07）：

| clone | 実体 | 移行での役割 |
|---|---|---|
| `../asp3_pico_sdk` | カーネル同梱（arch/cfg/include/kernel/library/syssvc/target＋**独自CMakeLists.txt**＝asp3_core CMakeのpico版fork＋`asp3_pico_sdk.cmake`） | **移植元**。archive（読取専用）して参照点に |
| `../asp3_pico_sdk_sample` | **submodule `asp3`→asp3_pico_sdk 済み**・`sample1/`（`pico_sdk_import.cmake`＋動作するCMakeLists：PICO_BOARD pico2・sdk 2.1.1・toolchain 14_2_Rel1・`asp3_set_pico_sdk_options`＝--wrap） | **再利用の土台**。最終的に `asp3_pico_sdk` へ改名 |

→ submodule・pico_sdk_import・--wrap設定など**動作実績のある資産が sample 側に揃っている**ため、新規作成せず `asp3_pico_sdk_sample` を改修する（A案）。

### 移行手順（sample側＝別リポジトリ作業）

1. **旧 `asp3_pico_sdk` を archive**（または `_legacy` 改名）。改名衝突を回避し移植元として温存
2. **submoduleの差替**：`asp3_pico_sdk_sample` の submodule を
   旧 `asp3_pico_sdk` → **`asp3_core`** へ（`.gitmodules` のurl変更・path `asp3`→`asp3_core`）
3. **pico固有部を sample 側へ移設**（旧 asp3_pico_sdk から）：
   - `asp3_pico_sdk.cmake`（`PICO_PLATFORM`→`ASP3_TARGET`＋**`ASP3_TARGET_DIR`** 解決・
     `asp3_set_pico_sdk_options`＝`irq_*` の--wrap）→ sample repo ルート
   - `target/rp2350-arm-s_pico_sdk` → sample repo の `target/`
   - チップ依存部：asp3_core の `arch/arm_m_gcc/rp2350` を使うか、SDK版を
     sample側 `arch/` に持つかは実装時に判断（共通arch・cfgエンジンは asp3_core 参照のまま）
4. **sample1 CMakeLists の付替**：現状 `include(../asp3/asp3_pico_sdk.cmake)`＋
   `add_subdirectory(${ASP3_CMAKE_DIR})`（＝旧fork CMake）を、
   **asp3_core の正準 CMakeLists ＋ `ASP3_TARGET_DIR`** を使う形へ
   （旧 asp3_pico_sdk の fork CMakeLists.txt は廃止＝asp3_coreに一本化）
5. sample1 を単一フォルダ化（`PICO_PLATFORM` で arm/riscv 切替）
6. **タイマ競合の解決**：SDK alarm（TIMER0）と ASP3 HRT（TIMER0）の調停
7. RISC-V版は後続（asp3_pico_sdk未対応・本リポジトリの
   `raspberrypi_pico2_riscv_gcc` のXh3irq実装を移植元にできる）
8. 検証は実機（pico-sdk必須・CIはビルドまで）
9. 安定後に `asp3_pico_sdk_sample` → `asp3_pico_sdk` へ改名

> 旧 `asp3_pico_sdk` の `CMakeLists.txt` は asp3_core の CMakeLists を pico向けに
> fork したもの（`include(${PROJECT_SOURCE_DIR}/target/${ASP3_TARGET}/target.cmake)`）。
> 新構成では **asp3_core の CMakeLists に一本化**し、pico差分は `asp3_pico_sdk.cmake`
> ヘルパ＋外部 target（`ASP3_TARGET_DIR`）に閉じる＝fork解消が移行のキモ。

## スコープ外

- FSP統合・STM32統合（同じ受け入れ口＝`ASP3_TARGET_DIR` で後日同様に）
- asp3_pico_sdk リポジトリ側の実装そのもの（別リポジトリ作業）
- asp3_core を各SDKリポジトリのsubmoduleに登録する運用切替（FILE_PLAN §2）

## リスク・確認事項

- **タイマ競合**（SDKのTIMER0 alarm と ASP3 HRT）— SDK側実装の最大論点
- 外部target.cmakeが `ASP3_ROOT_DIR` と `CMAKE_CURRENT_LIST_DIR` を取り違えると
  パス解決が壊れる → 規約を明文化＋スモークで担保
- pico-sdk のバージョンピン（devcontainer/CIへの追加可否はSDK側で判断）

## 実施結果

### asp3_core側（受け入れ口整備）— 完了（2026-06-10）

外部リポジトリ（各社SDK）が `target/` を本リポジトリ外に置けるよう、CMakeエントリに
**後方互換の `ASP3_TARGET_DIR` 変数**を導入した。SDK固有の arch/target/sample の実装は
asp3_pico_sdk リポジトリ側の作業（本リポジトリのスコープ外）。

#### 変更したファイル

| ファイル | 変更内容 |
|---|---|
| `asp3_core.cmake` | `ASP3_TARGET_DIR` を導入。未定義時は `${ASP3_ROOT_DIR}/target/${ASP3_TARGET}`（従来動作）を既定とし、存在確認を `${ASP3_TARGET_DIR}/target.cmake` 基準へ変更。外部指定時のエラーメッセージに `-DASP3_TARGET_DIR=<dir>` の案内を追加 |
| `CMakeLists.txt` | `target.cmake` の include を `${ASP3_ROOT_DIR}/target/${ASP3_TARGET}/...` → `${ASP3_TARGET_DIR}/...` へ変更（解決は `asp3_core.cmake` に一元化） |
| `docs/porting/PORTING_GUIDE.md` | Step 6 に「外部（SDK）ターゲットの置き方（`ASP3_TARGET_DIR`）」節を追加。参照規約（チップ依存部＝`CMAKE_CURRENT_LIST_DIR` 相対／共通arch・cfgテンプレート＝`ASP3_ROOT_DIR`／`ASP3_*_FILES`＝絶対パス）を明文化 |
| `DIVERGENCE_MAP.md` | `asp3_core.cmake` 行に `ASP3_TARGET_DIR`（外部SDKターゲット受け入れ口＝後方互換拡張）を追記 |
| `docs/dev/README.md` | Pico SDK統合の状態を「計画中」→「実施中（asp3_core側 完了）」へ更新 |

追加・削除ファイル：なし（既存ファイルの後方互換拡張のみ）。

#### Git情報

- ベースコミット：`c6a0079`（main）
- ブランチ：`feat/pico-sdk-target-dir`
- ファイルリスト再現：`git diff --stat c6a0079 feat/pico-sdk-target-dir -- asp3_core.cmake CMakeLists.txt docs/porting/PORTING_GUIDE.md DIVERGENCE_MAP.md docs/dev/`

#### 検証結果（POSIX）

（注：2026-06-16 に QEMU の M33 ターゲットを mps2-an505/IoTKit へ置換〔現プリセット mps2_an505-qemu・ターゲット名 mps2_an505_gcc〕。FPU が CPU0 に実装されハード浮動小数点で再検証可能になった。本節以下の `mps2_an521-qemu` / `mps2_an521_gcc` 表記は置換前の記録。以降の M33 QEMU 検証は mps2-an505 を使用。）

1. **既存プリセット無変更（CI green維持）**：
   - `linux`（POSIX CLI）— ビルドOK・`asp` 実行で `task1 is running` 確認。
   - `mps2_an521-qemu`（Cortex-M33）— ビルドOK・`asp.elf` 生成。QEMU(mps2-an521) 実行で起動確認。
2. **外部ターゲットスモーク**：`target/linux_gcc` を `/tmp/asp3_ext_target/linux_gcc` へコピーし、
   チップ依存部 `TARGETDIR` を `${CMAKE_CURRENT_LIST_DIR}` へ書き換え（共通arch `arch/posix_gcc` は
   `${ASP3_ROOT_DIR}` のまま）。`-DASP3_TARGET=linux_gcc -DASP3_TARGET_DIR=/tmp/asp3_ext_target/linux_gcc`
   でconfigure・ビルド・実行が成功（外部の `target.cmake` が読まれ、チップ依存部は外部・共通archは
   submodule側から正しく解決されることを確認）。

> 実機検証（タイマ競合の解決等）は asp3_pico_sdk リポジトリ側の作業。本リポジトリのスコープは
> 受け入れ口整備まで。

---

### asp3_core側（ライブラリ専用モード）— 完了（2026-06-10）

SDKアプリ（pico_stdlib をリンクする最終実行ファイル）が asp3_core を
`add_subdirectory` して `asp3` ライブラリだけを取り込めるよう、`CMakeLists.txt` に
**`ASP3_LIBRARY_ONLY` オプション**を追加した（既定OFF＝従来どおり `asp` 実行ファイルまでビルド）。

| ファイル | 変更内容 |
|---|---|
| `CMakeLists.txt` | `option(ASP3_LIBRARY_ONLY ... OFF)` を追加。ON のとき `asp` 実行ファイル・サンプル・ctest登録・実行/デバッグターゲットを作らず、`asp3` ライブラリ・cfg生成・ヘルパ関数（`asp3_add_syssvc` / `asp3_cfg_check`）のみを公開（`cfg1_out` は offset.h 生成に必要なため維持） |

- ブランチ：`feat/library-only`（ベース `9a15203` → コミット `5619f92`、CI 全9ジョブ green を確認して main へ ff マージ）。
- 検証：`linux`（OFF）で `asp` 生成を確認。`mps2_an521_gcc` を `-DASP3_LIBRARY_ONLY=ON` で
  configure・ビルドし `libasp3.a` 生成・`asp` 非生成を確認。

### SDK側（asp3_pico_sdk／別リポジトリ）— submodule移行・ビルド・実機動作確認 完了（2026-06-10）

A案（既存 `asp3_pico_sdk_sample` を再利用）で、旧 `asp3_pico_sdk`（カーネル同梱fork）への
依存を解消し、純カーネルの asp3_core を submodule 参照する構成へ移行。GitHub再編：旧
`asp3_pico_sdk`→`asp3_pico_sdk_legacy`（archive）、`asp3_pico_sdk_sample`→`asp3_pico_sdk`（新・正本）に改名。

| 区分 | 内容 |
|---|---|
| submodule差替 | `.gitmodules` の `asp3`（asp3_pico_sdk）→ `asp3_core`（`asp3_core.git`） |
| pico固有部の移設 | `asp3_pico_sdk.cmake`（`PICO_PLATFORM`→`ASP3_TARGET`/`ASP3_TARGET_DIR`／`ASP3_CORE_DIR`／`irq_*` の `--wrap`）と SDK ターゲット依存部（チップarch は後に asp3_core 共用へ集約） |
| CMake一本化 | `sample1/CMakeLists.txt` の fork CMake を廃止し、`add_subdirectory(asp3_core, ASP3_LIBRARY_ONLY=ON)` ＋ `asp3_add_syssvc()` ＋ `asp3_set_pico_sdk_options()` に一本化 |

検証：pico-sdk 2.1.1 ＋ arm-none-eabi-gcc で `PICO_PLATFORM=rp2350-arm-s` を configure・ビルドし
`sample1_pico_sdk.elf`／`.uf2` 生成・cfg 3パス・`--wrap` 誘導（`__wrap_irq_set_exclusive_handler`）の
リンクを確認。**PICO2 実機**に書込み、UART に起動メッセージ＋`task1 is running ...` の周期出力を確認。
（タイマ競合の定量検証はタスク1、RISC-V対応はタスク2を参照。）

### asp3_pico_sdk側 タスク1：タイマ競合の検証と調停 — 完了（2026-06-11）

#### 現状把握（静的解析）

| コンポーネント | 使用ハードウェアタイマ | 使用アラームチャンネル | IRQ |
|---|---|---|---|
| ASP3 HRT | TIMER0 | **ALARM0** (`RP2350_TIMER0_ALARM0`) | `RP2350_TIMER0_0_IRQn` |
| pico-sdk デフォルト alarm pool | TIMER0 | **ALARM3** (`PICO_TIME_DEFAULT_ALARM_POOL_HARDWARE_ALARM_NUM=3`) | `RP2350_TIMER0_3_IRQn` |

- **アラームチャンネルは分離**されており直接競合なし。
- `busy_wait_us()` / `time_us_64()` は TIMER0 TIMERAWL/H を読み取るのみ（書込みなし）→安全。
- `sleep_ms()` / `add_alarm_*()` は ALARM3 経由 → ALARM0 と衝突しない。
- alarm pool 初期化時の `irq_set_exclusive_handler(TIMER0_3_IRQn, ...)` は `--wrap` 経由で
  ASP3 の `_kernel_exc_tbl[]` に正しく登録される（`target_kernel_impl.c` の
  `__wrap_irq_set_exclusive_handler` が処理）。

#### 定量検証（PICO2 実機・2026-06-11）

**検証プログラム**: `asp3_pico_sdk/sample1/timer_check/`
（`timer_check.c` / `timer_check.cfg` / `CMakeLists.txt`）

**dly_tsk(1000000) 精度計測（10 試行）**

| 試行 | fch_hrt 測定値 (us) | 誤差 (us) | get_tim 測定値 (us) | 誤差 (us) |
|---|---|---|---|---|
| 0 | 1,000,014 | +14 | 1,000,013 | +13 |
| 1 | 1,000,006 | +6 | 1,000,007 | +7 |
| 2–9 | 1,000,006 | +6 | 1,000,006 | +6 |

- 初回のみ +14 us（ログタスク起動オーバーヘッド）、以降は安定して **+6 us (+6 ppm)**。

**get_tim() 単調増加 + ドリフト検証（300 秒）**

| 時刻 | fch_hrt_d (us) | get_tim_d (us) | 累積ドリフト (us) |
|---|---|---|---|
| 10 s | 1,000,006 | 1,000,006 | 68 |
| 20 s | 1,000,006 | 1,000,006 | 142 |
| 150 s | 1,000,006 | 1,000,006 | 974 |
| 300 s | 1,000,006 | 1,000,006 | **1,934** |

- `errors=0`：300 秒間で単調増加違反なし。
- 累積ドリフト 1,934 us / 300 s = **6.45 ppm**（水晶振動子の周波数オフセット）。
- `fch_hrt()` と `get_tim()` の測定値が完全一致 → 同一 TIMER0 TIMERAWL からの導出を確認。

#### 共存設計の結論

**pico-sdk のタイマ API（`sleep_ms` / `add_alarm_*` 等）は現状のまま使用可能**。

ALARM チャンネルが ASP3 (ALARM0) と pico-sdk デフォルト pool (ALARM3) で分離されており、
`--wrap` による IRQ 管理統合も正常動作している。ASP3 カーネル側・pico-sdk 側のいずれも
変更不要。アプリが `add_alarm_*` を使う場合の制約：

1. **alarm pool に ALARM0 を指定しないこと**（`alarm_pool_create(timer0, 0, ...)` の 0 は禁止）。
   デフォルト pool（ALARM3）を使う限り問題なし。
2. `busy_wait_us()` / `sleep_ms()` の呼び出しは割込みコンテキスト外（タスクコンテキスト）で行うこと
   （ASP3 の割込み禁止規約に従う）。
3. 候補(a)「ASP3 HRT を別タイマに逃がす」・候補(c)「SDK タイマ API 禁止」は不要。

これらの内容を `asp3_pico_sdk/` の README または `target/rp2350-arm-s_pico_sdk/` 内
コメントに明記することを推奨。
### asp3_pico_sdk側 タスク2：RISC-V（Hazard3）版 — 完了（2026-06-11）

`PICO_PLATFORM=rp2350-riscv` を実装し、RP2350 の RISC-V コア（Hazard3）で ASP3 を起動。

| 区分 | 内容 |
|---|---|
| ヘルパ分岐 | `asp3_pico_sdk.cmake` の `rp2350-riscv` を実装（`ASP3_TARGET=pico2_riscv_sdk_gcc`／`ASP3_TARGET_DIR` 解決）。`asp3_set_pico_sdk_options()` を ISA で分岐 |
| 移植元 | asp3_core の RISC-V ベアメタル実装（`arch/riscv_gcc/{rp2350,common}`＝Xh3irq・RISC-V IMAGE_DEF、`target/pico2_riscv_gcc`）。arch は asp3_core 側を共用（SDK側 arch の重複は全廃＝ARM-S も asp3_core のチップarchへ集約） |
| シリアルRX | RISC-V では UART RX 割込みを **ASP3 ネイティブ ISR（`target_serial.cfg` の `CRE_ISR`）**で受ける。pico-sdk の `irq_*` 登録は Hazard3 の mtvec 解釈と非互換のため `--wrap` では誘導できない（ARM-S は NVIC のため `--wrap` で誘導） |
| sample統合 | `sample1` を ISA 非依存の単一フォルダ化（`PICO_PLATFORM` で切替。差は割込み禁止命令の `#if __ARM_ARCH` のみ）。VS Code Pico 拡張の「Switch Platform」で ARM↔RISC-V 切替 |

検証（実機・2026-06-11）：拡張同梱の RISC-V toolchain（`RISCV_RPI_2_0_0_5`＝newlib）で
`PICO_PLATFORM=rp2350-riscv` をビルドし、**PICO2 実機**で `task1` 周期出力＋シリアル RX
（`a`→`act_tsk(1)`、`2a`→`act_tsk(2)`）を確認。pico-sdk **2.1.1 / 2.2.0** の両方でビルド確認。

### asp3_core側 ターゲットリネーム — 完了（2026-06-11）

SDK 側ターゲット（`pico2_arm_sdk_gcc` / `pico2_riscv_sdk_gcc`）との対称性のため、
ベアメタル pico2 ターゲットを改称：

- `target/raspberrypi_pico2_gcc/` → `target/pico2_arm_gcc/`（プリセット `raspberrypi_pico2`→`pico2_arm`）
- `target/raspberrypi_pico2_riscv_gcc/` → `target/pico2_riscv_gcc/`（プリセット `raspberrypi_pico2_riscv`→`pico2_riscv`）
- 追随更新：`CMakePresets.json`・`.github/workflows/ci.yml`・`AGENTS.md`・`DIVERGENCE_MAP.md`・`docs/porting/IMPL_INDEX.md`・各 `presets.json`／`run.cmake`／`rpi_pico2.ld`。

> 未完（後続）：①RISC-V 実機での `timer_check` 相当の定量検証（ARM-S は完了）。
> ②skill パッケージ（build/flash/debug）。
> ※OS Awareness の RISC-V 対応（Xh3irq）はベアメタル `pico2_riscv_gcc` で実装・実機確認
> 済み（`docs/dev/pico2-riscv.md`）。SDK統合版での確認のみ後続。
> ※`asp3_core` は public 化済みのため、SDK 利用者の submodule 取得は匿名で可（旧・認証課題は解消）。
