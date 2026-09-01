# TTSP3 適合性テスト 適用マニュアル

TOPPERS テストスイート **TTSP3** の API 適合性テスト（`api_test`）を、asp3_core の
各ターゲットに対して **CMake + QEMU** で実行・自動判定するための運用手順書。

- ドライバ：`test/ttsp/run_ttsp.py`（本ディレクトリ）
- 設計・経緯・検証結果：`docs/dev/ttsp3-conformance.md`（**正本**。本書は使い方に特化）
- **TTSP3 は読み取り専用**（一切改変しない）。各ターゲットの TTSP 定義・資産は asp3_core 側の
  **各ターゲット依存部 `target/<t>/ttsp3/`** に置く（ドライバは `target/*/ttsp3/ttsp_target.py` を
  自動探索＝中央の定義表を持たない。ターゲット追加時に本ドライバを編集しない）。

---

## 1. 前提

| 必要物 | 用途 | 備考 |
|---|---|---|
| TTSP3 ソース | テスト供給源 | 既定 `<repo>/../../TTSP3/work/ttsp3`。`--ttsp3-root`/`$TTSP3_ROOT` で指定可 |
| Python 3 | ドライバ実行 | |
| Ruby | `ttg.rb`（functional yaml の生成） | dev コンテナ非同梱＝`apt install ruby` |
| 各ターゲットの toolchain / QEMU | build / run | 下表参照 |

### ターゲット対応表

| `--target` | preset | アーキ | 必要 toolchain | 実行系 | TTSP資産 |
|---|---|---|---|---|---|
| `zybo_z7` | zybo_z7-qemu | Cortex-A9 | arm-none-eabi | qemu-system-arm | **TTSP3同梱**（zybo_z7_gcc） |
| **`zybo_z7_hw`** | **zybo_z7（実機）** | **Cortex-A9** | **arm-none-eabi** | **xsct(Vitis)+実機** | **TTSP3同梱**（zybo_z7_gcc） |
| **`pico2_arm_hw`** | **pico2_arm（実機）** | **Cortex-M33** | **arm-none-eabi** | **OpenOCD/CMSIS-DAP+実機** | `target/pico2_arm_gcc/ttsp3` |
| **`stm32mp257f_dk_arm64_hw`** | **stm32mp257f_dk_arm64（実機）** | **Cortex-A35** | **aarch64-none-elf** | **OpenOCD/SWD+実機** | `target/stm32mp257f_dk_arm64_gcc/ttsp3` |
| `mps2_an505` | mps2_an505-qemu | Cortex-M33 | arm-none-eabi | qemu-system-arm | `target/mps2_an505_gcc/ttsp3` |
| `zcu102_arm64` | zcu102_arm64-qemu | Cortex-A53 | aarch64-none-elf | qemu-system-aarch64 | `target/zcu102_arm64_gcc/ttsp3` |
| `polarfire_soc_kit` | polarfire_soc_kit-qemu | RV64GC | riscv64-unknown-elf | qemu-system-riscv64 | `target/polarfire_soc_kit_gcc/ttsp3` |
| **`polarfire_soc_kit_hw`** | **polarfire_soc_kit（実機 Discovery）** | **U54/RV64GC** | **riscv64-unknown-elf** | **SoftConsole openocd+gdb+実機** | `target/polarfire_soc_kit_gcc/ttsp3` |

> ツールチェーン/QEMU が揃った環境（dev コンテナ `ghcr.io/toppers/asp3_core-dev`）での実行を推奨。
> QEMU が無いアーキは `--build-only` で build 検証のみ可能。
>
> **実機ターゲット（`zybo_z7_hw`）**：QEMU の代わりに xsct(Vitis) で実機にロード＆実行し
> UART(`/dev/ttyUSB1`)で判定する。手順・資産・**実機結果**は各ターゲット依存部フォルダの
> **`target/zybo_z7_gcc/TTSP3_HOWTO.md`**（実機ロード資産は `target/zybo_z7_gcc/ttsp3/`）。
> 実機実行は逐次のみ（`--jobs` 不可）で1件 ~15s のため、staticAPI と代表モジュールの
> スポット確認に用いる（functional 全件は QEMU 版でカバー）。

---

## 2. クイックスタート（zybo_z7・全 HW 対応）

