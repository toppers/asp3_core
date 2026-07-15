# PORTING_GUIDE.md — TOPPERS/ASP3 Core 新ターゲット移植ガイド

> **AIへの指示**：このガイドと `target_spec.yaml` を読み、Step 1から順番に実施せよ。  
> 各Stepの「✅ 確認」が通過するまで次のStepに進まないこと。  
> 不明な点は作業を止めてユーザーに質問すること。

---

## 前提・移植対象ファイル一覧

新ターゲット `target/<name>/` に作成するファイル：

```
target/<name>/
├── target_spec.yaml         ← ハードウェア記述（AIへの入力・本リポジトリ独自）
├── target_config.h          ← メモリマップ・割り込み設定・基本マクロ
├── target_kernel.h          ← カーネル設定（スタックサイズ等）
├── target_kernel_impl.h     ← カーネル内部実装設定（arch/との接続）
├── target_kernel_impl.c     ← カーネル初期化・ターゲット固有処理
├── target_timer.h           ← HRTタイマ設定
├── target_timer.c           ← HRTタイマ実装（tick生成）
├── target_serial.h          ← シリアル設定
├── target_serial.c          ← シリアル実装（syslog出力）
├── target_syssvc.h          ← システムサービス設定
├── target_kernel.cfg        ← ターゲットの静的構成（cfg）
├── target_kernel.py         ← pass2 kernel_cfg生成テンプレート（旧.trb）
├── target_check.py          ← pass3 チェックテンプレート（旧.trb）
└── target.cmake             ← ターゲット依存部の登録（変数積み上げ＋arch.cmake include）
```

> ファイル名・構成は `target/pico2_arm_gcc/`（実機ベアメタル）を参照。`target_kernel.py` /
> `target_check.py` は旧Ruby cfg の `.trb` テンプレートをPython化したもの。

**変更してはいけないファイル**：`kernel/` 配下のファイルは一切編集しないこと。

---

## 参照実装インデックス

迷ったときに参照すべき既存実装：

| 実装したいもの | 参照ターゲット | 参照ファイル |
|---|---|---|
| SysTickによるHRTタイマ | `mps2_an505_gcc` | `target_timer.c` |
| NVICによる割り込み制御 | `mps2_an505_gcc` | `target_config.h`, `target_kernel.c` |
| Pico SDK統合タイマ／UART | 外部リポジトリ `asp3_pico_sdk` | （SDK統合は別リポジトリ＝`ASP3_TARGET_DIR`方式。本リポジトリのベアメタル版は `pico2_arm_gcc`） |
| GICv3割り込み制御 | `stm32mp257f_dk_arm64_gcc` | `target_kernel.c` |
| Cortex-A35アーキテクチャ | `stm32mp257f_dk_arm64_gcc` | `arch/arm64_gcc/` |
| RISC-V コンテキストスイッチ | `pico2_riscv_gcc` | `arch/riscv_gcc/common/core_support.S` |
| POSIXシミュレーション | `linux_gcc` | 全ファイル |
| セミホスティングシリアル | `mps2_an505_gcc` | `target_serial.c` |

---

## Step 0：準備

### 0-1. target_spec.yaml の作成

`target/<name>/target_spec.yaml` をテンプレートからコピーして記入する。  
AIはデータシートから以下を読み取って記入を補助できる：

- **必須**：メモリマップ（flash/SRAM アドレス・サイズ）
- **必須**：タイマペリフェラル（tick生成用）
- **必須**：UARTペリフェラル（シリアル出力用）
- **重要**：割り込みコントローラ（NVIC/GIC）設定
- **任意**：クロック設定（SDKが行う場合はその旨記載）

### 0-2. ディレクトリ作成

```bash
cp -r target/_template target/<name>
cd target/<name>
# target_spec.yaml を配置済みであることを確認
```

### 0-3. 参照ターゲットの確認

`target_spec.yaml` の `reference.closest_target` に記載した既存ターゲットを  
全ファイル把握してから作業を開始すること。

