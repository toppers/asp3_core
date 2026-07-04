# AGENTS.md

> **TOPPERS/ASP3 Core**（リポジトリ：`asp3_core`）における、全AIコーディングツール共通の正本。  
> Claude Code / Cline / Cursor 等のツール固有ファイル（`CLAUDE.md` / `.clinerules` / `.cursorrules`）は、本ファイルを参照すること。  
> このファイルが規約・手順の**唯一の正本（single source of truth）**である。

> 📋 プロジェクト全体像（ASP3からの変更点・AI向け記述・その他）は `docs/OVERVIEW.md` を参照。

---

## 1. プロジェクト概要

TOPPERS/ASP3カーネルを上流追従しながら、各社SDK（Raspberry Pi Pico SDK / Renesas FSP / STM32 HAL）と協調動作させる共通カーネル基盤。μITRON4.0系のシングルコアRTOS。

| 項目 | 内容 |
|---|---|
| 名称 | **TOPPERS/ASP3 Core**（リポジトリ `asp3_core`、ソース識別子 `asp3`） |
| ベース | TOPPERS/ASP3（手動マージで上流追従） |
| API | TOPPERS API互換（識別子プレフィックス `asp3`） |
| 第一目的 | 各社SDKとの協調動作（4リポジトリを共通基盤に統合） |
| ライセンス | TOPPERSライセンス（改変版である旨を明示。「TOPPERS/」名は規則第4条＝運営委員会承認マター） |
| 対象ターゲット | ARMv8-M(Cortex-M33) / ARMv8-A(Cortex-A35) / RISC-V(Hazard3, ESP32-C3) ＋ POSIX / QEMU（3 ISAとも実機対応済み：PICO2(ARM/RISC-V)・STM32MP257F-DK・ESP32-C3） |

### 機能追加計画

上流ASP3に対して本リポジトリで追加・変更する機能（詳細・経緯は `docs/asp3_derivative_plan.md` §3 を参照）。