```bash
# 1テストだけ
python3 test/ttsp/run_ttsp.py api_test/ASP/task_manage/act_tsk/act_tsk_a.yaml

# モジュール一括（TAP出力）
python3 test/ttsp/run_ttsp.py --tap api_test/ASP/semaphore

# staticAPI 全件（error系は QEMU 不要で高速）
python3 test/ttsp/run_ttsp.py api_test/ASP/staticAPI

# functional 全件（重い：全 yaml を build+run）
python3 test/ttsp/run_ttsp.py --only yaml --tap api_test/ASP
```

成功時の最終行（stderr）：
```
== TTSP3 [zybo_z7] PASS=… FAIL=0 SKIP=… / … ==
```
終了コードは FAIL があれば 1、無ければ 0（CI gate に利用）。

---

## 3. ドライバの使い方

```
run_ttsp.py [オプション] [テストパス ...]
```

- **テストパス**：TTSP3 ルートからの相対 or 絶対。ディレクトリは再帰探索。
  省略時は `api_test/ASP` 全体。
- `--target <名>`：対応表の名前（既定 `zybo_z7`）。
- `--ttsp3-root <dir>`：TTSP3 ルート（既定 `$TTSP3_ROOT` または `<repo>/../../TTSP3/work/ttsp3`）。
- `--only error|runtime|yaml`：系統を絞る（カンマ区切り可）。
- `--list`：実行せず対象と系統を列挙（件数サマリ付き）。
- `--tap`：TAP 形式（`ok N` / `not ok N` / `ok N # SKIP`）で出力。
- `--build-only`：build 成功＝PASS（QEMU 実行をスキップ）。qemu 未導入アーキ・CI 用。
- `--keep`：build ディレクトリを残す（既定は 1 件ごとに削除）。
- `-v, --verbose`：FAIL 時に build/実行ログを表示。

### テスト3系統（自動分類）

| 系統 | 判定条件 | 実行 | 合格条件 |
|---|---|---|---|
| `error` | dir に `err_code.txt` | configure+build（QEMU不要） | ビルド失敗かつ cfg の `error: E_*` が `err_code.txt` と一致 |
| `runtime` | dir に `out.c`・`err_code.txt`無し | build → QEMU | 出力に `All check points passed.` |
| `yaml` | `*.yaml` | `ttg.rb -a -p` で `out.{c,cfg,h}` 生成 → build → QEMU | 同上 |

---

## 4. 結果の読み方と SKIP の仕組み

PASS / FAIL / SKIP の3値。**SKIP は「本ターゲットで未検証」**を意味する（HW 依存）。
ドライバは3段で SKIP を判定する（`TARGETS` dict の設定で制御）：

1. **モジュール SKIP**（`skip_modules`）：`interrupt`/`exception` を丸ごと除外。
   カーネル API（`ras_int` 等）・割込み/例外設定がターゲット依存のため。
2. **HW呼び出し事前 SKIP**（`unsupported_hw`）：生成 `out.c` が未実装 HW 関数
   （`ttsp_target_gain_tick`＝HWタイマ早送り、`ttsp_int_raise`、`ttsp_cpuexc_raise`）を
   呼ぶテストを **build 前**に除外（`_ntc`/`_ten` 変種・cyclic/alarm/time_event 等）。
3. **soft HW 失敗時再分類**（`soft_hw`）：`stop_tick`/`start_tick`（no-op 実装）依存で
   **実行失敗**したものを SKIP に再分類（通れば PASS）。**ビルド失敗は HW 無関係＝真の FAIL** のまま。

zybo_z7 は全 HW 対応のため SKIP 0。mps2/zcu102/polarfire は上記で HW 依存を SKIP し、
純カーネル系をカバーする。

### 既知の制約

- **staticAPI エラー系のターゲット差**：割込み/例外/ICS/通知の静的API（`CFG_INT`/`DEF_EXC`/
  `DEF_INH`/`DEF_ICS`/`CRE_ALM`/`CRE_CYC` 等）は cfg のエラーコードがターゲット依存で、
  TTSP3 の `err_code.txt`（参照ターゲット基準）と一致せず FAIL になることがある
  （mps2 で 29 件・polarfire で 6 件）。functional 適合性とは別問題。
- **HWタイマ早送り/割込み/例外**は asp3_core 側資産で未実装＝該当テストは SKIP。
  本対応するには各アーキで `gain_tick`/`int_raise`/`cpuexc` を実装する（CLAUDE.md に従い
  生成後の人間確認が必要な別タスク）。

---

## 5. 新ターゲットの追加

QEMU 対応の新ターゲットを横断対象にする手順。

### Step 1：TTSP ターゲット資産を依存部に作る

