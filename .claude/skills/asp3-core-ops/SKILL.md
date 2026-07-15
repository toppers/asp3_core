---
name: asp3-core-ops
description: asp3_core リポジトリ固有の運用（ビルド・QEMU/実機実行・TAPテスト・構造化ログ(slog)解析・TTSP3適合性テスト・上流マージ台帳）の具体コマンドとツール。CMakeプリセットでビルドするとき、QEMU(mps2_an505/mps2_an386/mps3_an547/zcu102/polarfire 等)で動かすとき、`--tap`/`--slog` を使うとき、`scripts/parse_slog.py`/`check_events.py` でトレースを解析するとき、`test/ttsp/run_ttsp.py` で TTSP3 を回すとき、`DIVERGENCE_MAP.md`/`UPSTREAM_PRISTINE.txt`/`docs/dev/cfg-spec-map.md` を扱うとき、SAFEG(`ENABLE_SAFEG_M`)ビルドを確認するとき、ARM-M実機で性能評価（DWT CYCCNT・`USE_ARM_DWT_PMCNT` で ns 精度の histogram／`PERF_DWT.md`）を行うときに使う。実機(SWD/JTAG)でロード実行・テストするとき＝OpenOCD(rp2350.cfg/rp2350-riscv.cfg)・`--target run`/`swd-run`/`osdebug`・移植検証テスト(`test/porting` の `# 8/8 passed`)・機能テスト全件(`scripts/ci/run_board_pico2.sh` で testexec)・pico2_arm/pico2_riscv/stm32mp257f_dk 実機・`gh run watch` でのCI確認 にも使う。**規約の正本は `AGENTS.md`。TOPPERS共通の概念は別skill `toppers-kernel-dev`/`toppers-kernel-debug`/`toppers-asp`。本skillはasp3_core固有の具体手順だけを補う。**
---

# asp3_core 運用ツール（リポジトリ固有）

このリポジトリ（TOPPERS/ASP3 Core）で実際に使う**具体コマンド・ツール・台帳ファイル**の
早見。概念・規約は他で正本化されているので、ここは「このリポジトリでの叩き方」に徹する。

## 正本・概念へのリンク（先に読む）

- **規約・手順の正本**：このリポジトリの `AGENTS.md`（§4 ビルド・§8 構造化ログ・§10 上流マージ・§2 禁則）。
- **TOPPERS共通の概念**（実装非依存）：
  - カーネル作業規約・上流追従 → skill `toppers-kernel-dev`
  - 不具合切り分け・観測・TTSP3概念 → skill `toppers-kernel-debug`
  - アプリのAPI/静的API/エラー辞書 → skill `toppers-asp`
- 詳細手順：`docs/building.md`（ビルド）・`docs/dev/`（機能ごとの経緯）・`test/ttsp/README.md`（TTSP3）。

---

## 1. ビルド（CMakeプリセット）

```bash
# プリセット一覧
cmake --list-presets            # configure  /  cmake --list-presets=build  # run-*

# 最速の確認（POSIX・ハードなし）
cmake --preset linux -B build/linux && cmake --build build/linux
timeout 5 ./build/linux/asp --tap        # サンプル実行（--slog / --help あり）
ctest --preset linux                     # スモークテスト

# QEMU（例）
cmake --preset mps2_an505-qemu -B build/mps2_an505-qemu && cmake --build build/mps2_an505-qemu
timeout 30 qemu-system-arm -M mps2-an505 -cpu cortex-m33 \
  -kernel build/mps2_an505-qemu/asp.elf \
  -semihosting -semihosting-config enable=on,target=native -nographic
# run-* ビルドプリセットがあれば1コマンド：
cmake --build --preset run-mps2_an505-qemu
```

- **プリセット名＝ターゲット名から `_gcc` を除いたもの**。QEMU/実機両対応はQEMU側を `-qemu`。
- QEMUマシン名はハイフン（`mps2-an505`）、ASP3ターゲット名はアンダースコア（`mps2_an505_gcc`）。
- 対応ターゲット・コマンドの全量は `AGENTS.md` §4 と `docs/building.md`。
- **SAFEG（ARMv8-M TrustZone Dual-OS）**：`-DENABLE_SAFEG_M=ON` で有効化（既定OFF＝素ASP3不変）。
  対象は mps2_an505 / pico2_arm / mimxrt685evk（各 `*_safeg.ld`＋SAU/NS_VTOR を target に同梱）。詳細は `docs/dev/safeg.md`。
  - `-DENABLE_SAFEG_IMPLIB=ON`：gate(`cmse_nonsecure_entry`)を持つアプリで CMSE import lib `secure_nsclib.o` を最終ELFに生成（NS がリンク）。gate無アプリ(sample)では "no symbols" 失敗するため opt-in（既定OFF）。
  - **Secure+NS の遷移テスト(test/) を実機で回す手順**（test_safeg ビルドレシピ／NS の TST_ENABLE／2イメージ書込み／`[TST]` 取得／imxrt685 の RESET自走+capture先行／RP2350 の SWD-lock→UF2／QEMU の INVEP 限界）は **SafeG-M リポの skill `safeg-m-ops`**（asp3_core はベース／ビルド基盤を提供する側）。

