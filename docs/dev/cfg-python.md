# cfgのPython化

## 項目

cfgのPython化（AGENTS.md §1 機能追加計画、優先度：高）

## 内容

Rubyベースのコンフィギュレータ（cfg）をPythonで再実装する。
宣言的スペック（データ駆動）設計を推奨とし、AIがコード生成ロジックを追いやすい構造にする。
あわせてビルド時のRuby依存（cfg実行・.trbテンプレート評価）を除去する。

### 方針

cfgは2層に分けて扱う（[`cfg-spec-map.md`](cfg-spec-map.md)＝CFG_SPEC_MAP。初出は `docs/asp3_derivative_plan.md` §8.1）：

- **層① 静的API定義（api-table）＝ `kernel/kernel_api.def`・`*_sym.def`**
  接頭辞DSL（`#`=ID, `.`=符号無し, `+`=符号付き, `&`=一般, `^`=ポインタ, `$`=文字列,
  後置 `*`=キー `?`=オプション `...`=リスト）のテキスト。**上流と同形式のまま維持**し、
  Python版エンジンがこれをそのまま読む（YAML等への二重定義は持たない）。
  上流の静的API追加はこのファイルのテキストマージで追従できる。
- **層② エンジン＝ `cfg.py` / `pass1.py` / `pass2.py`（Ruby→Python移植）**
  api-tableを解釈してコード生成する汎用エンジン。テキスト差分が効かない
  **DIVERGED** 部分であり、上流 cfg.rb 系の挙動変更時は CFG_SPEC_MAP の
  対応表（最終確認バージョン）で手動再反映する。
- **生成テンプレート（旧 `.trb`）**：パス2/3でRubyスクリプトとして評価される
  `target_kernel.trb`・`target_check.trb`（target）、`core_offset.trb` 等（arch）も
  Python化の対象（`target_kernel.py`・`target_check.py`・`core_offset.py`）。

### 現状

- `cfg/` は**上流Ruby版のまま**：`cfg.rb`（751行）・`pass1.rb`（986行）・
  `pass2.rb`（552行）・`GenFile.rb`（106行）・`SRecord.rb`（235行）、計約2,630行。
- ビルドはMakefileから `cfg.rb` を3パスで起動している（TECSレス化後のビルドで動作確認済み）：
  - パス1：`.cfg`＋api-table → `cfg1_out.c`（静的APIの抽出・パラメータ評価）
  - パス2：`cfg1_out` 実行結果＋ `.trb` → `kernel_cfg.c/h`・`offset.h` 等の生成
  - パス3：ロードモジュール（シンボル・ROMイメージ）＋ `target_check.trb` → 構成チェック
- 各 `target/<name>/` に `target_kernel.trb`・`target_check.trb`、各archに
  `core_offset.trb` 等の `.trb` テンプレートが存在する。
- `DIVERGENCE_MAP.md` には `cfg/cfg.py`・`pass1.py`・`pass2.py`（エンジン）と
  `*.py` 生成テンプレートの行が**計画として記載済み**（実体は未作成）。

### ベース実装（asp3_fsp）