| 項目 | 内容 | 優先度 |
|---|---|---|
| TECSレス | syssvcをプレーンCで実装。tecsgenへの依存を除去。AI向けにコードの間接層が消え追いやすくなる | 高 |
| cfgのPython化 | RubyベースのコンフィギュレータをPythonで再実装。宣言的スペック（データ駆動）設計推奨 | 高 |
| .rbツールの.py化 | RubyのツールをPythonに書き換える | 高 |
| CMake対応 | CMakeListsおよびasp3_pico_sdk.cmakeで実装済み | 高 |
| ファイルの削除 | 不要なファイルを削除 | 高 |
| QEMUターゲット(ARMv8-A) | ZCU102依存部を追加してサポートする | 高 |
| QEMUターゲット(RISC-V) | Polafire依存部を追加してサポートする | 高 |
| QEMUターゲット(ARMv7-M) | mps2-an386（Cortex-M4）依存部を追加。ARMv7-M（非TrustZone・FPv4-SP）の動作検証用。`__TARGET_ARCH_THUMB=4` 経路の確認 | 中 |
| QEMUターゲット(ARMv8.1-M) | mps3-an547（Cortex-M55/SSE-300）依存部を追加。ARMv8.1-M＋MVE(Helium) の検証用。asp3_fsp@30bf318 の MVE VPR 退避を共通archへ復活（RA8M2積み残しの解消） | 中 |
| CLIターゲット | POSIX sim。エージェントのbuild→run→testループ用 | 高 |
| CI整備 | GitHub Actions：全ターゲットbuild＋POSIX/QEMUテスト実行 | 高 |
| OS Awareness 対応 | OS Awareness を全ターゲット使用可能とする | 高  |
| 移植検証テスト | 新ターゲット移植の最初の動作確認（カーネル基本6項目・TAP機械判定）。test/porting/ の実体化 | 高 |
| RISC-V Hazard3ターゲット | PICO2のRISC-Vコア（Hazard3）をSDK非依存ベアメタルで対応（pico2_arm_gccのRISC-V版） | 高 |
| ドキュメントMarkdown化 | 統合仕様書・APIリファレンスをMarkdown化（RAG対応） | 中 |
| devcontainer / Docker | ツールチェーン・QEMU・Pythonをピン留めした開発コンテナ。開発機/CI/エージェント環境の再現性を確保 | 中 |
| Pico SDK統合 | pico-sdkと協調動作（第一目的の第1弾）。SDK固有のarch/targetは外側リポジトリ(asp3_pico_sdk)で管理し、asp3_coreは`ASP3_TARGET_DIR`で受け入れる | 中 |
| FSP統合 | Renesas FSPと協調動作（第一目的の第2弾）。**完了**（2026-06-12）：外側リポジトリ(asp3_fsp)で管理＝`ASP3_TARGET_DIR`。EK-RA6M5/EK-RA8M2 実機検証済み。ツールチェーンはLLVM-ARM/clang・RASC生成依存 | 中 |
| STM32 HAL統合 | STM32Cube HALと協調動作（第一目的の第3弾）。**完了**（2026-06-12）：外側リポジトリ(asp3_stm32cube)で管理＝`ASP3_TARGET_DIR`。旧世代asp3(TECS/Ruby)から非TECS+Python cfg化を実施。NUCLEO-H563ZI/H533RE 実機検証済み（test_porting 6/6・testexec）。toolchainはarm-none-eabi gcc・CubeMX生成依存 | 中 |
| NXP MCUXpresso SDK統合 | NXP MCUXpresso SDKと協調動作（第一目的の第4弾・EVK-MIMXRT685/CM33）。Phase A＝ベアメタルターゲットをasp3_core本体へ（genuine 3.7.0移植を非TECS+Python cfg変換・SDK不要・CIビルド可）→Phase B＝外側リポジトリでSDK統合 | 中 |
| ESP-IDF統合 | Espressif ESP-IDFと協調動作（第一目的の第5弾・ESP32-C3/RV32IMC）。ESP-IDFはFreeRTOS前提のためesp-hal-3rdparty方式（Zephyr/NuttXの前例）で下層hal/soc層とのみ協調。Phase A＝ベアメタルESP32-C3ターゲットをasp3_core本体へ（Espressif QEMU fork対応・CIビルド可）→Phase B＝外側リポジトリ(asp3_esp_idf)でesp-hal統合＋Wi-Fi os_adapter shim→Phase C＝lwIP統合（DHCP＋実通信＋BSDソケット互換）→Phase D＝Bluetooth（BLE）統合（NimBLE＋os_adapter shim．実施中＝コントローラ起動まで実機成功，`emi.c`内部アサートで停止・原因調査中）。Phase C**完了**（2026-07-04）：実機でDHCP取得・ゲートウェイping・BSDソケットAPI（socket/bind/listen/accept/recv/send）でのTCP/UDP通信成功。経緯は`docs/dev/esp-idf-integration.md` | 中 |
| ESP32-C6ターゲット | ESP-IDF統合のESP32-C6横展開（RV32 HP×1＋LP×1・Wi-Fi 6／BLE／802.15.4）。C3との差分＝割込みコントローラがPLIC_MX命名のレジスタ（当初CLICと誤認・訂正済み）・クロック制御がPCRペリフェラルへ移動・IROM/DROM分離なしのリンカマップ。`feat/esp32c6`ブランチ。**実機でtest_porting 6/6 PASS（160MHz動作確認・較正済み）**（2026-07-04）：Direct Boot・WDT無効化・mie CSR初期化（C3と異なり実機でmie/mipアクセスが正常＝要修正だった旧仮定）・割込み駆動USB Serial/JTAGコンソール・SYSTIMER/HRT割込み動作確認。PCRクロック切替はROM起動時点で既に160MHz（SPLL÷3÷1）設定済みと判明＝レジスタ書換え不要。SIL_DLY_TIM1/2較正（30/12）・dlynse 17件OK確認。**Phase A完了**：test_porting 6/6・**testexec 35/36 PASS**（`target_spec.yaml`／`DIVERGENCE_MAP.md`／`docs/porting/IMPL_INDEX.md`とも整備済み。唯一の失敗＝`int1`＝CFG_INTの3エントリ目以降が実機でどの物理線／FROM_CPUソース組合せでも配送されないハードウェア級の未解決issue）。QEMU（Espressif fork v9.2.2）はesp32c6マシン未実装＝実機専用（CIジョブ見送り）。残＝int1原因調査・LPコア相互作用確認。経緯は`docs/dev/esp32c6-target.md` | 高 |
| skillパッケージ | 移植ガイドskill。**完了**（2026-06-12）：各SDKリポジトリ内 `.claude/skills/` に実装（asp3_fsp=porting-asp3-to-renesas-ra・asp3_stm32cube=porting-asp3-to-stm32。picoは不要と判断） | 中 |
| メモリ保護 | Zephyr相当のセーフティネット型保護（コードRO・SRAM実行禁止・NULL検出．スタック下限保護=PSPLIMは実装済み）。MPU(arm_m)/MMU(arm64)/PMP(riscv)で静的設定・kernel/無改変 | 中 |
| SAFEG（ARMv8-M TrustZone） | 上流SafeG（A-profileデュアルOS）のCortex-M（TrustZone-M）版を arm_m 共通へ取り込み。ASP3をセキュア側に置きNS側を隔離。既定OFF（`ENABLE_SAFEG_M`）で素ASP3不変・kernel/無改変（`#ifdef TOPPERS_SAFEG_M`ガード）。an505(QEMU)/pico2/mimxrt685evk。経緯は`docs/dev/safeg.md` | 中 |
| TTSP3 適合性テスト | TOPPERSテストスイートTTSP3のAPI/SIL適合性テストをQEMUで各ターゲットに対し実行。TTSP3不変＝asp3_coreのCMake+QEMUで回す外部ドライバ方式（経緯は`docs/dev/ttsp3-conformance.md`） | 中 |