---

## Step 1：target_config.h — 基本マクロ・メモリマップ

### 目的
カーネルが使用するメモリ配置・割り込み優先度・タイマ設定を定義する。

### AIへの指示
`target_spec.yaml` の `memory` / `tick_timer` / `architecture` セクションを読み、  
`mps2_an505_gcc/target_config.h` を参照しながら以下のマクロを定義する。

### 定義すべき主要マクロ

```c
/* メモリ配置 */
#define TOPPERS_ISTKSIZE    (4096)          /* 割り込みスタックサイズ（最低4KB） */

/* タイマ設定 */
#define TIMER_INTPRI        TMIN_INTPRI     /* tick割り込みプライオリティ（最低） */
#define TOPPERS_SYSTIM_PCLK <source_hz>    /* タイマソースクロック（Hz） */

/* シリアル設定 */
#define LOGTASK_PORTID      1               /* syslog用シリアルポートID */
#define UART_BAUD           115200

/* ターゲット識別 */
#define TARGET_NAME         "<display_name>"
```

### ✅ 確認
```bash
grep -c "TOPPERS_ISTKSIZE\|TIMER_INTPRI\|TOPPERS_SYSTIM_PCLK" target/<name>/target_config.h
# → 3以上であること
```

---

## Step 2：target_kernel.h / target_kernel_impl.h — カーネル設定

### 目的
カーネルのスタックサイズ・アーキテクチャ依存の設定を行う。

### AIへの指示
`target_spec.yaml` の `architecture` セクションを読み、  
`architecture.isa` に応じて適切な arch/ ヘッダをインクルードする。

### ISAとarch/の対応

| isa | arch/ インクルードパス |
|---|---|
| armv8-m | `arch/arm_m_gcc/common/core_kernel.h` |
| armv8-a | `arch/arm64_gcc/common/core_kernel.h` |
| armv7-m | `arch/arm_m_gcc/common/core_kernel.h` |
| riscv32 | `arch/riscv_gcc/common/core_kernel.h` |

### target_kernel.h の骨格

```c
#ifndef TOPPERS_TARGET_KERNEL_H
#define TOPPERS_TARGET_KERNEL_H

/* アーキテクチャ依存部のインクルード */
#include "arch/<arch_dir>/common/core_kernel.h"

/* デフォルトスタックサイズ */
#define DEFAULT_SSTKSZ      4096    /* システムスタック */
#define DEFAULT_ISTKSZ      4096    /* 割り込みスタック */

#endif /* TOPPERS_TARGET_KERNEL_H */
```

### ✅ 確認
```bash
python3 cfg/cfg.py --check-target target/<name> include/
# エラーなく完了すること
```

---

## Step 3：HRTタイマ実装（target_timer.h / target_timer.c）

### 目的
カーネルのtickレス高分解能タイマ（HRT）を実装する。  
ASP3のHRTは「tickを定期生成する」のではなく「指定時刻にイベントを上げる」方式。

### HRTインタフェース（実装必須）

```c
/* タイマ初期化（kernel起動時に呼ばれる） */
void target_hrt_initialize(intptr_t exinf);

/* タイマ終了処理 */
void target_hrt_terminate(intptr_t exinf);

/* 現在のHRTカウント値を返す（μs単位） */
HRTCNT target_hrt_get_current(void);

/* 次のHRTイベントを hrtcnt μs後に設定する */
void target_hrt_set_event(HRTCNT hrtcnt);

/* HRTイベントを即時発生させる（タイムアウト処理用） */
void target_hrt_raise_event(void);
```

### タイマ種別ごとの実装パターン

#### SysTick（Cortex-M 推奨）