## 2. 構造化ログ（slog）でのデバッグ

```bash
# 整形（人間可読）
timeout 5 ./build/linux/asp --slog 2>&1 | python3 scripts/parse_slog.py
# JSON化して期待イベント列と照合（TAP的に判定）
timeout 5 ./build/linux/asp --slog 2>&1 | python3 scripts/parse_slog.py --json > actual.jsonl
python3 scripts/check_events.py test/porting/expected/<name>.json actual.jsonl
```

イベント形式 `T=<tick_us>,EV=<event>,...` と全イベント種別は
[`references/slog-events.md`](references/slog-events.md)（asp3_core 固有の出力仕様）。
切り分けの考え方（症状→原因）は skill `toppers-kernel-debug`。

## 3. TTSP3 適合性テスト

```bash
# functional 全件（ttg生成→build→QEMU）
python3 test/ttsp/run_ttsp.py --target mps2_an505 --tap api_test/ASP
# 静的API
python3 test/ttsp/run_ttsp.py --target mps2_an505 --tap api_test/ASP/staticAPI
```

ドライバ `test/ttsp/run_ttsp.py` のオプション・並列実行・SKIP判定は
[`references/ttsp3-driver.md`](references/ttsp3-driver.md)。経緯は `docs/dev/ttsp3-conformance.md`。
TTSP3 の概念・PASS/FAIL/SKIP の意味は skill `toppers-kernel-debug`。

**実機 HW で回す**（`--target <t>_hw`）：QEMU の代わりにデバッガでロード実行する。
PolarFire SoC Discovery Kit は §7 と `target/polarfire_soc_kit_gcc/TTSP3_HOWTO.md`
（SoftConsole openocd+gdb・reset init なし・staticAPI 123/6/9・semaphore 73/0/44）。

## 4. 上流マージの台帳（このリポジトリの実ファイル）

| ファイル | 役割 |
|---|---|
| `UPSTREAM_VERSION` | 現在ベースの上流ASP3バージョン |
| `DIVERGENCE_MAP.md` | 上流乖離台帳（ファイル単位・PRISTINE/EXTENDED/DIVERGED/NEW） |
| `UPSTREAM_PRISTINE.txt` | 上流そのまま＝上書きしてよいファイル一覧 |
| `kernel/kernel_api.def` | 静的API構造の正本（接頭辞DSL） |
| `docs/dev/cfg-spec-map.md` | cfg（Python）の上流追従台帳（CFG_SPEC_MAP） |

手順は `AGENTS.md` §10、考え方は skill `toppers-kernel-dev`。

## 5. 検証の鉄則（再掲）

- 変更したら**必ず `cmake --build` が通ること**を確認してから報告。
- テストは **TAP（`ok`/`not ok`）** で機械判定。`# SKIP` は想定スキップ。
- 実行結果を根拠に報告する。「動くはず」は禁止。
- `compile_commands.json` をルートにリンク維持：
  `ln -sf build/<preset>/compile_commands.json compile_commands.json`。

## 6. 性能評価（DWT CYCCNT・ns精度 histogram）

ARM-M **実機**で `syssvc/histogram` の計測時間源を DWT CYCCNT サイクルカウンタに
差し替え、ns 精度で実行時間分布を採る。概念・対象ボード・変換式（cycles→ns）・
レジスタは `arch/arm_m_gcc/common/PERF_DWT.md`（このリポ）。汎用の考え方（histogram
の時間源差し替え／エミュレータは性能カウンタを実装しないことが多い）は skill
`toppers-kernel-debug`。

- **オプトインフラグ**：`USE_ARM_DWT_PMCNT`（`arm_gcc` の `USE_ARM_PMCNT` と同型）。
  未指定の通常ビルドは既定の `fch_hrt`（μs精度）＝無影響。
- **実機専用**：QEMU の ARM-M は DWT CYCCNT を実装せず常に 0（`DWT_CTRL.NOCYCCNT=0`
  で「実装あり」に見える罠）。QEMU では指定しない（指定すると全計測 0）。