### 機能追加の実施ルール

> 目的：変更点を明らかにし、他のTOPPERS系RTOSで同様の作業をする際の参考とすること。

- 機能追加項目ごとに `docs/dev/<項目スラッグ>.md` を作成する。ファイル名は英小文字ケバブケース（例：`tecs-less.md`、`cfg-python.md`）とし、`docs/dev/README.md` の索引表で上記「機能追加計画」の項目名と対応付ける。
- 各.mdの構成は **項目／内容／実施プラン／実施結果** とする。
  - **実施プランは着手前に書き、実施結果は完了時に書く**（セッションを跨ぐ作業の再開点を兼ねる）。
  - 実施結果には以下を含める：
    - 変更したファイルのリストと各変更内容の概要
    - 追加・削除したファイル
    - Git情報：ベースコミット・関連コミット範囲・ファイルリストを再現するコマンド例（例：`git diff --stat upstream main -- <paths>`）
    - 検証結果：テストの実施範囲（POSIX / QEMU / 実機）と結果
- `docs/dev/README.md` には次を置く：
  - 索引表（項目名・ファイル・状態〔計画中／実施中／完了〕）
  - 各.mdのテンプレート
- `DIVERGENCE_MAP.md` との役割分担：DIVERGENCE_MAPは**ファイル単位**の上流乖離台帳、`docs/dev/` は**機能単位**の経緯・手順書。PRISTINE領域（`kernel/` 等）に変更が及んだ項目は相互にリンクする。

---

## 2. ⚠️ 禁則事項（作業前に必読・最重要）

### 禁則①：`kernel/` 配下を直接編集しないこと

`kernel/`・`include/`・`library/` は上流ASP3コアをほぼそのまま維持する領域。  
直接編集すると上流との手動マージコストが増大し、追従が破綻する。

```
✗ kernel/task.c を編集する
✗ include/kernel.h を編集する
✓ target/<name>/ に追加実装する
✓ syssvc/ に新しいシステムサービスを追加する
✓ 新規ファイルを追加する
```