```c
/* target_timer.c — SysTick使用 */
#include "core_cm33.h"   /* or CMSIS */

void target_hrt_initialize(intptr_t exinf) {
    /* SysTickをμs単位カウンタとして設定 */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0xFFFFFF;            /* 最大値（24bit）*/
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_ENABLE_Msk;
    /* 最初のイベントを設定 */
    target_hrt_set_event(HRTCNT_BOUND);
}

HRTCNT target_hrt_get_current(void) {
    /* SysTickはダウンカウンタ → μs変換 */
    return (HRTCNT)((SysTick->LOAD - SysTick->VAL)
                    / (TOPPERS_SYSTIM_PCLK / 1000000U));
}
```

#### SDK提供タイマ（Pico SDK等）

```c
/* 外部リポジトリ asp3_pico_sdk の target_timer.c を参照（SDK統合版） */
#include "hardware/timer.h"

static repeating_timer_t hrt_timer;

static bool hrt_callback(repeating_timer_t *rt) {
    hrt_handler();   /* ASP3カーネルHRTハンドラ */
    return true;
}

void target_hrt_initialize(intptr_t exinf) {
    add_repeating_timer_us(-1000, hrt_callback, NULL, &hrt_timer);
}
```

### ✅ 確認（ビルドのみ）
```bash
cmake --preset <target_preset> -B build/<name>
cmake --build build/<name> 2>&1 | grep -i "error" | head -20
# コンパイルエラーなし（リンクエラーは次Stepで解消）
```

---

## Step 4：シリアル出力（target_serial.h / target_serial.c）

### 目的
`syslog()` の出力先となる文字出力関数を実装する。  
最低限 `target_fput_log(char c)` 1関数を実装すれば syslog が動く。

### 実装必須関数

```c
/* 1文字をログポートに出力（ポーリング・ブロッキングで可） */
void target_fput_log(char c);
```

### シリアル種別ごとの実装パターン

#### UART（ポーリング）

```c
/* メモリマップドUARTの最小実装 */
void target_fput_log(char c) {
    volatile uint32_t *UART_DR   = (uint32_t *)(UART_BASE + 0x00);
    volatile uint32_t *UART_FR   = (uint32_t *)(UART_BASE + 0x18);
    while (*UART_FR & (1 << 5));  /* TX FIFO full wait */
    *UART_DR = (uint32_t)c;
}
```

#### セミホスティング（QEMU / デバッガ接続時）

```c
/* mps2_an505_gcc/target_serial.c を参照 */
void target_fput_log(char c) {
    /* ARM semihosting SYS_WRITEC (0x03) */
    register uint32_t r0 asm("r0") = 0x03;
    register char    *r1 asm("r1") = &c;
    __asm volatile("bkpt #0xAB" :: "r"(r0), "r"(r1));
}
```

#### Pico SDK UART

```c
/* 外部リポジトリ asp3_pico_sdk の target_serial.c を参照（SDK統合版） */
#include "hardware/uart.h"
void target_fput_log(char c) {
    uart_putc_raw(uart0, c);
}
```

### ✅ 確認
```bash
# POSIXターゲットとの比較：起動後にバナーが出ることを確認
cmake --build build/<name>
# 実機またはQEMUで実行し "TOPPERS/ASP3..." の文字列が出ることを確認
```

---

## Step 5：target_kernel.c — カーネル初期化

### 目的
カーネル起動前後のターゲット固有初期化を実装する。

### 実装すべき関数

```c
/* カーネル起動前の初期化（時計・ペリフェラル初期化） */
void target_initialize(void);

/* カーネル終了処理（デバッグ停止・LEDオフ等） */
void target_exit(void);
```

### 骨格

```c
void target_initialize(void) {
    /* 1. システムクロック初期化（SDK使用時はSDKが実施済みの場合あり） */
    /* 2. 割り込みコントローラ初期化 */
    /*    Cortex-M: NVIC設定（プライオリティグループ等） */
    /*    Cortex-A: GIC初期化 → stm32mp257f_dk_arm64_gcc/target_kernel.c 参照 */
    /* 3. シリアル初期化（UARTクロック設定・ピン設定） */
    /* 4. HRTタイマ初期化はkernel側で呼ぶため、ここでは不要 */

    /* ARM Cortex-M: 割り込みプライオリティグループを設定 */
    NVIC_SetPriorityGrouping(0U);   /* 全ビットをサブプライオリティなしに */
}

void target_exit(void) {
    /* デバッグ用：QEMU終了またはbreakpoint */
    qemu_exit(0);   /* syssvc/qemu_exit.c */
}
```

