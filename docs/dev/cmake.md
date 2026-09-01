# CMake対応

## 項目

CMake対応（AGENTS.md §1 機能追加計画、優先度：高）

## 内容

CMakeビルドシステムを導入し、`cmake --preset` による閉ループ検証（build→run→test）と
各社SDK（pico-sdk / FSP）との統合の基盤とする。Makefileビルドは残置する
（削除は「ファイルの削除」項目）。

### ベース実装

- **[asp3_fsp](https://github.com/toppers/asp3_fsp)**：`asp3_fsp.cmake`（エントリ：
  `ASP3_ROOT_DIR`・`ASP3_TARGET`）＋ `CMakeLists.txt`（284行。cfg.py 3パスの
  custom_command パイプライン・`libasp3.a`・pass3チェック関数 `asp3_cfg_check`）＋
  `target/<name>/target.cmake`（変数積み上げ＋`arch.cmake` include）
- **[asp3_pico_sdk](https://github.com/toppers/asp3_pico_sdk)**：同系構成で
  pico-sdk 統合（`PICO_PLATFORM` 連動・`asp3_set_pico_sdk_options`）
- 両者とも Python版cfg・`.py` テンプレート前提であり、本リポジトリの
  cfgのPython化（完了）とそのまま接続できる

### 現状

- リポジトリには `CMakePresets.json` が配置済み（posix / m33-qemu / pico2-m33 /
  pico2-riscv / stm32mp257-a35。`ASP3_TARGET` 変数と `cmake/toolchain-*.cmake` を参照）
- `CMakeLists.txt`・`asp3_core.cmake`・`cmake/`・各 `target.cmake`／`arch.cmake` は**未作成**
- AGENTS.md §3 に設計が記載済み：`asp3_core.cmake` [NEW]（ASP3_ROOT_DIR設定・
  TARGET選択・ヘルパ関数）、`cmake/` [NEW]（ツールチェーンファイル）

## 実施プラン

（ユーザー提示の進め方をベースに、改善提案①〜⑤を反映）

（注：2026-06-16 に QEMU の M33 ターゲットを mps2-an505/IoTKit へ置換〔現ターゲット名 mps2_an505_gcc〕。FPU が CPU0 に実装されハード浮動小数点で再検証可能になった。本ファイル中の `mps2_an521` / `mps2-an521`（実施プランおよび下記の追加ファイル一覧）は置換前の記録。現行パスは `target/mps2_an505_gcc/target.cmake`。以降の M33 QEMU 検証は mps2-an505 を使用。）

1. **共通基盤の追加**（asp3_fsp をベースに汎用化）
   - `asp3_core.cmake`：エントリ（`ASP3_ROOT_DIR`・`ASP3_TARGET` 既定とターゲット存在確認）
   - `CMakeLists.txt`：cfg 3パスパイプライン＋ `libasp3.a` ＋ `asp3_cfg_check`。
     FSP固有変数（`RASC_*`）への依存を除去して汎用化。サンプルアプリ（sample1→ `asp`）の
     ビルドもルートで行う（AGENTSクイックスタートの実体化）
   - `cmake/toolchain-arm-none-eabi.cmake`（既存プリセットが参照する名前に合わせる）
   - `arch/arm_m_gcc/common/arch.cmake`、`target/mps2_an521_gcc/target.cmake`
2. **mps2-an521 で動作確認**
   - `cmake --preset m33-qemu -B build/m33-qemu && cmake --build build/m33-qemu`
   - ①生成物一致検証：CMakeビルドの `kernel_cfg.c/h`・`offset.h`・`cfg1_out.c` を
     Makefileビルドの生成物と diff（cfgパイプライン移植の正しさを機械的に確認）
   - ④QEMU実行ターゲット：`cmake --build build/m33-qemu --target run` で
     QEMU起動（Makefileの `make run` 相当）。sample1動作・configuration check passed を確認
3. **開発者レビュー**：ファイルを確認いただき、指示があれば対応
4. **他ターゲット展開**
   - ②posix（linux_gcc）：`--preset posix` → `./build/posix/asp` 実行
     （AGENTSクイックスタートがそのまま通る状態にする）
   - ③zybo_z7：プリセットを追加（既存presetに無いため）→ QEMU実行確認
   - stm32mp257-a35：`cmake/toolchain-a35.cmake` 追加→開発機ではコンパイル＋生成物diffまで
     （リンク・実機は実機側マシン）
   - ⑤pico2-m33：pico-sdk 必須のため、SDKが利用可能な環境での手順を整備
     （本マシンに無い場合はビルド要件の記録まで）。pico2-riscv は arch/riscv_gcc
     未実装のため**スコープ外**（プリセットは残置）
5. **記録・後処理**
   - `DIVERGENCE_MAP.md`（CMakeLists.txt・cmake/ 行の実体化）、AGENTS.md §4 前提メモの
     更新（CMake移植済みターゲットの明記）、`compile_commands.json` リンク（AGENTS §5）、
     本ファイルの実施結果、`docs/dev/README.md` 状態更新。コミット・プッシュ

### 改善提案（プランに反映済み）

| # | 提案 | 理由 |
|---|---|---|
| ① | Makefileビルドとの生成物diff一致検証 | cfgパイプラインのCMake移植の正しさを目視でなく機械的に確認（cfg Python化と同じ手法） |
| ② | posixプリセットも本項目で対応 | AGENTSクイックスタート（`--preset posix`）の実体化。エージェントの最速ループが完成 |
| ③ | 既存CMakePresets.jsonに準拠＋zyboプリセット追加 | プリセット名・toolchainファイル名の不整合を防ぐ。QEMU 2ターゲット体制の維持 |
| ④ | QEMU実行のCMakeターゲット化（`--target run`） | build→run の閉ループをCMakeだけで完結 |
| ⑤ | pico2はpico-sdk要件を明記して段階対応、pico2-riscvはスコープ外 | SDK依存・アーキ未実装の現実を計画に反映 |

## 実施結果

（2026-06-05 記載。pico2のみpico-sdk環境での後続対応）

### 追加したファイル

| ファイル | 内容 |
|---|---|
| `asp3_core.cmake` | エントリ（ASP3_ROOT_DIR／ASP3_TARGET検証・`asp3_add_syssvc()`ヘルパ） |
| `CMakeLists.txt` | cfg 3パスパイプライン・`libasp3.a`・サンプルアプリ`asp`・`asp3_cfg_check()`・`run`ターゲット。offset.h無しターゲット対応・`ASP3_START_FILES`（START_OBJS相当）対応 |
| `cmake/toolchain-arm-none-eabi.cmake` | Cortex-M/A用（実行ファイルは`.elf`） |
| `cmake/toolchain-a35.cmake` | AArch64用（`A35_TOOLCHAIN_PREFIX`で代用ツールチェーン指定可） |
| `arch/{arm_m_gcc,arm_gcc,arm64_gcc}/common/arch.cmake`・`arch/posix_gcc/arch.cmake` | コア依存部 |
| `arch/arm_gcc/zynq7000/chip.cmake`・`arch/arm64_gcc/stm32mp2/chip.cmake` | チップ依存部 |
| `target/{mps2_an521,linux,zybo_z7,stm32mp257f_dk_arm64}_gcc/target.cmake` | ターゲット依存部（変数積み上げ＋arch/chip include） |

### 変更したファイル

| ファイル | 変更内容概要 |
|---|---|
| `CMakePresets.json` | `zybo-qemu`プリセット追加・buildPresets（`run-posix`/`run-m33-qemu`/`run-zybo-qemu`）追加 |
| `docs/building.md`・`AGENTS.md` §4前提メモ | 対応状況の反映 |

### Git情報

- ベースコミット：`e08b2de`（cmake計画）
- 関連コミット範囲：`5a57728`（共通基盤＋mps2）〜`c3ce334`（posix/zybo/stm32展開）
- ファイルリスト再現コマンド例：`git diff --stat e08b2de HEAD -- '*.cmake' CMakeLists.txt CMakePresets.json cmake`

### 検証結果

**生成物の一致確認**：CMakeビルドの `kernel_cfg.c/h`・`offset.h` がMakefileビルドと
**完全一致**（mps2／posix／zybo／stm32の4ターゲット。`cfg1_out.c`はパス表記差のみ）。

| ターゲット（プリセット） | 実施 | 結果 |
|---|---|---|
| m33-qemu | ○ | ビルド（FLASH 22,720B≒Makefile版22,484B）・`--target run`でQEMU実行・`r`入力タスク切替・pass3チェック実行 |
| posix | ○ | ビルド・実行（バナー・task1動作）。AGENTSクイックスタートが実体化 |
| zybo-qemu | ○ | ビルド・QEMU実行・`r`入力タスク切替 |
| stm32mp257-a35 | △ | `aarch64-linux-gnu`代用で`libasp3.a`まで＋生成物一致。**リンク・実機はaarch64-none-elf環境で実施のこと** |
| pico2-m33 | ○ | **対応済み（2026-06-06追補）**。raspberrypi_pico2_gccは**ベアメタル移植でpico-sdk不要**と判明（当初評価を訂正）。ビルド＋生成物一致（RAM同値）。`--target run`=OpenOCD書込み。実機確認は実機接続PCで |
| pico2-riscv | − | スコープ外（arch/riscv_gcc未実装） |

### 残課題

- ~~pico2のCMake対応~~ → **対応済み（2026-06-06）**。pico-sdk統合版（rp2350-arm-s_pico_sdk）は将来のSDK統合項目で
- dummyのCMake対応（cfgテスト用。必要になった時点で。Makefile版は利用可能）
- ctest統合（QEMUテストのテスト化）はCI整備項目で扱う

### DIVERGENCE_MAP との関連

- `CMakeLists.txt`・`cmake/`の行（NEW）が実体化。`*.cmake`（arch/target）行を追加

## 削除対象ファイル（「ファイルの削除」項目で実施・条件付き）

CMake対応の完了に伴い不要となるmake版ビルドファイル：

- `sample/Makefile`（テンプレート）
- `target/*/Makefile.target`（全ターゲット）
- `arch/*/common/Makefile.core`・`arch/*/*/Makefile.chip`
- `configure.py`（Makefileビルドの入口．make版全廃と同時）
  ※`configure.rb` は .rbツール項目で削除

### ⚠️ 削除の前提条件（フェーズ2として別途実施）

以下が解消されるまで削除できない：

1. ~~pico2のCMake対応が未完~~ → **解消（2026-06-06）**
2. ~~testcfg.py／testexec.py がMakefileビルドに依存~~ → **解消（2026-06-06．
   両ツールをCMakeベースに改修．dummy_gccもtarget.cmake追加でCMake対応）**
3. ~~実機側ワークフロー（swd-run等）~~ → **解消（2026-06-06．run.cmakeで
   swd-run/osdebug/console等をCMakeターゲット化）**

**→ 前提条件はすべて解消し、フェーズ2（make版削除）を実施済み（2026-06-06）。
ビルドはCMakeのみ。削除の実施記録は `file-cleanup.md` の実施結果（フェーズ2）を参照。**