やむを得ずコアに変更が必要な場合は、**作業を止めてユーザーに確認**し、`DIVERGENCE_MAP.md` に記録してから実施すること。

### 禁則②：カーネル内で動的メモリ確保を使用しないこと

```c
✗ malloc(), calloc(), realloc(), free()
✗ new / delete（C++）
✓ 静的配列
✓ 固定長メモリプール（get_mpf / rel_mpf）
```

ASP3の安全設計方針（ISO 26262 / IEC 61508への適合性維持）。一般Cの習慣で動的確保を入れないこと。

---

## 3. ディレクトリ構成と変更種別

```
kernel/            [PRISTINE]  上流ASP3コア。編集禁止（kernel_api.def / *_sym.def 含む）
include/           [PRISTINE]  上流ヘッダ。編集禁止
library/           [PRISTINE]  上流ライブラリ。編集禁止
arch/              [EXTENDED]  アーキ依存（arm_m_gcc/=Cortex-M(chip: rp2350, imxrt600), arm_gcc/=Cortex-A9(chip: zynq7000), arm64_gcc/=AArch64(chip: stm32mp2, zynqmp), riscv_gcc/=RV32IMAC/RV64GC(chip: rp2350=Xh3irq, polarfire_soc=PLIC)）
syssvc/            [EXTENDED]  TECSレス版システムサービス（上流 extension/non_tecs/syssvc 由来。ローカル発明ではない）
cfg/               [DIVERGED]  Python コンフィギュレータ・エンジン（上流はRuby cfg.rb/pass1.rb/pass2.rb）
target/            [NEW]       ターゲット依存部（SDK統合はここ）
asp3_core.cmake    [NEW]       CMakeエントリ（ASP3_ROOT_DIR設定・TARGET選択・ヘルパ関数）
cmake/             [NEW]       ツールチェーンファイル（-mcpu等のコンパイラフラグ）
test/              [NEW]       テストスイート（TAP形式）
scripts/           [NEW]       ログパーサ等の自動化スクリプト
docs/              [NEW]       APIリファレンス・エラー辞書・計画・マージ台帳
```

| 種別 | 意味 | マージ時の扱い |
|---|---|---|
| PRISTINE | 上流そのまま | 上流版で上書き（人手不要） |
| EXTENDED | 上流＋追加 | 上流変更を確認し取り込む |
| DIVERGED | 意図的に変更 | DIVERGENCE_MAPに従い要確認 |
| NEW | 新規追加 | 上流と衝突なし |

---

## 4. ビルド & テスト

### ターゲット別コマンド