### Cortex-A35固有の注意事項

```c
/* stm32mp257f_dk_arm64_gcc/target_kernel.c を参照 */
/* GICv3初期化が必要：
   - GICD（Distributor）初期化
   - GICR（Redistributor）初期化
   - ICC（CPU Interface）有効化
   - EL設定に応じたグループ設定
*/
```

### ✅ 確認
```bash
cmake --build build/<name> && \
echo "Build OK" || echo "Build FAILED"
```

---

## Step 6：target.cmake — ターゲット依存部の登録

### 目的
ターゲット固有のcfgファイル・生成スクリプト・ソース・include・マクロを、
ASP3のCMake変数コントラクトに**積み上げる（list APPEND）**。最後に arch.cmake を include する。

> このプロジェクトのCMakeは asp3_pico_sdk をベースとする。target.cmake は
> 「変数に値を積んで arch.cmake を呼ぶ」だけで、ビルドロジック本体（libasp3.a・cfg 3パス）は
> ルート `CMakeLists.txt` 側にある。コンパイラフラグ・リンカスクリプトはツールチェーンファイル
> （`cmake/toolchain-*.cmake`）とSDK側が持つので、target.cmake には書かない。

### 変数コントラクト（target.cmake で設定するもの）

| 変数 | 内容 |
|---|---|
| `ASP3_CFG_FILES` | `target_kernel.cfg`（ターゲットの静的構成） |
| `ASP3_KERNEL_CFG_TRB_FILES` | `target_kernel.py`（pass2のkernel_cfg生成テンプレート、旧.trb） |
| `ASP3_CHECK_TRB_FILES` | `target_check.py`（pass3のチェックテンプレート、旧.trb） |
| `ASP3_TARGET_C_FILES` | `target_kernel_impl.c` / `target_timer.c` / `target_serial.c` |
| `ASP3_INCLUDE_DIRS` | ターゲット・SDKのインクルードパス |
| `ASP3_COMPILE_DEFS` | ターゲット固有マクロ（FPU設定・チップ識別等） |

### 骨格（asp3_pico_sdk の target.cmake に倣う）

```cmake
# target/<name>/target.cmake

set(ARCHDIR   ${ASP3_ROOT_DIR}/arch/<arch_dir>)
set(CHIPDIR   ${PROJECT_SOURCE_DIR}/arch/<arch_dir>/<chip>)
set(TARGETDIR ${PROJECT_SOURCE_DIR}/target/<name>)

# 静的構成ファイル
list(APPEND ASP3_CFG_FILES
    ${TARGETDIR}/target_kernel.cfg
)

# 生成テンプレート（旧 .trb → Python）
list(APPEND ASP3_KERNEL_CFG_TRB_FILES
    ${TARGETDIR}/target_kernel.py
)
list(APPEND ASP3_CHECK_TRB_FILES
    ${TARGETDIR}/target_check.py
)

# インクルードパス（SDKヘッダ等）
list(APPEND ASP3_INCLUDE_DIRS
    ${TARGETDIR}
    # ${SDK}/.../include  など
)

# ターゲット固有マクロ
list(APPEND ASP3_COMPILE_DEFS
    # 例：USE_TIM_AS_HRT TOPPERS_FPU_ENABLE など
)

# ターゲット依存Cソース
list(APPEND ASP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
    ${TARGETDIR}/target_timer.c
    ${TARGETDIR}/target_serial.c
)

# アーキテクチャ依存部を読み込む（最後に必ず）
include(${CHIPDIR}/arch.cmake)
```

### arch.cmake 側（アーキ依存部・参考）

