# ビルド手順（CMake）

TOPPERS/ASP3 Core のビルド方法。ビルドシステムは **CMake**（Ninjaジェネレータ）で、
cfg 3パス（静的API抽出 → カーネル構成生成 → 構成チェック）を自動実行する。
コンフィギュレータは Python版（`cfg/cfg.py`）。

> 共通の既定（asp3_core の変更点）
> - **非TECS構成**（TECSは廃止済み）
> - syssvc共通ソース（syslog/banner/serial/serial_cfg/logtask）と
>   ターゲットSIOドライバは自動でリンク対象に入る
> - `-DASP3_OMIT_DEFAULT_SYSSVC=ON` で上記の組込みを抑止（素のカーネルビルド用）

---

## 1. 基本の流れ（例：QEMU mps2-an505）

```bash
# 1. configure（プリセットがビルドフォルダ・toolchain・ターゲットを決める）
cmake --preset mps2_an505-qemu -B build/mps2_an505-qemu

# 2. ビルド（cfg 3パス → libasp3.a → asp.elf → 構成チェックまで自動実行）
cmake --build build/mps2_an505-qemu

# 3. 実行（QEMU 起動。終了は Ctrl-A X）
cmake --build build/mps2_an505-qemu --target run
```

プリセット一覧は `cmake --list-presets` で表示される。

> プリセット定義は**ターゲット依存部のフォルダ**（`target/<name>/presets.json`）に
> あり、ルートの `CMakePresets.json` はそれらを include する一覧のみを持つ
> （共通設定は `cmake/presets-base.json`）。ターゲット追加時の手順は
> `docs/porting/PORTING_GUIDE.md` Step 9-2 を参照。

### make のように簡単に使う（ninja）

ジェネレータが Ninja のため、ビルドフォルダ内では ninja が make の代わりになる。

```bash
cd build/mps2_an505-qemu
ninja        # ビルド
ninja run    # ビルドして実行

# cd したくない場合
ninja -C build/mps2_an505-qemu run
```

buildPresets（`run-linux`／`run-mps2_an505-qemu`／`run-mps2_an386-qemu`／`run-mps3_an547-qemu`／
`run-zybo_z7-qemu`／`run-zcu102_arm64-qemu`／`run-polarfire_soc_kit-qemu`）でルートから
`cmake --build --preset run-mps2_an505-qemu` の1コマンド実行もできる
（一覧は `cmake --list-presets=build`）。

---

## 2. ビルド毎のフォルダ

`-B` で指定するフォルダがビルド毎のフォルダで、いくつでも並存できる（cd 不要）。

```bash
# ターゲット別
cmake --preset mps2_an505-qemu -B build/mps2_an505-qemu
cmake --preset linux    -B build/linux

# 同じターゲットで構成違い（例：テストプログラム）
cmake --preset mps2_an505-qemu -B build/mps2-sem1 \
  -DASP3_APPLNAME=test_sem1 -DASP3_APPLDIR=$PWD/test \
  "-DASP3_EXTRA_APP_C_FILES=$PWD/syssvc/test_svc.c"
cmake --build build/mps2-sem1
```

### プリセットを使わない場合（完全手動）

```bash
cmake -B build/mytest -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DASP3_TARGET=mps2_an505_gcc
cmake --build build/mytest
```

### 主なCMake変数

| 変数 | 意味 |
|---|---|
| `ASP3_TARGET` | ターゲット名（`target/` 配下のディレクトリ名） |
| `ASP3_APPLNAME` | アプリ名（既定：sample1） |
| `ASP3_APPLDIR` | アプリのディレクトリ（既定：`sample/`） |
| `ASP3_APPCFGNAME` | `.cfg` ファイル名がアプリ名と異なる場合に指定 |
| `ASP3_EXTRA_APP_C_FILES` | 追加でビルドするソース（`;`区切り） |
| `ASP3_EXTRA_COMPILE_DEFS` | 追加のマクロ定義（`;`区切り、`-D`なし） |
| `ASP3_OMIT_DEFAULT_SYSSVC` | ONでsyssvcを組み込まない |

### 補足

- 構成は `CMakeCache.txt` に記憶されるため、2回目以降は `cmake --build build/xxx` だけでよい
- 生成物（`kernel_cfg.c/h`・`offset.h` 等）は `build/xxx/generated/` 配下
- 完全クリーンは `rm -rf build/xxx` でフォルダごと削除して configure からやり直すのが確実
- `compile_commands.json` がビルドフォルダに自動生成される
  （`ln -sf build/mps2_an505-qemu/compile_commands.json compile_commands.json` でルートにリンク：AGENTS.md §5）

---

## 3. 実行（QEMU／実機）

### run ターゲット