```bash
# POSIX CLI（ハードなし・最速確認）
cmake --preset linux -B build/linux && cmake --build build/linux
timeout 5 ./build/linux/asp            # サンプル実行（--tap/--slog/--helpあり）
ctest --preset linux                   # スモークテスト（sample1実行＋slog照合）

# QEMU Cortex-M33（mps2-an505・ハードなし）
cmake --preset mps2_an505-qemu -B build/mps2_an505-qemu && cmake --build build/mps2_an505-qemu
timeout 30 qemu-system-arm -M mps2-an505 -cpu cortex-m33 \
  -kernel build/mps2_an505-qemu/asp.elf \
  -semihosting -semihosting-config enable=on,target=native -nographic

# QEMU Cortex-A53（xlnx-zcu102・ハードなし）
cmake --preset zcu102_arm64-qemu -B build/zcu102_arm64-qemu && cmake --build build/zcu102_arm64-qemu
timeout 30 qemu-system-aarch64 -machine xlnx-zcu102,secure=on -nographic \
  -semihosting-config enable=on,target=native -kernel build/zcu102_arm64-qemu/asp.elf

# QEMU RISC-V RV64GC（microchip-icicle-kit・ハードなし）
cmake --preset polarfire_soc_kit-qemu -B build/polarfire_soc_kit-qemu && cmake --build build/polarfire_soc_kit-qemu
timeout 30 qemu-system-riscv64 -machine microchip-icicle-kit -nographic \
  -semihosting-config enable=on,target=native -bios none -kernel build/polarfire_soc_kit-qemu/asp.elf

# QEMU RISC-V RV32IMC（esp32c3・ハードなし・Espressif版QEMUが必要）
# ELFではなくフラッシュイメージ（asp_flash.bin＝Direct Boot形式・ポストビルド生成）から起動する
cmake --preset esp32c3-qemu -B build/esp32c3-qemu && cmake --build build/esp32c3-qemu
timeout 30 qemu-system-riscv32 -M esp32c3 -nographic -semihosting \
  -drive file=build/esp32c3-qemu/asp_flash.bin,if=mtd,format=raw

# Raspberry Pi PICO2 (Cortex-M33 / 実機)
cmake --preset pico2_arm -B build/pico2_arm && cmake --build build/pico2_arm

# Raspberry Pi PICO2 (RISC-V Hazard3 / 実機)
cmake --preset pico2_riscv -B build/pico2_riscv && cmake --build build/pico2_riscv

# NXP EVK-MIMXRT685 (Cortex-M33 / 実機)
cmake --preset mimxrt685evk -B build/mimxrt685evk && cmake --build build/mimxrt685evk

# STM32MP257F-DK (Cortex-A35 / 実機)
cmake --preset stm32mp257f_dk_arm64 -B build/stm32mp257f_dk_arm64 && cmake --build build/stm32mp257f_dk_arm64
```

### テスト実行順序（必ずこの順で）

```
1. POSIX        ← ハードなし・最速。まずここで確認
2. QEMU(M33)    ← M-profileの動作確認
3. 実機         ← 最後に実ハードで確認
```

> **前提メモ**
> - ビルドは **CMakeのみ**（Makefile版は廃止済み。詳細は `docs/building.md`）。**プリセット名＝ターゲット名から `_gcc` を除いたもの**（QEMU/実機両対応のターゲットはQEMU側を `<プリセット名>-qemu`）。一覧は `cmake --list-presets`。
> - `linux` プリセットの実体は **ホストシミュレーション**（`target/linux_gcc`＝上流SVNの `asp3_arch_posix_gcc` パッケージ由来）。
> - QEMUマシン名は `mps2-an505`（ハイフン）だが、ASP3ターゲット名は `mps2_an505_gcc`（アンダースコア）。
> - ツールチェーン・QEMUをピン留めした**開発コンテナ**（`ghcr.io/exshonda/asp3_core-dev`・`.devcontainer/`）があり、CIも同一イメージで実行する。手順は `docs/building.md` §6、設計は `docs/dev/devcontainer.md`。

### 検証の鉄則

- **コードを変更したら必ず `cmake --build` でコンパイルが通ることを確認してから報告すること。**
- テスト結果はTAP形式（`ok N` / `not ok N`）で判定する。
- 「動くはず」で報告しない。ビルド・テストの実行結果を根拠とすること。

---

## 5. compile_commands.json

ビルド後に自動生成される（`CMAKE_EXPORT_COMPILE_COMMANDS=ON` 既定）。  
コード解析・補完の精度に直結するため、ルートへのリンクを維持すること。

```bash
ln -sf build/linux/compile_commands.json compile_commands.json
```

クロスコンパイルのインクルードパスは `.clangd` が補正済み。  
clangd起動時は `--query-driver="/usr/bin/arm-none-eabi-gcc*"` を付与すること。

---

## 6. コーディング規約

### 命名規約（TOPPERS流）

| 種別 | 規則 | 例 |
|---|---|---|
| 関数 | 小文字＋アンダースコア | `act_tsk`, `wai_sem` |
| 型 | 大文字（typedef） | `ID`, `ER`, `T_CTSK`, `PRI` |
| マクロ・定数 | 大文字＋アンダースコア | `TMIN_TPRI`, `E_OK`, `TA_NULL` |
| カーネル内部関数 | プレフィックス維持 | 上流の命名に従う |