arch.cmake では `ASP3_SYMVAL_TABLES`（`core_sym.def`）、`ASP3_OFFSET_TRB_FILES`
（`core_offset.py`）、`ASP3_ARCH_C_FILES`（`core_kernel_impl.c` / `core_support.S`）、
アーキ向け `ASP3_COMPILE_DEFS` を積む。既存の `arch/arm_m_gcc/rp2350/arch.cmake` を参照。

### コンパイラフラグ・リンカスクリプトの所在

target.cmake には書かない：
- **コンパイラフラグ**（`-mcpu` 等）→ `cmake/toolchain-<arch>.cmake`（CMakePresets の toolchainFile）
- **リンカスクリプト** → SDK提供（Pico SDK等）、またはターゲットの `.ld` をツールチェーン/リンク設定で指定
- **ライブラリ構成・cfg 3パス** → ルート `CMakeLists.txt`（変更不要）

### ✅ 確認
```bash
cmake --preset <new_preset> -B build/<name>
# Configureがエラーなく完了し、libasp3.a のビルド設定が生成されること
```

### 外部（SDK）ターゲットの置き方（`ASP3_TARGET_DIR`）

ターゲット依存部（`target/<name>/`）を **asp3_core リポジトリの外**（各社SDKリポジトリ側）に
置くこともできる。asp3_core を各SDKリポジトリの submodule として参照する構成
（Pico SDK統合・`docs/dev/pico-sdk-integration.md`）で、asp3_core を純カーネルに保つための仕組み。

```bash
# 外部の target.cmake を使ってビルド（ASP3_ROOT_DIR は submodule の asp3_core）
cmake -S asp3_core -B build \
  -DASP3_TARGET=<name> \
  -DASP3_TARGET_DIR=/abs/path/to/sdk_repo/target/<name>
```

`ASP3_TARGET_DIR` は `asp3_core.cmake` が解決する変数（**後方互換**）：

- **未指定時**＝`${ASP3_ROOT_DIR}/target/${ASP3_TARGET}`（＝従来動作。既存プリセットは無変更）。
- **指定時**＝そのディレクトリの `target.cmake` を include する。絶対パスで与える。

#### 外部 target.cmake の参照規約（パス解決のキモ）

外部に置く `target.cmake` では、参照先によってベースパスを使い分ける：

| 参照対象 | ベース | 理由 |
|---|---|---|
| **チップ／ターゲット依存部**（自分の `target_*.c` / `.cfg` / `arch.cmake` / `chip.cmake`） | `${CMAKE_CURRENT_LIST_DIR}` 相対 | 外部リポジトリ側に実体があるため。`ASP3_ROOT_DIR`（submodule）と取り違えるとパスが壊れる |
| **共通 arch**（`arch/<core>/common/arch.cmake`）・**cfgテンプレート**（`core_*.py`）・カーネル本体 | `${ASP3_ROOT_DIR}` | submodule（asp3_core）側に残るため |
| **cfgテンプレート・Cソースの供給**（`ASP3_*_FILES`） | **絶対パス**で積む（既存規約） | リスト変数はルート `CMakeLists.txt` が絶対パス前提で扱う |

```cmake
# 外部（SDK）リポジトリ側 target/<name>/target.cmake の骨格
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})            # ← 外部の自分自身（ASP3_ROOT_DIR ではない）
set(CHIPDIR   ${CMAKE_CURRENT_LIST_DIR}/../../arch/<arch_dir>/<chip>)  # 外部に持つ場合

list(APPEND ASP3_TARGET_C_FILES ${TARGETDIR}/target_kernel_impl.c)    # 絶対パスで積む
list(APPEND ASP3_INCLUDE_DIRS   ${TARGETDIR})

# 共通 arch・cfgエンジンは submodule（asp3_core）側＝ASP3_ROOT_DIR 参照のまま
include(${ASP3_ROOT_DIR}/arch/<arch_dir>/common/arch.cmake)
```