ターゲットの `target.cmake`（`ASP3_RUN_COMMAND`）が定義する。
未ビルドなら自動でビルドしてから実行する。

```bash
ninja -C build/mps2_an505-qemu run     # QEMU（終了は Ctrl-A X）
ninja -C build/linux run        # ホスト実行
ninja -C build/pico2_arm run    # 実機書込み（OpenOCD program）
```

テストプログラムは `ext_ker` 時にセミホスティングで QEMU が自動終了する。

### osdebug ターゲット（QEMU＋OS-awareness）

QEMU で実行するターゲット（`ASP3_RUN_COMMAND` が qemu-system 系）では、
`osdebug` で QEMU を gdbserver 付き（`-s -S`）で起動し、`gdb-multiarch` から
OS-awareness（`scripts/gdb_os_aware/os_awareness.py`）を読み込んで接続できる。

```bash
ninja -C build/mps2_an505-qemu osdebug
# gdb内: continue →（実行中に）Ctrl-C → atask / stask / sem / cyc / intr 等
```

- コマンド一覧・ターゲット依存部の仕組みは `scripts/gdb_os_aware/README.md` を参照。
- QEMU の出力はビルドディレクトリの `qemu-osdebug.log` に出る。
- linux（ホスト実行）はネイティブ gdb でそのまま使える：
  `gdb ./build/linux/asp -ex 'source scripts/gdb_os_aware/os_awareness.py'`
- 制約: zcu102 は QEMU 8.2 の不具合により `intr` の ena/pend 列を既定無効
  （詳細は `target/zcu102_arm64_gcc/target_os_awareness.py`）。polarfire は
  マルチクラスタ構成のため接続手順を差し替えている（`target.cmake` 参照）。

### 実機デバッグ系ターゲット（pico2／stm32mp257）

`target/<name>/run.cmake` が定義する（`ninja -C build/<dir> <target>`）：

| ターゲット | pico2 | stm32mp257 | 内容 |
|---|---|---|---|
| `run`／`swd-run` | run | swd-run | OpenOCDで書込み・実行 |
| `openocd`・`gdb`・`swd-debug` | ○ | ○ | デバッグセッション |
| `osdebug` | − | ○ | OS-awareness付き（gdb-multiarch．QEMUターゲットの osdebug は上記） |
| `console` | ○ | ○ | UARTコンソール（picocom等自動選択） |

### QEMUを直接起動

```bash
qemu-system-arm -machine mps2-an505 -nographic \
  -semihosting-config enable=on,target=native -kernel build/mps2_an505-qemu/asp.elf

# zybo（UART1が2本目の-serial）
qemu-system-arm -M xilinx-zynq-a9 -semihosting -nographic \
  -serial /dev/null -serial mon:stdio -kernel build/zybo_z7-qemu/asp.elf
```

スクリプトからは `timeout` と入力の送り込みが使える：

```bash
(sleep 3; printf 'r'; sleep 3) | timeout 10 qemu-system-arm -machine mps2-an505 \
  -nographic -semihosting-config enable=on,target=native -kernel build/mps2_an505-qemu/asp.elf
```

---

## 4. テストランナ

```bash
# 移植検証テスト（新ターゲット移植の最初の動作確認．6項目TAP．
# 詳細は test/porting/README.md・docs/dev/porting-test.md）
cmake --preset linux -B build/test_porting-linux \
  -DASP3_APPLDIR=test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=test/porting/tap.c
cmake --build build/test_porting-linux
ctest --test-dir build/test_porting-linux   # linuxのみctest登録（# 8/8 passed照合）

# 機能テスト（QEMU mps2の例）：作業ディレクトリを作って実行
mkdir TEST-MPS2 && cd TEST-MPS2
echo '--preset mps2_an505-qemu' > TARGET_OPTIONS
echo 'timeout 20 qemu-system-arm -machine mps2-an505 -nographic \
  -semihosting-config enable=on,target=native -kernel asp.elf' > TARGET_RUN
python3 ../test/testexec.py sem1 flg1      # build＋exec

# cfgテスト（dummyターゲット）
mkdir TESTCFG && cd TESTCFG
echo '-G Ninja -DASP3_TARGET=dummy_gcc -DASP3_OMIT_DEFAULT_SYSSVC=ON \
  -DASP3_EXTRA_COMPILE_DEFS=TOPPERS_OMIT_SYSLOG' > TARGET_OPTIONS
python3 ../test_cfg/testcfg.py all
```

---

## 5. ターゲット対応状況

**プリセット名＝ターゲット名から `_gcc` を除いたもの**。QEMU/実機の両対応で
条件コンパイルが変わるターゲットは、QEMU側を `<プリセット名>-qemu` とする。

