# 実機の叩き方（OpenOCD / シリアル / 移植検証）— asp3_core 固有

実機（SWD/JTAG）でロード実行・テストするときの具体手順。概念（観測の考え方）は
skill `toppers-kernel-debug`。ここは「このリポでの叩き方」に徹する。

## OpenOCD のターゲット cfg（ボード別）

| ターゲット | OpenOCD設定 | ロード手段（run.cmake） |
|---|---|---|
| `raspberrypi_pico2`（ARM・M33） | `target/rp2350.cfg` | `--target run`（program verify reset exit） |
| `raspberrypi_pico2_riscv`（Hazard3） | `target/rp2350-riscv.cfg` | `--target run`（同上．gdbは`gdb-multiarch`） |
| `stm32mp257f_dk_arm64`（A35） | 同梱 `target/.../openocd/openocd.cfg` | `--target swd-run`（FSBLでDDR初期化→a35_0 examine/halt→load→PC=entry→resume） |
| `polarfire_soc_kit`（U54・実機） | SoftConsole openocd | §7（REALBOARD_BRINGUP.md．reset initしない） |

pico2系（ARM/RISC-V）は同じ Debugprobe（CMSIS-DAP・`2e8a:000c`）で書き込む。
**RISC-V側のgdbは `riscv64-unknown-elf-gdb` がaptに無いため `gdb-multiarch` を使う**
（run.cmake の `OSGDB` 既定）。

## OpenOCD の作法（ハマりどころ）

- **掴みっぱなし防止**：OpenOCDプロセスが残るとアダプタを掴んだままになり、次の
  起動が `Unsupported DTM version: -1`（RISC-V）等で失敗する。次を実行する前に
  `pkill -x openocd`（`-f` は付けない。コマンド文字列一致で自分のシェルまで殺す）。
- **シリアルは書き込みより先に開く**：起動直後のバナー／TAP出力の取りこぼし防止。
  `stty -F /dev/ttyACM0 115200 raw -echo` → `timeout N cat /dev/ttyACM0 > log &` →
  その後に OpenOCD で program。
- 実機接続PCの詳細（接続デバイス・udev・sudo要否）はメモリ `jikki-pc-environment` を参照。

## 移植検証テスト（test/porting）を実機で

新ターゲットの最初の動作確認。6項目TAP（詳細は `test/porting/README.md`）。
**`tap.c` を `ASP3_EXTRA_APP_C_FILES` で必ず足す**：

```bash
cmake --preset <preset> -B build/test_porting-<name> \
  -DASP3_APPLDIR=test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=test/porting/tap.c
cmake --build build/test_porting-<name>
# 実機：シリアル捕捉→ run（pico2系）／swd-run（stm32）でロード実行
```

- 合否＝**`# 6/6 passed` 行**のパース（全ターゲット共通。QEMU終了コードに依存しない）。
- linux は ctest 登録済み（`ctest --test-dir build/test_porting-linux`）。
- 故障切り分け順（①ブート/UART→②タイマ歩進→③ディスパッチャ→④⑤カーネル本体→
  ⑥タイマ割込み経路）。落ちた項目→疑う場所は `test/porting/README.md` の表。

## 機能テスト全件（testexec）を実機で

`test/testexec.py` の `TARGET_RUN` に実機ランナを渡す。pico2用ランナは
`scripts/ci/run_board_pico2.sh`（UARTキャプチャ→OpenOCD書込み→完走マーカ待ち。
ARM/RISC-V共用・引数でcfg切替）。CI判定ラッパ `scripts/ci/run_testexec.py` 経由で
TAPサマリを得る：

```bash
pkill -x openocd
scripts/ci/run_testexec.py --options "--preset raspberrypi_pico2_riscv" \
  --run "bash $(pwd)/scripts/ci/run_board_pico2.sh target/rp2350-riscv.cfg 120" \
  --workdir build/testexec-pico2riscv \
  task1 sem1 ... cpuexc1 ... cpuexc10        # テスト名を列挙
```

- **テストバッチを並行実行しない**（ボードと /dev/ttyACM0 を共有→誤判定）。1本ずつ逐次。
- `.ld` や target を編集して再実行するときは **OBJ ディレクトリを消してから**
  （`rm -rf build/testexec-*/OBJ-<NAME>`）。リンカスクリプト変更が再リンクされない
  ことがある（後述の LINK_DEPENDS 対策後も、OBJ流用時は安全側で削除）。
- `cpuexc10` 等は `This test program is not necessary.` → `# SKIP`（正常）。

## OS Awareness（osdebug）を実機で

`--target osdebug`（pico2_riscv / stm32 で定義済み）。OpenOCDを背後で起動し
gdbから `os_awareness.py` を読む。実機では gdb の `continue&`→`interrupt` ではなく
**`monitor resume`→`monitor halt`→`maintenance flush register-cache`** が確実
（gdb側の実行状態とOpenOCDのhaltがずれてレジスタ/メモリを読めないため）。

```
atask   # タスク動的情報（state/stk/runtsk/schedtsk/primap）
stask   # タスク静的情報
intr    # 構成済み割込み（INTNO/pri/attr/ena/pend/handler）
```

- Hazard3 の `intr` は ena/pend を **Xh3irq の窓方式CSR**（meiea/meipa）から
  OpenOCD `riscv exec_progbuf` 経由で読む（`arch/riscv_gcc/rp2350/chip_os_awareness.py`）。
- INTNO の表示はターゲットの番号体系（pico2_riscv は IRQ番号+1、stm32 は GIC INTID）。

## CI の確認（gh）

```bash
gh run list --repo toppers/asp3_core --branch main --limit 3
RUN=$(gh run list --repo toppers/asp3_core --branch main --limit 1 --json databaseId --jq '.[0].databaseId')
gh run watch $RUN --repo toppers/asp3_core --exit-status   # 完了まで待ち＋合否
gh run view $RUN --repo toppers/asp3_core --log-failed     # 失敗ログ
```

- `actions/checkout@v5`・`actions/upload-artifact@v7`（Node24．v5/v4は警告）。
- POSIX の `mutex7` は共有ランナ高負荷で稀にフレーク（per-test timeout 60s に延長済み）。
  単発失敗は `gh run rerun <id> --failed` で再実行して確認。

## 実機検証で踏んだ落とし穴（このリポ実績）

| 症状 | 原因 | 対処 |
|---|---|---|
| 終了時の末尾出力（`# N/N passed`等）が欠落 | UART終了処理がTX FIFOドレイン前にディスエーブル | `cls_por`にFR.BUSY待ち追加（rp2350_uart.c） |
| 初期値付き変数が壊れる（notify1の`count_variable`） | ROM化コピー元を`.text`終端としたためセクションALIGNでLMAが繰上りズレ | リンカで`__idata_start = LOADADDR(.data)` |
| `.ld`を編集しても反映されない | `-Wl,-T`はリンク依存にならない | `LINK_DEPENDS`にldを登録（CMakeLists.txt） |
| pico2_riscv で割込み有効時にハング（`r`キー等） | Hazard3はMEIトラップ入口で優先度スタックをpushしmretでpop。ASP3はmret非経由のディスパッチ経路があり不整合 | popをソフトで一本化（irc_end_int）。MRETEIRQで戻すのは不可 |
| polarfire QEMU が無出力（実機接続PC） | QEMU 8.2.2 は icicle-kit の `-bios none` 直Mモードブート非対応（QEMU 10.1+必要） | CI（ピン留めコンテナ・QEMU 10.2）で確認 |