### 必須ルール

- **静的確保のみ**（禁則②参照）。
- **カーネル内で再帰を使わない**（スタック境界が保証できない）。
- エラーは戻り値 `ER` 型で返す（例外を投げない）。
- サービスコールの戻り値 `E_*` は必ず確認する。
- 浮動小数点はタスクコンテキストでのみ使用（割り込みハンドラ内禁止、FPU退避コスト回避）。

### MISRA-C

安全クリティカル領域を意識し、MISRA-C:2012サブセット相当を守る。  
CIのstatic-analysisジョブが `cppcheck`（syssvc/・target/・arch/＝PRISTINE除外）と
`clang-tidy`（POSIXのcompile_commands.json対象）でチェックする
（当面warning運用＝non-blocking。レポートはartifacts。経緯は `docs/dev/ci.md`）。
生成コードがこれに違反しないこと。

---

## 7. cfg（Pythonコンフィギュレータ）

静的構成ファイル（`.cfg`）からカーネル構成コード（`kernel_cfg.c/h`・`offset.h`）を生成する。  
**CMakeビルドが3パスで自動実行する**ため、通常は手動起動しない。

cfgは3要素に分かれ、要素ごとにマージ難易度が異なる：

| 要素 | 実体 | マージ性 |
|---|---|---|
| 静的API定義（api-table） | `kernel/kernel_api.def`・`*_sym.def` | 上流と同形式 → **テキストマージ可** |
| エンジン | `cfg/cfg.py`・`pass1.py`・`pass2.py` | Ruby→Python移植 → **DIVERGED** |
| 生成テンプレート | `core_offset.py`（arch）・`target_kernel.py`・`target_check.py`（target、旧.trb） | DIVERGED |

- 静的APIの構造（パラメータ・型）の**正本は `kernel/kernel_api.def`**（接頭辞DSL：`#`=ID, `.`=符号無し, `+`=符号付き, `&`=一般, `^`=ポインタ, `$`=文字列, 後置 `*`=キー `?`=オプション `...`=リスト）。YAML等の二重定義は持たない。
- 新しい静的APIは `kernel_api.def` の行追加で対応（テキストマージ可）。
- エンジン（cfg.py系）が上流 cfg.rb の挙動変更を受けたときのみ手動再反映が必要。対応は `docs/dev/cfg-spec-map.md`（CFG_SPEC_MAP）で管理する。

---

## 8. 構造化ログ

トレースログ機能（`ASP3_ENABLE_TRACE=ON`・linuxプリセットは既定ON）を
有効にしたビルドは、以下の行指向フォーマットでカーネルイベントを出力できる
（linux_gccでは実行時オプション `--slog` で有効化。実装は `arch/tracelog/`）。

```
T=<tick_us>,EV=<event>[,<key>=<val>]*
```

例：
```
T=4881,EV=TSK_STAT,ID=2,STAT=RUNNABLE
T=4890,EV=SVC_ENTER,API=act_tsk,A1=3
T=4899,EV=SVC_LEAVE,API=act_tsk,RET=0
T=5200,EV=ERR,API=wai_sem,CODE=-18
```

主なイベント：`TSK_STAT`（タスク状態変化）／`TSK_RUN`（ディスパッチ先）／
`SVC_ENTER`・`SVC_LEAVE`（サービスコール出入り）／`ERR`（エラー復帰）／
`INH_*`・`ISR_*`・`CYC_*`・`ALM_*`・`EXC_*`（ハンドラ出入り）。
一覧は `arch/tracelog/trace_slog.c` 冒頭コメントを参照。

解析・検証：
```bash
timeout 5 ./build/linux/asp --slog 2>&1 | python3 scripts/parse_slog.py   # 人間可読
timeout 5 ./build/linux/asp --slog 2>&1 | python3 scripts/parse_slog.py --json \
  > actual.jsonl
python3 scripts/check_events.py test/porting/expected/sample1.json actual.jsonl
```