| ターゲット | プリセット | 実行 |
|---|---|---|
| linux_gcc | linux | ホスト実行（run） |
| mps2_an505_gcc | mps2_an505-qemu | QEMU（run．QEMU専用ターゲット．Cortex-M33/ARMv8-M） |
| mps2_an386_gcc | mps2_an386-qemu | QEMU（run．QEMU専用ターゲット．Cortex-M4/ARMv7-M・`__TARGET_ARCH_THUMB=4` 経路検証） |
| mps3_an547_gcc | mps3_an547-qemu | QEMU（run．QEMU専用ターゲット．Cortex-M55/ARMv8.1-M＋MVE） |
| zybo_z7_gcc | zybo_z7／**zybo_z7-qemu** | 実機（xilinx_sdk/jtag.tcl）／QEMU（run） |
| zcu102_arm64_gcc | zcu102_arm64／**zcu102_arm64-qemu** | 実機（実行手段は今後整備）／QEMU（run）．aarch64-none-elf が無い環境は `-DA35_TOOLCHAIN_PREFIX=aarch64-linux-gnu-` を付与 |
| pico2_arm_gcc | pico2_arm | 実機（run=OpenOCD書込み．gdb/console等） |
| mimxrt685evk_gcc | mimxrt685evk | 実機（XIP実行．書込み=MCU-Link（LinkServer等）．手順は`target_user.md`） |
| stm32mp257f_dk_arm64_gcc | stm32mp257f_dk_arm64 | 実機（swd-run/osdebug/console等．リンクは aarch64-none-elf 環境） |
| polarfire_soc_kit_gcc | polarfire_soc_kit／**polarfire_soc_kit-qemu** | 実機（実行手段は今後整備）／QEMU（run）．qemu-system-riscv64 がPATHにない場合は `-DQEMU_SYSTEM_RISCV64=...` を付与 |
| dummy_gcc | （プリセット無し．`-DASP3_TARGET=dummy_gcc`） | cfgテスト用ホストビルド |
| pico2_riscv_gcc | pico2_riscv | 実機（run=OpenOCD書込み（rp2350-riscv.cfg）．gdb/console等）．SDK非依存ベアメタル（Xh3irq） |
| esp32c3_gcc | **esp32c3-qemu**／esp32c3 | QEMU（run）．**Espressif版QEMU（esp32c3マシンを持つfork）が必要**＝PATHにない場合は `-DQEMU_SYSTEM_RISCV32_ESP=...` を付与．ELFでなくフラッシュイメージ（asp_flash.bin＝Direct Boot形式・ポストビルド生成）から起動／実機（run=esptool書込み．ポートは `-DESP32C3_PORT=...`）．実機コンソールは既定でUSB Serial/JTAG（`/dev/ttyACM*`直結．UART配線ボードは `-DESP32C3_CONSOLE=uart0`）．テスト一括は `scripts/ci/run_board_esp32c3.py`（ESP32C3_TTY/ESPTOOL環境変数） |

CMake対応の経緯は `docs/dev/cmake.md` を参照。

---

## 6. 開発コンテナでのビルド

ツールチェーン・QEMU・Pythonをピン留めした開発コンテナ
（`ghcr.io/exshonda/asp3_core-dev`）を用意している。CIも同一イメージで
実行するため、コンテナ内で通ればCIでも通る（設計は `docs/dev/devcontainer.md`）。

### VS Code / Claude Code（devcontainer）

`.devcontainer/devcontainer.json` がイメージ・clangd拡張・postCreateを
定義済み。「Reopen in Container」だけで全ターゲットのbuild→run→test
ループが揃う。

privateリポジトリのGHCRイメージのため、初回は docker login が必要：

```bash
gh auth token | docker login ghcr.io -u <github-user> --password-stdin
```

### docker run で直接使う

```bash
docker run --rm -it -v "$PWD":/workspaces/asp3_core -w /workspaces/asp3_core \
  ghcr.io/exshonda/asp3_core-dev:20260606
# コンテナ内（aarch64-none-elf同梱＝zcu102/stm32もプリセット素のままで通る）
cmake --preset linux -B build/linux && cmake --build build/linux
```

- 非rootユーザ `vscode`（UID 1000）で動作する。ホストのUIDが1000なら
  ボリュームマウントした生成物の所有者問題は起きない
- 同梱ツールのバージョン一覧はコンテナ内 `/etc/asp3_core-toolchain-versions.txt`
- イメージの更新（Dockerfile変更）は `.github/workflows/container.yml` が
  GHCRへ自動push（latest＋日付タグ）。参照側（devcontainer.json・ci.yml・
  nightly.yml・本節）は日付タグを使うため、更新時はタグを一括置換する