> リポジトリ内ターゲット（`target/linux_gcc` 等）は従来どおり `${ASP3_ROOT_DIR}/target/<name>` を
> 使えばよい（`ASP3_ROOT_DIR` == リポジトリルートで一致するため）。`CMAKE_CURRENT_LIST_DIR` 相対は
> **外部に置くターゲットを書くときの規約**。

#### 別ツールチェーンの併用（gcc 以外の SDK）

外部 SDK が asp3_core 既存ターゲット（gcc：arm-none-eabi/aarch64/riscv64）と
**異なるツールチェーン**を使う場合（例：FSP統合は LLVM-ARM/clang＝ATfE）、
ツールチェーンファイルは **asp3_core ではなく外部リポジトリ側に置く**（asp3_core を汚さない）。

- 外部アプリの最上位 `CMakeLists.txt`（`project()` より前）または `-DCMAKE_TOOLCHAIN_FILE=` で
  SDK 側のツールチェーンファイル（例 `cmake/llvm.cmake`）を指定する。
- `add_subdirectory(asp3_core ...)`（`ASP3_LIBRARY_ONLY=ON`）で取り込む `asp3` ライブラリ・
  `cfg1_out` は、その最上位で確定したツールチェーン（コンパイラ・`CMAKE_C_FLAGS` 等）を継承する。
  ＝asp3_core 側は**ツールチェーン非依存**で、gcc 専用の前提を持たない（`cmake/toolchain-*.cmake`
  は asp3_core 内ターゲット用で、外部 SDK はこれを使わなくてよい）。
- `cfg1_out`（静的API値抽出用の使い捨て ELF）が SDK 生成ヘッダ等に依存する場合は、
  外部側で `add_dependencies(cfg1_out <生成ターゲット>)` を **`add_subdirectory(asp3_core)` の後**に
  記述する（`cfg1_out` ターゲットはそこで初めて生成されるため）。
- 詳細な実例は `docs/dev/pico-sdk-integration.md`（Pico SDK・clang不要）／
  `docs/dev/fsp-integration.md`（Renesas FSP・LLVM-ARM/clang＋RASC 生成依存）を参照。

---

## Step 7：ビルド全体確認

```bash
# フルビルド
cmake --preset <new_preset> -B build/<name>
cmake --build build/<name> 2>&1 | tee build_log.txt

# エラー確認
grep -c "^.*error:" build_log.txt    # → 0 であること
grep -c "^.*warning:" build_log.txt  # → 確認・対処
```

---

## Step 8：動作確認（段階的テスト）

### 8-1. syslog出力確認（最初の動作確認）

```c
/* sample/sample1.c の先頭タスクで確認 */
void task(intptr_t exinf) {
    syslog(LOG_NOTICE, "Hello from %s!", TARGET_NAME);
    /* ここまで出ればsyslog/serial/timerが動いている */
}
```

期待出力：
```
TOPPERS/ASP3 Kernel Release X.Y.Z ...
Hello from <TARGET_NAME>!
```

### 8-2. TAP移植検証テストの実行

`test/porting/` の8項目テストを，既存の `ASP3_APPLDIR`/`ASP3_APPLNAME`
機構でビルドして実行する（詳細は `test/porting/README.md`）：

```bash
cmake --preset <プリセット名> -B build/test_porting-<name> \
  -DASP3_APPLDIR=test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=test/porting/tap.c
cmake --build build/test_porting-<name>
# QEMUターゲット：AGENTS.md §4のQEMUコマンドで実行
# 実機ターゲット：--target run（書込み）＋ --target console（シリアル）
```

期待TAP出力：
```
1..8
ok 1 - syslog_output
ok 2 - tick_timer_basic
ok 3 - task_create_activate
ok 4 - semaphore_signal_wait
ok 5 - eventflag_set_wait
ok 6 - alarm_handler
ok 7 - isr_delayed_dispatch
ok 8 - wake_from_idle
# 8/8 passed
```