エラーコード（`E_*` / 負値）の意味は `docs/errors.md` を参照すること。

---

## 9. Git 開発フロー

詳細は `docs/FILE_PLAN.md` を参照。要点：

### ブランチ構成

```
upstream  ← ASP3タルボールをそのまま取り込むブランチ（差分基準点）
main      ← 作業ブランチ（常にビルド・テストが通る状態を維持）
feat/*    ← 機能ブランチ（複数セッション/破壊的変更時のみ）
```

### 機能ブランチを切る基準

- 切る：複数セッションにまたがる作業、既存ターゲットを壊すリスク、新規ターゲット追加、テスト/CI大幅変更
- mainに直接：ドキュメント更新、小規模修正、整形、CI微調整

### コミットメッセージ

```
<type>(<scope>): <summary>

type:  feat / fix / docs / test / chore / refactor
scope: target / syssvc / cfg / cmake / upstream / ci

例: feat(target): add mps2-an505 QEMU target
    chore(upstream): merge ASP3 3.8.0
```

---

## 10. 上流マージ支援手順

上流ASP3の新バージョンとのマージを支援する場合、以下に従うこと：

1. `UPSTREAM_VERSION` で現在のベースバージョンを確認する。
2. `DIVERGENCE_MAP.md` を読み、変更種別・影響範囲を把握する。
3. `git diff upstream main` と上流diffを `DIVERGENCE_MAP` に照合し、影響ファイルを列挙する。
4. `UPSTREAM_PRISTINE.txt` に記載のファイルは上流版で上書き更新してよい。
5. `DIVERGENCE_MAP` に該当するファイルは**「要人間確認」とマークして停止**すること。
6. cfgに関わる変更は `docs/dev/cfg-spec-map.md`（CFG_SPEC_MAP）を参照する（静的API追加なら `kernel_api.def` のマージで足りる／エンジン変更なら手動再反映）。
7. 変更後、POSIX → QEMU(mps2-an505) の順で回帰テストを実行する。
8. マージ完了後に `UPSTREAM_VERSION` を更新する。

---

## 11. 新ターゲットの追加

新しいプロセッサ/ボードへの移植は **`docs/porting/PORTING_GUIDE.md`** に従うこと。

1. `target/<name>/target_spec.yaml` をテンプレートから作成・記入する。
2. `PORTING_GUIDE.md` のStep順に実装する（target.cmake は変数積み上げ＋arch.cmake include）。
3. 各Stepの「✅ 確認」を通過してから次に進む。
4. 完了後、`DIVERGENCE_MAP.md`・`target/<name>/presets.json`（＋ルート`CMakePresets.json`のinclude追記）・`.github/workflows/ci.yml`・`docs/porting/IMPL_INDEX.md` を更新する。

メモリマップ・割り込み設定・コンテキストスイッチは誤るとハードフォルトの原因になるため、**生成後に必ず人間の確認を求めること**。

---

## 12. 主要API早見表

詳細は `docs/api/<サービスコール名>.md`。

| API | 説明 |
|---|---|
| `act_tsk(tskid)` | タスクを起動 |
| `wup_tsk(tskid)` | タスクを起床 |
| `slp_tsk()` | 自タスクを起床待ち |
| `dly_tsk(dlytim)` | 時間待ち（μs） |
| `sig_sem(semid)` / `wai_sem(semid)` | セマフォ シグナル / 待ち |
| `set_flg(flgid, setptn)` / `wai_flg(...)` | イベントフラグ セット / 待ち |
| `snd_dtq(dtqid, data)` / `rcv_dtq(dtqid, p_data)` | データキュー 送信 / 受信 |
| `get_tim(p_systim)` | システム時刻取得（μs） |
| `fch_hrt()` | 高分解能タイマ取得（μs） |
| `loc_cpu()` / `unl_cpu()` | CPUロック / 解除 |