**[asp3_fsp](https://github.com/toppers/asp3_fsp) の実装済みPython版cfgをベースとする。**

- Python版エンジン：`asp3_fsp/main` の `asp3/cfg/`
  （`cfg.py`・`pass1.py`・`pass2.py`・`gen_file.py`・`srecord.py`、計約2,626行。**cfg 1.7.0ベース**）
- ベースとしたRuby版 cfg 1.7.0：`asp3_fsp` の `step1-complete` タグの `asp3/cfg/`
- 本リポジトリのRuby版は **cfg 1.7.1**（`cfg/MANIFEST`）。
  1.7.0→1.7.1のエンジン差分は小規模（diff行数：cfg.rb 12・pass1.rb 30・pass2.rb 77）
- Python版は `.trb` 生成テンプレートを **Pythonコードとして `exec`** する設計。
  そのため `.trb` 群のPython構文版が必要で、asp3_fsp には以下が変換済み：
  - `kernel/`：`kernel.py`・`kernel_check.py`・`genoffset.py`＋オブジェクト別11本（計14本。
    本リポジトリの `kernel/*.trb` 14本と1対1対応）
  - `arch/arm_m_gcc/common/`：`core_offset.py`・`core_check.py`
  - `target/`：`ek_ra6m5`・`ek_ra8m2` の `target_kernel.py`・`target_check.py`（変換の規範）

### 関連記述

- `docs/asp3_derivative_plan.md` §3：「RubyベースのコンフィギュレータをPythonで再実装。宣言的スペック（データ駆動）設計推奨」
- `docs/asp3_derivative_plan.md` §8.1：CFG_SPEC_MAP節（層①／層②の分離とマージ運用）
- `AGENTS.md` §7：cfg（Pythonコンフィギュレータ）の構成と運用
- `DIVERGENCE_MAP.md`：cfgエンジン・生成テンプレートの乖離行（計画）

## 実施プラン

asp3_fsp の実装済みコードをベースに、以下の手順で進める。

1. **エンジンの導入**
   - `asp3_fsp/main` の `asp3/cfg/`（`cfg.py`・`pass1.py`・`pass2.py`・`gen_file.py`・`srecord.py`）を
     本リポジトリの `cfg/` に追加する。
   - Ruby版（`cfg.rb` 系）は残置する（生成物比較・フォールバック用）。削除は別項目「ファイルの削除」で実施。
2. **cfg 1.7.0→1.7.1 差分の反映**
   - Ruby 1.7.0（`asp3_fsp` の `step1-complete` タグ）と本リポジトリの Ruby 1.7.1 の diff を抽出し、
     エンジンの挙動変更を Python 版へ手動反映する（`GenFile.rb`・`SRecord.rb` の差分も確認）。
   - Python版の `VERSION` を 1.7.1 に更新する。
3. **生成テンプレート（.trb → .py）の整備**
   - `kernel/`：asp3_fsp の `kernel/*.py` 14本を取込む。ベースが1.7.0のため、
     1.7.1 の `kernel/*.trb` との差分を確認のうえ反映する。
     **kernel/ への新規ファイル追加にあたるため、DIVERGENCE_MAP.md に記録する**（禁則①の手続き。
     既存ファイルの編集はしない）。
   - `arch/`：
     - `arm_m_gcc/common`（mps2_an521・pico2用）：asp3_fsp の `core_offset.py`・`core_check.py` を流用。
       `core_kernel.trb` 相当のPython版が asp3_fsp に見当たらないため、FSPでの扱いを調査のうえ
       必要なら新規変換。
     - `arm_gcc/common`・`arm_gcc/zynq7000`（zybo用）、`arm64_gcc/common`・`arm64_gcc/stm32mp2`
       （stm32mp257f用）、`arm_gcc/rza1`（gr_peach用）、`posix_gcc`（linux用）：**新規にPython変換**。
     - `core_*_v6m.trb`（ARMv6-M用）は使用ターゲットが無いため対象外（必要時に変換）。
   - `target/`：全8ターゲットの `target_kernel.trb`・`target_check.trb`（＋`dummy_gcc/target_offset.trb`）を
     Python変換する（asp3_fsp の `ek_ra8m2/target_*.py` を規範とする）。
4. **ビルド統合**
   - `configure.rb` の `CFG` デフォルトを `ruby cfg/cfg.rb` → `python3 cfg/cfg.py` に変更
     （`-g` オプションでRuby版にも切替可能なまま）。DIVERGENCE_MAP に記録。
   - `sample/Makefile` の `.trb` 参照（`TARGET_KERNEL_TRB` 等）を `.py` に切替える
     （asp3_fsp の `sample/Makefile` を規範に最小差分で）。
5. **検証**
   - **生成物の一致確認**：各ターゲットで Ruby版／Python版それぞれの生成物
     （`cfg1_out.c`・`kernel_cfg.c/h`・`offset.h`）を diff し、一致（または説明可能な差のみ）を確認。
   - **動作確認（linux＋QEMU）**：
     - linux_gcc：`obj/obj_linux` でビルド・実行（sample1動作）
     - mps2_an521_gcc／zybo_z7_gcc：ビルド警告ゼロ＋QEMU実行
       （sample1・シリアル入力 `r` でタスク切替・configuration check passed）
   - その他ターゲットはビルド確認まで：pico2／ct11mpcore／gr_peach／dummy はリンク・check まで、
     stm32mp257f は開発機ではコンパイル＋pass2生成物diffまで（リンク以降は実機側マシン）。
6. **記録・後処理**
   - `DIVERGENCE_MAP.md`：cfgエンジン行の実体化（最終確認バージョン cfg 1.7.1）・`kernel/*.py` 追加行・
     `configure.rb`／`Makefile` 変更を記録。
   - [`cfg-spec-map.md`](cfg-spec-map.md)（CFG_SPEC_MAP）の対応表（最終確認バージョン）を更新。
   - 本ファイルの実施結果を記載し、`docs/dev/README.md` の状態を更新。コミット・プッシュ。

## 実施結果

（2026-06-05 記載）

（注：2026-06-16 に QEMU の M33 ターゲットを mps2-an505/IoTKit へ置換〔現ターゲット名 mps2_an505_gcc〕。FPU が CPU0 に実装されハード浮動小数点で再検証可能になった。本ファイル中の `mps2_an521` / `mps2-an521`（実施プランおよび下記の検証結果・生成物一致確認）は置換前の記録。以降の M33 QEMU 検証は mps2-an505 を使用。）

### 変更したファイル

| ファイル | 変更内容概要 |
|---|---|
| `cfg/pass1.py`・`cfg/pass2.py`（導入後に修正） | cfg 1.7.0→1.7.1差分を移植（オブジェクト識別名の重複チェックをpass1→pass2へ移動・VALIDフラグ方式化）。pass1.pyはcfg1_out.cの空行欠落も修正（Ruby版とバイト一致） |
| `kernel/kernel.py`・`kernel_check.py`・`interrupt.py`（取込み後に修正） | 1.7.1差分（OMIT_ISTK/TOPPERS_ISTKPT条件化・SYMBOL(...,True)化）を反映。interrupt.pyはFSP実装のlambdaスコープバグ（CRE_ISRでUnboundLocalError）を修正 |
| `configure.rb` | CFGデフォルトを `python3 cfg/cfg.py` に変更（`-g` でRuby版選択可） |
| `sample/Makefile`・`arch/{arm_m,arm,arm64}_gcc/common/Makefile.core`・`target/{dummy,simtimer_ct11mpcore}_gcc/Makefile.target` | 生成テンプレート参照を `.trb`→`.py` に切替（arm_mのv6m系は未変換のため.trbのまま） |
| `target/{ct11mpcore,gr_peach,dummy}_gcc/Makefile.target` | 非TECS SIOオブジェクト追加（非TECSデフォルト化で顕在化したTECSレス未整備分） |
| `DIVERGENCE_MAP.md`・`docs/asp3_derivative_plan.md`（CFG_SPEC_MAP節） | cfg関連の乖離行を実体化・最終確認バージョンを cfg 1.7.1 に更新 |

### 追加したファイル

- エンジン（asp3_fsp/main 由来）：`cfg/{cfg,pass1,pass2,gen_file,srecord}.py`（Ruby版は残置）
- kernelテンプレート（asp3_fsp由来・kernel/への新規追加）：`kernel/*.py` 14本
  （kernel・kernel_check・genoffset＋alarm/cyclic/dataqueue/eventflag/exception/
  interrupt/mempfix/mutex/pridataq/semaphore/task）
- archテンプレート（新規変換。arm_mのcore_offset/core_checkのみasp3_fsp流用）：
  - `arch/arm_m_gcc/common/{core_kernel,core_offset,core_check}.py`
  - `arch/arm_gcc/common/{core_kernel,core_offset,core_check}.py`・
    `arch/arm_gcc/zynq7000/chip_kernel.py`・`arch/arm_gcc/rza1/chip_kernel.py`
  - `arch/arm64_gcc/common/{core_kernel,core_offset,core_check,gic_kernel}.py`・
    `arch/arm64_gcc/stm32mp2/chip_kernel.py`
  - `arch/posix_gcc/{posix_kernel,posix_check}.py`
- targetテンプレート（新規変換）：全9ターゲットの `target_kernel.py`・`target_check.py`
  ＋ `target/dummy_gcc/target_offset.py`

### 削除したファイル

なし（Ruby版エンジン・`.trb` は残置。削除は別項目「ファイルの削除」で実施）

### Git情報

- ベースコミット：`5d9eb9d`（cfg-python計画記載）
- 関連コミット範囲：`9653b52`（エンジン＋kernel取込み）〜`c4f2c03`（テンプレート変換＋デフォルト切替）
- ファイルリスト再現コマンド例：`git diff --stat 5d9eb9d HEAD -- cfg kernel arch target configure.rb sample/Makefile`

### 検証結果

**生成物の一致確認**：全9ターゲット（linux/mps2_an521/zybo_z7/raspberrypi_pico2/
ct11mpcore/gr_peach/dummy/stm32mp257f_dk）で、Ruby版とPython版の
`kernel_cfg.c`・`kernel_cfg.h`・`offset.h`・`cfg1_out.c` が**完全一致**（diff 0行）。

| テスト | 実施 | 結果 |
|---|---|---|
| linux_gcc（実行） | ○ | デフォルト設定でビルド・check passed・sample1動作 |
| QEMU mps2-an521（実行） | ○ | ビルド警告なし・check passed・sample1動作・`r`入力でタスク切替（CRE_ISR生成経路含む） |
| QEMU xilinx-zynq-a9（実行） | ○ | 同上 |
| raspberrypi_pico2（ビルド） | ○ | check passed |
| ct11mpcore／gr_peach／dummy（ビルド） | ○ | check passed |
| stm32mp257f_dk（実行） | ○ | pass1/2生成物一致・リンク・pass3成功・**実機でsample1動作**（バナー・task1・`r`でタスク切替．2026-06-06実機接続PCで確認） |

### DIVERGENCE_MAP との関連

- cfgエンジン・`kernel/*.py`（kernel/への新規追加）・arch/targetテンプレート・
  `configure.rb`/Makefile類の乖離行を記録（最終確認バージョン cfg 1.7.1）
- CFG_SPEC_MAP節（`docs/asp3_derivative_plan.md` §8.1）の対応表も更新

## 削除対象ファイル（「ファイルの削除」項目で実施）

Python化の完了に伴い不要となるRuby版：

- エンジン：`cfg/cfg.rb`・`cfg/pass1.rb`・`cfg/pass2.rb`・`cfg/GenFile.rb`・`cfg/SRecord.rb`
- 生成テンプレート（`.trb`）一式：
  - `kernel/*.trb`（14本）
  - `arch/arm_m_gcc/common/core_{kernel,offset,check}.trb`・`core_{kernel,offset}_v6m.trb`
    （v6m系は`.py`未変換のまま削除．必要になった場合は上流から取得して変換）
  - `arch/arm_gcc/common/core_{kernel,offset,check}.trb`・`arch/arm_gcc/zynq7000/chip_kernel.trb`・
    `arch/arm_gcc/rza1/chip_kernel.trb`
  - `arch/arm64_gcc/common/core_{kernel,offset,check}.trb`・`gic_kernel.trb`・
    `arch/arm64_gcc/stm32mp2/chip_kernel.trb`
  - `arch/posix_gcc/posix_{kernel,check}.trb`
  - `target/*/target_{kernel,check}.trb`・`target/dummy_gcc/target_offset.trb`