- **対象**：`core_syssvc.h` を取り込む ARM-M 実機。asp3_core 本体は `pico2_arm` /
  `mimxrt685evk`。SDK統合リポは RA6M5/RA8M2(asp3_fsp)・RT685/MCXN947(asp3_mcuxsdk)・
  pico2_arm_sdk(asp3_pico_sdk)。RISC-V(`pico2_riscv` 等)は DWT 非対象。

```bash
# 例：既存 perf0 を pico2_arm で DWT(ns)計測ビルド（histogram を SYSOBJ 追加）
R=$(pwd)
cmake -S . -B build/perf0-pico2 --preset pico2_arm \
  -DASP3_APPLDIR=$R/test -DASP3_APPLNAME=perf0 \
  "-DASP3_EXTRA_APP_C_FILES=$R/syssvc/test_svc.c;$R/syssvc/histogram.c" \
  "-DASP3_EXTRA_COMPILE_DEFS=USE_ARM_DWT_PMCNT"
cmake --build build/perf0-pico2
# OpenOCD(CMSIS-DAP)で program verify reset exit → UART に ns 単位の分布
#   実測（PICO2/M33/150MHz）：計測オーバヘッド ≈ 140 ns
```

- 新ボードで有効化する手順（target_syssvc.h で `core_syssvc.h` を取り込み、
  `HIST_CONV_TIM` をクロック依存で与える）は `PERF_DWT.md` を参照。
- フラグ off でも `core_syssvc.h` は空＝回帰しない。perf 経路の手早いコンパイル検証は
  `compile_commands.json` の core_kernel_impl.c/histogram.c のコマンドに
  `-DUSE_ARM_DWT_PMCNT -fsyntax-only` を足して再実行（SDK統合リポで実機が無いとき有効）。

## 7. 実機ブリングアップ（PolarFire SoC Discovery Kit / U54・RV64GC）

asp3 は mpfs_hal の `system_startup` を持たず、**HSS が起動時に設定した MSS IO・クロックに
依存**する。実機（JTAG 起動）では次の要点で起こす（詳細・落とし穴は
`target/polarfire_soc_kit_gcc/REALBOARD_BRINGUP.md`）：

- **`-DPOLARFIRE_DISCOVERY=ON`** … Discovery ボード選択（コンソール=MMUART1・L2-LIM リンカ）。
- **L2-LIM(0x08000000) リンク**（`polarfire_soc_kit_lim.ld`）… DDR 学習不要のデバッグロード領域。
- **`reset init` をしない** … `monitor halt`→`load`→PC=`start`→`continue`。reset init は HSS の
  MSS IO 設定を壊し MMUART1 が無音になる。
- **起動時 CLINT MSIP クリア**（`chip_initialize` で実装済み）… HSS 残存の MSIP が MSIE 許可時に
  発火して未登録割込みになるのを防ぐ（FMP3 の `clear_msip` 相当）。

ロード手段は SoftConsole openocd + gdb。HW TTSP3 は §3 と `TTSP3_HOWTO.md`。
TOPPERS 共通の概念（前段ブートFW依存・デバッガ観測・MP→SP の割込み残留）は
skill `toppers-kernel-debug`/`toppers-kernel-dev`。

## 8. 実機ロード実行・移植検証・testexec（pico2/stm32）

新ターゲットを実機で起こす／回帰するときの叩き方は
[`references/onboard-run.md`](references/onboard-run.md)：

- ボード別 OpenOCD cfg と `--target run`/`swd-run` の使い分け
- OpenOCD の作法（`pkill -x openocd`・シリアルは書込み前に開く）
- **移植検証テスト** `test/porting`（`tap.c` を `ASP3_EXTRA_APP_C_FILES` で足す・
  `# 8/8 passed` 判定・故障切り分け順）
- **機能テスト全件** `scripts/ci/run_board_pico2.sh`（testexec の実機ランナ・逐次実行・
  OBJ削除のタイミング）
- **osdebug**（実機は `monitor resume/halt`＋`flush register-cache`）
- `gh run watch` でのCI確認・実機で踏んだ落とし穴（UARTドレイン・ld LMAずれ・
  Hazard3優先度スタック・QEMU版依存）

## references/

| ファイル | 引くタイミング |
|---|---|
| [`references/slog-events.md`](references/slog-events.md) | `--slog` の全イベント種別・キー・読み方（asp3_core固有形式） |
| [`references/ttsp3-driver.md`](references/ttsp3-driver.md) | `run_ttsp.py` のオプション・並列実行・SKIP/FAIL判定の具体 |
| [`references/onboard-run.md`](references/onboard-run.md) | 実機ロード実行・OpenOCD作法・test/porting・testexec実機ランナ・osdebug・落とし穴 |