**合格＝`# 8/8 passed` 行があること**（全ターゲット共通の機械判定）。
項目の並びは故障切り分けの順序（①ブート/UART → ②タイマ歩進 →
③ディスパッチャ → ④⑤カーネル本体 → ⑥タイマ割込み経路 → ⑦⑧割込み
出口でのディスパッチ経路．CLIC等mret非経由出口でHW優先度状態が固着し
得るチップの検出用）になっている．落ちた項目から疑う場所は
`test/porting/README.md` の表を参照．

### 8-3. POSIX simとのクロスチェック

```bash
# POSIXで同じテストを実行して合否を確認（ctest登録済み）
cmake --preset linux -B build/test_porting-linux \
  -DASP3_APPLDIR=test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=test/porting/tap.c
cmake --build build/test_porting-linux
ctest --test-dir build/test_porting-linux
```

---

## Step 9：移植完了後の作業

### 9-1. DIVERGENCE_MAP.md の更新

```markdown
| `target/<name>/` | 新規ターゲット追加 | <vendor> <chip> SDK統合 |
上流変更時のリスク: 上流ターゲット追加時に命名衝突確認 |
```

### 9-2. プリセットの追加（target/<name>/presets.json）

プリセットはターゲット依存部のフォルダに置き、ルートの `CMakePresets.json` は
include の一覧のみを持つ（共通設定は `cmake/presets-base.json` の `_base`）。

**(1) `target/<name>/presets.json` を作成**：

```json
{
  "version": 4,
  "include": ["../../cmake/presets-base.json"],
  "configurePresets": [
    {
      "name": "<preset名>",
      "inherits": "_base",
      "displayName": "<display_name>",
      "toolchainFile": "${sourceDir}/cmake/toolchain-<arch>.cmake",
      "cacheVariables": { "ASP3_TARGET": "<name>" }
    }
  ],
  "buildPresets": [
    { "name": "<preset名>", "configurePreset": "<preset名>" },
    { "name": "run-<preset名>", "configurePreset": "<preset名>", "targets": ["run"] }
  ]
}
```

**(2) ルート `CMakePresets.json` の `include` に1行追加**：

```json
"target/<name>/presets.json"
```

### 9-3. CI（GitHub Actions）への追加

`.github/workflows/ci.yml` に新ターゲットのビルドジョブを追加：

```yaml
- name: Build (<name>)
  run: |
    cmake --preset <name> -B build/<name>
    cmake --build build/<name>
```

### 9-4. IMPL_INDEX.md の更新

新ターゲットで実装した内容を参照インデックスに追記する：

```markdown
| <新たに実装した機能> | `<name>` | `target_timer.c` |
```

---

## よくある問題と対処

| 症状 | 原因候補 | 確認箇所 |
|---|---|---|
| ビルドは通るが起動しない | ベクタテーブルアドレス誤り | `target_config.h` の `vector_table` |
| syslog出力が出ない | UARTクロック設定誤り | `target_serial.c` のボーレート計算 |
| タスクが切り替わらない | SysTickが発火していない | `target_timer.c` の初期化 |
| ハードフォルトで落ちる | スタックサイズ不足 / MPU設定 | `DEFAULT_ISTKSZ` / MPU設定 |
| 割り込みが届かない | プライオリティ設定誤り | `TIMER_INTPRI` / NVIC設定 |
| Cortex-A35で止まる | EL遷移 / GIC初期化不足 | `stm32mp257f_dk_arm64_gcc` の実装参照 |

---

## 移植に必要なデータシート情報チェックリスト

移植を始める前に以下がデータシートから確認できていること：

- [ ] フラッシュ / SRAM のアドレスとサイズ
- [ ] ベクタテーブルのアドレス（固定か可変か）
- [ ] システムクロック周波数（またはSDKが設定するか）
- [ ] tick タイマとして使用するペリフェラル名とベースアドレス
- [ ] タイマの割り込み番号とプライオリティビット数
- [ ] UART ペリフェラルのベースアドレス・ボーレート設定レジスタ
- [ ] UART 送信FIFOフルフラグのビット位置
- [ ] 割り込みコントローラの種類（NVIC / GICv2 / GICv3 / PLIC）