静的API（`.cfg`に記述）：`CRE_TSK` / `CRE_SEM` / `CRE_FLG` / `CRE_DTQ` / `CRE_ALM` / `CRE_CYC` / `DEF_INH` / `CRE_ISR` ほか。構造の正本は `kernel/kernel_api.def`。

---

## 13. 参照ファイル索引

作業内容に応じて以下を読むこと：

| やりたいこと | 読むファイル |
|---|---|
| **プロジェクト全体像** | **`docs/OVERVIEW.md`** |
| ビルド・テスト方法 | 本ファイル §4 + `docs/building.md`（CMakeビルドの詳細手順） |
| 新ターゲット移植 | `docs/porting/PORTING_GUIDE.md` + `target_spec.yaml` |
| 既存実装の参照 | `docs/porting/IMPL_INDEX.md` |
| 機能追加の実施記録・経緯 | `docs/dev/README.md` + 本ファイル §1「機能追加の実施ルール」 |
| 上流マージ | `DIVERGENCE_MAP.md` + `UPSTREAM_PRISTINE.txt` + 本ファイル §10 |
| cfg仕様の追従 | `docs/dev/cfg-spec-map.md`（CFG_SPEC_MAP） |
| API仕様 | `docs/api/` |
| エラーコードの意味 | `docs/errors.md` |
| フォルダ構成・Git運用 | `docs/FILE_PLAN.md` |
| プロジェクト全体計画 | `docs/asp3_derivative_plan.md` |

---

## 14. SDK統合リポジトリ（外側リポジトリ）

4リポジトリとも **asp3_core を submodule（`asp3/asp3_core`）参照し、SDK固有の
arch/target/アプリ/移植skill を外側で管理**する構成（`ASP3_TARGET_DIR` 方式）。
統合の経緯は `docs/dev/{pico-sdk,fsp,stm32,nxp}-integration.md` を参照。

| リポジトリ | 内容 | 実機検証 |
|---|---|---|
| [asp3_pico_sdk](https://github.com/exshonda/asp3_pico_sdk) | Raspberry Pi Pico SDK統合（RP2350） | PICO2（ARM/RISC-V） |
| [asp3_fsp](https://github.com/exshonda/asp3_fsp) | Renesas FSP統合（RA・LLVM/clang＋RASC）＋移植skill | EK-RA6M5／EK-RA8M2 |
| [asp3_stm32cube](https://github.com/exshonda/asp3_stm32cube)（旧 stm32_vscode_asp） | STM32Cube HAL統合（STM32H5・CubeMX）＋移植skill。STM32MP257/A35ターゲットの移植元でもある | NUCLEO-H563ZI／H533RE |
| [asp3_mcuxsdk](https://github.com/exshonda/asp3_mcuxsdk) | NXP MCUXpresso SDK統合（i.MX RT685）。**Phase A・Phase B（SDK統合）とも完了・実機検証済**（`docs/dev/nxp-integration.md`） | EVK-MIMXRT685（Phase A：test_porting 6/6・testexec 33/35＝cpuexc1/4は arm_m 既知FAIL・dlynse較正・OS Awareness／Phase B：test_porting 6/6・testexec 33/36 PASS＝cpuexc1/4既知FAIL） |
| [asp3_esp_idf](https://github.com/exshonda/asp3_esp_idf) | ESP-IDF統合（ESP32-C3）。esp-hal-3rdparty方式＝FreeRTOS非依存のhal/soc層のみ使用。B-0/B-1（LL層コンソール・タイマ）・B-2（Wi-Fi os_adapter shim・scan＋WPA2接続）・C（lwIP・DHCP＋ping＋BSDソケット互換）とも完了（`docs/dev/esp-idf-integration.md`） | ESP32-C3実機（B-1：test_porting 6/6・testexec 36/36／B-2：AP16-17個スキャン・STA_CONNECTED成立／C：DHCP取得・ゲートウェイping成功・BSDソケットTCP通信成功） |

> asp3_core 側を変更したら、各リポジトリの submodule を bump して追従させること。