各ターゲット依存部 `target/<asp3_target>_gcc/ttsp3/` に3ファイルを置く
（既存の mps2/zcu102/polarfire を雛形に）：

- `ttsp_target_test.h`：
  - `RAISE_CPU_EXCEPTION`（アーキの未定義命令／例外発生命令。`arch/<a>/common/core_test.h` と同一に）
  - `TTSP_TASK_STACK_SIZE`/`TTSP_NON_TASK_STACK_SIZE`（64bit系は 4096 目安）
  - 各種異常値・割込み優先度・割込み番号・CPU例外番号（SKIP モジュール用でも**コンパイルが通る値**）
  - `TTSP_SIL_DLY_NSE_TIME`/`TTSP_LOOP_COUNT`/`TTSP_MOD_FCH_CNT`
  - 関数 extern 宣言（下記7関数）
  - **`get_stk`/`get_stksz` マクロは USE_TSKINICTXB を定義するアーキ（arm_m）でのみ定義**。
    非定義アーキ（arm_gcc/arm64/riscv）では書かない（汎用 lib が `TINIB.stk`/`stksz` を直接参照）。
- `ttsp_target_test.c`：7関数を定義。`stop_tick`/`start_tick`/`gain_tick` は no-op で可
  （HW モジュールは SKIP）。`int_raise` は `raise_int` があれば実コール、無ければ no-op。
  `cpuexc_raise` は `RAISE_CPU_EXCEPTION`。`cpuexc_hook`/`clear_int_req` は空。
- `ttsp_target.cfg`：空ファイル（改行1個）。

### Step 2：依存部に TTSP 定義ファイルを置く（中央 `TARGETS` への追記は不要）

同じ `target/<t>_gcc/ttsp3/` に `ttsp_target.py` を置くと、ドライバが
`target/*/ttsp3/ttsp_target.py` を自動探索して取り込む（共有 `run_ttsp.py` は編集不要）：
```python
# target/<t>_gcc/ttsp3/ttsp_target.py
TTSP_TARGETS = {
    "<name>": {
        "preset": "<preset>",
        "ttsp_target": None,            # TTSP3 同梱なら dir 名、無ければ None
        "lib": "target/<t>_gcc/ttsp3",  # 本フォルダ（repo 相対）
        "qemu": "qemu-system-… -kernel {elf} …",   # {elf} を置換
        "unsupported_hw": {"ttsp_target_gain_tick", "ttsp_int_raise", "ttsp_cpuexc_raise"},
        "soft_hw": {"ttsp_target_stop_tick", "ttsp_target_start_tick"},
        "skip_modules": {"interrupt", "exception"},
    },
}
```
実機ターゲットは `qemu: None` とし `hw` キー（kind/serial/jtag_tcl/ps7_init/… ＝既存の
zybo_z7_hw・stm32mp257f_dk_arm64_hw を参照）を追加する。実機ロード資産（jtag.tcl 等）も
本フォルダに置く。

### Step 3：検証

```bash
# 数件で疎通（HW SKIP が効くか・build が通るか）
python3 test/ttsp/run_ttsp.py --target <name> \
  api_test/ASP/task_manage/act_tsk/act_tsk_a.yaml \
  api_test/ASP/staticAPI/CRE_SEM/CRE_SEM_a
# qemu 未導入なら --build-only で build 検証
python3 test/ttsp/run_ttsp.py --target <name> --build-only api_test/ASP
```

> メモリマップ・割込み・例外まわりは誤るとハードフォルトの原因。生成後に人間が確認すること。

---

## 6. CI（nightly）

`/.github/workflows/nightly.yml` の `ttsp3-mps2` ジョブが mps2-an505 で functional 全件を
build+run する（gate＝`--only yaml` の FAIL 0、staticAPI は非gate）。
TTSP3（`toppers/TTSP3`）は public のため、checkout に認証は不要。

---

## 7. トラブルシュート

| 症状 | 原因 / 対処 |
|---|---|
| `'ttsp_target.cfg' not found` 等で build 失敗 | include パスが相対。**絶対パス**で渡す（ドライバは絶対化済。手動実行時は注意） |
| `out.h` の関数が undeclared／別テストが混入 | リポジトリ直下に **stray `out.c`/`out.cfg`/`out.h`** が残存（cwd で `ttg.rb` を誤実行した等）。削除する |
| 大量に SKIP | 仕様（HW 依存テスト）。`--only yaml` の FAIL が 0 なら functional 適合は成立 |
| staticAPI error 系が FAIL | §4 既知の制約（ターゲット依存の cfg エラーコード差） |
