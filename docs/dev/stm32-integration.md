# STM32 HAL統合（STM32Cube / Cortex-M）

> **注**：外側リポジトリは 2026-06-12 に `stm32_vscode_asp` →
> `asp3_stm32cubemx` → **`asp3_stm32cube`** へ改名し、その後 TOPPERSプロジェクトへ
> 移管した（https://github.com/toppers/asp3_stm32cube ・命名規約 asp3_＋SDK名に統一。
> CubeMXはFSPにおけるRASC相当の生成ツールのためリポ名にしない）。
> 同時に glue を `asp3_stm32cube.cmake`、H563ターゲットを
> `target/stm32cubemx` → **`target/stm32h563_nucleo`**（H533の
> `stm32h533_nucleo` と対称）へ改名した。本文中の旧名は経緯ログとしてそのまま維持している。

## 項目

STM32 HAL統合（AGENTS.md §1 機能追加計画、優先度：中。第一目的「各社SDKとの協調動作」の第3弾＝Pico SDK・FSPに続く）

## 内容

STM32Cube HAL（STM32CubeMX生成）と TOPPERS/ASP3 を協調動作させる。
移植元は `../stm32_vscode_asp`（NUCLEO-H563ZI＝STM32H563・Cortex-M33）。
Pico SDK／FSP統合と同じく **SDK固有部を外側リポジトリで管理＋asp3_core を
submodule＋`ASP3_TARGET_DIR` で受け入れる**方式を目指す。

> 注：asp3_core には既に `target/stm32mp257f_dk_arm64_gcc`（STM32MP2・Cortex-A35・
> ベアメタル）があるが、本項目は **STM32CubeMX HAL を使う Cortex-M（STM32H5系）**で別物。

## 現状（2026-06-11 調査・`../stm32_vscode_asp` をclone）

```
stm32_vscode_asp/
├── asp3/                          ← カーネル同梱（★旧世代：TECS＋Ruby cfg）
│   ├── arch/arm_m_gcc/{stm32h5xx_stm32cube, common}
│   ├── target/stm32cubemx         ← STM32Cubeターゲット依存部
│   ├── tecsgen/                   ← ★TECS同梱
│   ├── cfg/*.rb                   ← ★Ruby版cfg
│   ├── syssvc/*.cdl               ← ★TECSセル
│   └── asp3_stm32cube.cmake     ← STM32 glue
├── nucleo_h563zi/                 ← アプリ（STM32CubeMXプロジェクト）
│   ├── H563ZI.ioc                 ← CubeMX設定
│   ├── Core/ startup_stm32h563xx.s
│   └── cmake/{gcc-arm-none-eabi.cmake, stm32cubemx/}
```

### 他2つ（Pico/FSP）との決定的な違い
- **移植元が旧世代asp3**：**TECS（tecsgen同梱）＋ Ruby cfg（cfg.rb）**。
  Pico/FSPは既にTECSレス＋Python cfgだったが、**STM32はそうではない**。
  → 同梱asp3はそのまま使えず、**asp3_core（TECSレス＋Python cfg）へ移植**が要る。

### 移植が楽な点
- **ツールチェーンが arm-none-eabi (gcc)**（`cmake/gcc-arm-none-eabi.cmake`）＝
  既存ターゲット（mps2/pico/zybo）と同じ。**FSPのclangと違い新ツールチェーン不要**。
- **arm_m**（STM32H563＝Cortex-M33）＝既存arch系列。
- **TECSレス変換の前例がある**：`target/stm32mp257f_dk_arm64_gcc`／
  `arch/arm64_gcc/stm32mp2` は STM32 を TECS版→非TECS版へ変換済み
  （`PORTING_ASP3_STM32MP2.md`）。同じ要領で stm32h5xx_stm32cube を変換できる。
- asp3_core側の受け入れ口（`ASP3_TARGET_DIR`・`ASP3_LIBRARY_ONLY`）は整備済み。

### STM32固有の論点
- **STM32CubeMX生成依存**：`H563ZI.ioc` から CubeMX が `Core/`・HALドライバを生成
  （FSPのRASCに相当）。`add_subdirectory(cmake/stm32cubemx)` で取り込む。
- **割込みベクタ／startup**：`startup_stm32h563xx.s`（CubeMX生成）と ASP3 の
  ベクタ・例外の調停（NVIC掌握）。pico の `--wrap` 相当の検討が要るか確認。
- **実機**：NUCLEO-H563ZI（ST-LINK内蔵）。OpenOCD/STM32CubeProgrammerで書込み。

## 必要な環境（ユーザー確認事項）

| 要素 | 用途 | 状況 |
|---|---|---|
| **STM32CubeMX**（＋HAL/CMSIS） | `.ioc`→HAL/Core生成（FSPのRASC相当） | **要確認・要導入**（VSCode STM32拡張＝stm32-for-vscode 経由でも可）。**バージョンは導入時点の最新（現行）に合わせる**（移植元の旧バージョンに固定せず、CubeMX/HAL/CMSISは現行版で再生成） |
| arm-none-eabi-gcc | ビルド（gcc系） | **導入済**（既存ターゲットで使用中） |
| OpenOCD / STM32CubeProgrammer | NUCLEO-H563ZI 書込み・デバッグ | 実機検証時に要確認 |
| NUCLEO-H563ZI 実機 | 動作確認 | 実機接続PCで |

> STM32CubeMX（または同等の生成）が無いと `Core/`・HALが生成できずビルド不可。
> Pico（git clone完結）と違い、FSP同様**ツール生成依存**。CIはビルド対象外見込み。

## 実施プラン

### Step 0：移植元の素性確認（最初に）
- 同梱asp3（TECS/Ruby）と現 asp3_core の乖離を把握。**TECS版syssvc/SIOを
  非TECS版へ変換**する範囲を確定（stm32mp2変換を規範に）。
- `arch/arm_m_gcc/stm32h5xx_stm32cube`・`target/stm32cubemx` のうち、
  TECS依存（.cdl・t*.c）と純依存部（レジスタ定義・SIO実体）を仕分け。

### Step 1：asp3_core への arch/chip 追加（TECSレス・Python cfg化）
- `arch/arm_m_gcc/stm32h5xx_stm32cube`（または外側リポジトリ）に、
  非TECS版 SIO（`chip_serial.[ch]`＋HAL UARTラッパ）・`chip_kernel.py`等を作成
  （pico2_arm／stm32mp2の非TECS構成を規範）。
- cfgテンプレートは `.trb`/Ruby→`.py`（既存変換済みターゲットを規範）。

### Step 2：ターゲット依存部（外側リポジトリ stm32_vscode_asp 側）
- A案（Pico/FSPと同型）：asp3_core を submodule 参照、SDK固有 arch/target を
  外側で `ASP3_TARGET_DIR` 供給、fork CMakeを asp3_core正準へ。
- CubeMX生成フロー（`.ioc`→Core/HAL）＋ gcc toolchain を維持。

### Step 3：検証（実機 NUCLEO-H563ZI）
- CubeMX生成 → gccビルド → sample1（バナー・タスク切替）→ testexec。
- TECSレス変換の正しさを test_porting（6項目）で機械判定。

### Step 4：記録
- DIVERGENCE_MAP（stm32h5xx_stm32cube追加・必要なら）・README索引・本ファイル。

## スコープ外
- TECS構成のサポート（asp3_coreはTECSレス方針＝非TECS版に変換して取り込む）
- STM32全シリーズ（まずH5＝H563の1ボード）
- QEMU対応（NUCLEO-H563ZIのQEMUモデルは想定しない）
- CIでのビルド（CubeMX生成依存＝GitHubランナーに無い。実機環境で検証）

## リスク・確認事項
- **TECS→非TECS変換の工数**：3SDK中最大。ただし stm32mp2 で同種変換の前例・
  知見あり（PORTING_ASP3_STM32MP2.md）。
- **CubeMX生成の再現性**：`.ioc`→生成物のツールバージョン依存。**バージョンは現行（最新）に合わせる方針**（移植元のバージョンには固定しない）。`.ioc` を現行CubeMXで開き直して再生成→差分確認。生成物コミット可否も判断。
- 同梱asp3（旧世代）の他の差分（Ruby cfgテンプレート・arm_m chip実装）の現 asp3_core 比較。
- startup/ベクタの調停（CubeMX startup と ASP3 のNVIC/例外）。

## 実施結果

（2026-06-11 実施。Step 0〜Step 1ビルド検証まで完了。実機検証は次ステップ）

### 方針（確定）

A案（FSP/Pico と同型）：外側リポジトリ `stm32_vscode_asp` を **asp3_core サブモジュール
構成**へ変換し、STM32固有の arch（チップ層）/target を外側に残置、`ASP3_TARGET_DIR`
で asp3_core に供給する。**asp3_core 本体は無変更**（受け入れ口は Pico/FSP で整備済み）。
作業はすべて外側リポジトリ `stm32_vscode_asp`（ブランチ `feat/stm32-h5`）で実施。

### Step 0：移植元の素性確認（仕分け結果）

移植元 `../stm32_vscode_asp/asp3`（旧世代asp3：TECS＋Ruby cfg 同梱）のうち、STM32固有部
（`arch/arm_m_gcc/stm32h5xx_stm32cube`・`target/stm32cubemx`）は **ほぼ非TECS化済み**で、
残るTECS/Ruby依存は `.trb`×2 のみだった：

| 区分 | ファイル | 扱い |
|---|---|---|
| 非TECS済・流用可 | `target_serial.c`（HAL UART・plain `sio_*`）・`target_timer.c`（HAL TIM2=HRT/TIM5=割込み）・`target_kernel_impl.c`（TECS呼出は `#ifndef TOPPERS_OMIT_TECS` ガード内） | そのまま流用 |
| plain cfg | `target_kernel.cfg`・`target_serial.cfg`・`target_timer.cfg` | そのまま |
| 要 .trb→.py | `target_kernel.trb`・`target_check.trb` | Python化（下記） |
| chip層ヘッダ | `chip_kernel.h`/`chip_sil.h`(`TBITW_IPRI=4`)/`chip_kernel_impl.h` 他 | FSP(ra8m2)と**バイト一致**＝流用 |
| 破棄（asp3_coreに存在） | 同梱 `syssvc/*.cdl`・`tecsgen/`・Ruby `cfg/*.rb`・`arch/.../common/*.trb` | submodule化で削除 |

### Step 1〜2：移植実装

- **submodule化**：bundled `asp3/`（606ファイル）の純カーネル部を削除し `asp3/asp3_core`
  （submodule＝`https://github.com/toppers/asp3_core.git`）へ。STM32固有部のみ残置。
- **chip層 `asp3/arch/arm_m_gcc/stm32h5xx_stm32cube/`**：ヘッダ群は流用。`arch.cmake` のみ
  書換（`core_offset.trb`→`.py`、`ARCHDIR/CHIPDIR` 規約、`start.S` 非include＝CubeMX
  startup がリセットを握るため、存在しない `chip_serial.c` 参照を除去）。
- **target層 `asp3/target/stm32cubemx/`**：
  - `target_kernel.trb`→`target_kernel.py`：**Pattern B**＝arm_m 共通 `core_kernel.py` を
    使わず `kernel/kernel.py` を直include し、ベクタテーブルを **`Reset_Handler`/`&_estack`**
    （CubeMX startup 由来）に向けて生成（`_kernel_exc_tbl`・`_kernel_bitpat_cfgint` も生成）。
    FSP `target_kernel.py` と同型。`_estack`/`Reset_Handler` は既存ヘッダ宣言を使用（重複宣言しない）。
  - `target_check.trb`→`target_check.py`（`core_check.py` を include）。
  - `target.cmake`：FSP型（`ARCHDIR`=submodule、`CHIPDIR`=外側）。M33＋fpv5-sp-d16 フラグ。
    **`-Wl,--no-gc-sections` を追加**（下記理由）。
- **glue `asp3/asp3_stm32cube.cmake`**：`asp3_fsp.cmake` 同型（`ASP3_CORE_DIR`/
  `ASP3_ROOT_DIR`/`ASP3_TARGET_DIR`＋`asp3_set_stm32_options`）。
- **アプリ `nucleo_h563zi/CMakeLists.txt`**：fork廃止→`ASP3_LIBRARY_ONLY`＋
  `add_subdirectory(asp3_core)`＋`asp3_add_syssvc`＋`ASP3_APPLDIR/APPLNAME`。

### CubeMX 生成（headless 自動化）

NUCLEO 環境はGUI（xrdp）が不調だったため、`xvfb-run` + `xdotool`（要 `sudo apt install
xvfb xdotool`）で **完全headless自動化**した：CubeMX 6.17 を仮想ディスプレイ起動 →
`config load`/`project generate` → 出現ダイアログを screenshot（`xwd`→PNG）で確認しつつ
クリック：①移行ダイアログ→**Migrate**（現行最新＝CubeMX 6.17＋**FW_H5 V1.6.0**）→
②FW未導入→**Download**→③ライセンス→**I agree**＋**Finish**。FW_H5 V1.6.0（242MB）DL/導入後、
`Core/Drivers/cmake/`＋リンカスクリプトを生成。`.ioc` は移行で V1.6.0 に更新（CubeMXが書換）。
`swmgr install <名前>` は当版では firmware 名解決不可（"Unknown pack"）＝`project generate`
の自動DL経路が正解、という知見。

### ビルドで解決した3点（移植知見）

1. **cfg1_out が空＝`'TOPPERS_magic_number' is not found`**：CubeMXツールチェーン
   （`cmake/gcc-arm-none-eabi.cmake`）が `CMAKE_EXE_LINKER_FLAGS` に `-Wl,--gc-sections` を
   **グローバル付与**。asp3_core側の gc-sections 除去はツールチェーンのグローバル付与には
   効かず、cfg1_out（オフセット抽出用ELF）の未参照シンボルがGCで消失。`target.cmake` の
   `ASP3_LINK_OPTIONS` に **`-Wl,--no-gc-sections`**（後勝ち）を追加して解消。最終exeはCubeMX側でGC有効のまま。
2. **`_estack` 型衝突**：`target_kernel.py` の `extern uint32_t _estack;` が
   `target_kernel_impl.h` の `extern int _estack;` と衝突。重複 extern を削除（既存宣言を使用）。
3. **`EXC_RETURN_PREFIX` 再定義warning**：CMSIS `core_cm33.h`（`0xFF000000UL`）と arm_m.h
   （`0xff000000`）。非致命・FSP（M85）と同様。未対処（warningのまま）。

### ベクタ/startup 調停（確認済み）

`H563ZI.elf` では `0x08000000` に CubeMX `g_pfnVectors`（`.isr_vector`）、ASP3
`_kernel_vector_table` は `.vector`（非flash先頭）に配置。実行時は ASP3 の
`core_initialize()`（`core_kernel_impl.c`）が **`SCB->VTOR=_kernel_vector_table`** を設定する
ため整合（`Reset_Handler`→`main()`→`sta_ker()`→`target_initialize()`→`core_initialize()`）。
リンカスクリプト変更は不要。

### 変更したファイル（外側リポジトリ `stm32_vscode_asp`・ブランチ `feat/stm32-h5`）

| ファイル | 変更 |
|---|---|
| `.gitmodules`／`asp3/asp3_core` | 追加（submodule＝asp3_core `9f54ac8`） |
| `asp3/`（bundled kernel 571ファイル） | 削除（kernel/include/library/cfg/syssvc/tecsgen/tecs_kernel/test/test_cfg/utils/doc/arch/{gcc,tracelog,arm_m_gcc/common} 等） |
| `asp3/arch/arm_m_gcc/stm32h5xx_stm32cube/arch.cmake` | 書換（.py化・ARCHDIR/CHIPDIR・start.S非include） |
| `asp3/arch/arm_m_gcc/stm32h5xx_stm32cube/chip_*.h` | 流用（無変更） |
| `asp3/target/stm32cubemx/target_kernel.py`・`target_check.py` | 新規（.trb→.py） |
| `asp3/target/stm32cubemx/{target_kernel,target_check}.trb` | 削除 |
| `asp3/target/stm32cubemx/target.cmake` | 書換（FSP型・M33/fpv5-sp-d16・`-Wl,--no-gc-sections`） |
| `asp3/asp3_stm32cube.cmake` | 書換（FSP glue 同型） |
| `nucleo_h563zi/CMakeLists.txt` | submodule＋LIBRARY_ONLY＋asp3_add_syssvc へ |
| `nucleo_h563zi/{H563ZI.ioc,Core,Drivers,cmake,*.ld,CMakePresets.json}` | CubeMX 6.17＋FW_H5 V1.6.0 で再生成（`Drivers/cmake/*.ld` は `.gitignore` 済） |

asp3_core 本体：**無変更**（docs のみ：本ファイル・README索引）。

### Git情報

- asp3_core：ベース `9f54ac8`（submodule pin）。本体コード変更なし。
- 外側 `stm32_vscode_asp`：ブランチ `feat/stm32-h5`（main からの差分が全実装）。
  - ファイルリスト再現：`git -C ../stm32_vscode_asp diff --stat main feat/stm32-h5`

### 検証結果

- **ビルド（実機ターゲット）**：NUCLEO-H563ZI / STM32H563（Cortex-M33）。
  `cmake --preset Debug`（arm-none-eabi-gcc 13.2.1 / Ninja）→ **`H563ZI.elf` リンク成功**。
  FLASH 61,344 B (2.93%) / RAM 15,344 B (2.34%)。warning は `EXC_RETURN_PREFIX` 再定義のみ。
- **POSIX/QEMU**：対象外（CubeMX HAL 依存・QEMUモデル無し。スコープ外）。
- **実機（フラッシュ→バナー/タスク切替）**：**動作OK（2026-06-12、deskmini PC）**。
  ベクタ整列修正（2枚目参照）後のビルドで、バナー → `Sample program starts` →
  `r`（rot_rdq）で task1→2→3 切替を確認。**test_porting も 6/6 全通過**（下記）。
  ベクタ調停は静的に確認済み（上記）。CI はビルド対象外（CubeMX生成依存）。

## 2枚目：NUCLEO-H533RE（STM32H533 / Cortex-M33）

（2026-06-12 実施。1枚目 H563ZI を規範に2ボード目を移植。Step 1ビルド検証＋ベクタ
ディスパッチの静的検証まで完了。実機検証は次ステップ。）

### 1枚目（H563ZI）との差分＝ボード固有の要点

| 項目 | H563ZI（NUCLEO-144） | H533RE（NUCLEO-64） | 対応 |
|---|---|---|---|
| VCP UART | **USART3**（PD8/PD9） | **USART2**（PA2/PA3） | `target_serial.{c,h}` の `USART3_IRQn`→`USART2_IRQn`（IRQ番号3箇所） |
| BSPボード判定 | `USE_NUCLEO_H563ZI`（conf.h） | `USE_NUCLEO_H533RE`（conf.h） | BSP `stm32h5xx_nucleo.h` が conf.h 駆動で COM1=USART2 を選択。compile-def の `USE_NUCLEO_64` は無害（conf.h が正） |
| チップ define | `STM32H563xx` | `STM32H533xx` | `target.cmake` |
| タイマ | TIM2(HRT)/TIM5(割込み) | **同左**（H533.ioc も TIM2+TIM5） | 変更なし |
| メモリ | — | RAM 272KB / FLASH 512KB | `.ld`（CubeMX生成） |

その他（arch チップ層 `stm32h5xx_stm32cube`・cfg `.py`・`target_kernel_impl.*`・
`chip_*`）は H563 と共通で**そのまま流用**。Cortex-M33 + fpv5-sp-d16 も同一。

### 変更したファイル（外側 `stm32_vscode_asp`・新規ターゲット `stm32h533_nucleo`）

| ファイル | 変更 |
|---|---|
| `asp3/target/stm32h533_nucleo/` | `target/stm32cubemx`（H563）を複製し H533 向けに最小改変 |
| `…/target.cmake` | `STM32H563xx`→`STM32H533xx`・コメント更新 |
| `…/target_stddef.h` | `TOPPERS_NUCLEO_H563ZI`→`TOPPERS_NUCLEO_H533RE` |
| `…/target_syssvc.h` | `TARGET_NAME "NUCLEO(STM32H533RE)"` |
| `…/target_serial.h` | `INHNO/INTNO_USART` を `USART2_IRQn + 16` へ |
| `…/target_serial.c` | `siopcb_table` の `USART3_IRQn`→`USART2_IRQn` |
| `nucleo_h533re/Core/Src/main.c` | USER CODE 内に `#include "target_kernel.h"`／`sta_ker();` 追加（CubeMX再生成保持） |
| `nucleo_h533re/CMakeLists.txt` | CubeMX素→ asp3_core submodule取込（`ASP3_TARGET stm32h533_nucleo`・`include(../asp3/asp3_stm32cube.cmake)`・`ASP3_LIBRARY_ONLY`・`asp3_add_syssvc`） |
| `nucleo_h533re/{H533.ioc,Core,Drivers,cmake,.vscode,*.ld}` | CubeMX生成（H533RE用 BSP＝USART2／TIM2／TIM5）。**フォルダ構成は H563ZI と同等にフラット化**（旧 `nucleo_h533re/H533/` ネスト＋残骸 `H533RE/`・IAR用 `EWARM/` は削除） |

asp3_core 本体：**無変更**（docs のみ：本ファイル）。

### 検証結果（H533RE）

- **ビルド**：NUCLEO-H533RE / STM32H533（Cortex-M33）。`cmake --preset Debug`
  （arm-none-eabi-gcc 13.2.1 / Ninja）→ **`H533.elf` リンク成功**。
  FLASH 56,928 B (10.86%) / RAM 15,336 B (5.51%)。warning は `EXC_RETURN_PREFIX` 再定義のみ。
- **ベクタディスパッチ静的検証**：ASP3 `_kernel_vector_table` のカーネル管理割込みスロットは
  共通トランポリン `_kernel_core_int_entry` を指し、実ハンドラは `_kernel_exc_tbl[]` から
  ディスパッチされる設計。`_kernel_exc_tbl` の **USART2(idx 75)→`sio_handler`**・
  **TIM5(idx 48→idx 64)→`target_hrt_handler`** を `nm`/`objdump` で確認済み。
  `SCB->VTOR=_kernel_vector_table` は `core_initialize()` が設定（H563 と同経路）。
- **実機（フラッシュ→バナー/タスク切替）**：**動作OK（2026-06-12 解決）**。
  deskmini PC（Linux）で `STM32_Programmer_CLI -c port=SWD reset=HWrst -w
  build/Debug/H533.elf -v -rst` 書込み →
  バナー → `System logging task is started on port 1.` →
  `Sample program starts (exinf = 0).` → `task1 is running (NNN)` 連続出力。
  `r` 送信（rot_rdq）で task1→task2→task3 の切替を確認
  （SysTick・PendSV・svc_handler・USART 割込みの全経路が動作）。

#### 不具合の解析（2026-06-12 解決済み）

当初の症状＝バナー後に `exc_task`(sample1.c:307) の `ras_ter` が `E_CTX` を返し
続けるループ。これは**二次症状**で、真因は以下。

**真因：`_kernel_vector_table` の整列違反（VTOR の整列要件）**

- ARMv8-M の VTOR はベクタテーブルが「エントリ数×4 以上の2のべき乗」境界
  （H533：16+131=148エントリ → **1024byte**）に置かれることを要求し、
  VTOR 書込み時に下位ビットが切り捨てられる。
- CubeMX 生成のリンカスクリプトに `.vector` セクションの配置規則は無く、
  **orphan section として 4byte 整列で配置される**。`core_initialize()` が
  `VTOR = _kernel_vector_table` を設定すると**実効ベースが手前にずれ、
  全ベクタが「ずれた位置」からフェッチ**される。
- 当時のバイナリではテーブルが `0x0800D710`（下位 0x10 切捨て→実効
  `0x0800D700`、**4スロットずれ**）にあり、**SysTick（例外15）のベクタが
  table[11]=`_kernel_svc_handler`** になっていた。アイドル中（`p_runtsk=NULL`、
  割込まれた文脈の r4=0）に最初の SysTick で svc_handler が走り、
  `ldr r0,[r4,#TCB_stk_top]`（core_support.S:305）が `NULL+0x20` を読んで
  精密 BusFault（gdb 実測：CFSR=0x00008200、BFAR=0x20、被フォルト文脈
  IPSR=0x0f=SysTick、フレーム LR=0xFFFFFFBC）。
- **IRQ（番号16以上）はずれ先もほぼ `core_int_entry`** で、内部で IPSR から
  `exc_tbl[]` を引くため**正常動作してしまい**（バナーが出る・USART も動く）、
  発見が遅れた。ずれの影響は例外 11〜15 のシステム例外に集中する。
- 死後 `sta_ker()` が復帰し `main()` 末尾の `while(1)`（=0x080083E0）へ。
  この番地は **ICF で `Error_Handler` の `while(1)` と畳み込まれており**、
  addr2line/bt が「Error_Handler ← MX_ICACHE_Init」と誤誘導した
  （**FPU/MPU/ICACHE は無罪**）。

**修正（2点＋副次1点）**

1. `asp3/target/stm32h533_nucleo/target_kernel.py`：`_kernel_vector_table` に
   `__attribute__((section(".vector"), aligned(N)))` を付与。N はテンプレートで
   「エントリ数×4 を覆う2のべき乗」を動的計算（H533=1024）。リンカスクリプト
   （CubeMX 生成・.gitignore 対象）には手を入れない。
2. `asp3/arch/arm_m_gcc/stm32h5xx_stm32cube/arch.cmake`：
   `TOPPERS_ENABLE_TRUSTZONE` を除去。pico2_arm（RP2350＝Secure ブート）由来の
   流用で、`EXC_RETURN=0xFFFFFFFD`（ES/S=1、Secure 用）になっていた。実機は
   TZEN=0xC3（**TrustZone disabled**、`STM32_Programmer_CLI -ob displ` で確認）で
   HW 生成の EXC_RETURN は `0xFFFFFFBC`。**整列修正後に単独切り分けした結果、
   この実機では 0xFFFFFFFD でも動作した**（HW が RES0 ビットを無視）が、
   仕様上不正のため除去（TZEN 有効化時のみ定義する）。
3. `asp3/target/stm32h533_nucleo/target_serial.c`：`target_fput_log` が
   `putc(stdout)`＋空 `_write_r(){}` で実出力しない porting 仕様非準拠を修正。
   USART2（ST-LINK VCP）レジスタ直接ポーリング（`ISR.TXE` 待ち→`TDR` 書込み、
   `CR1.UE=0` の間は捨てる）に変更。gdb から `call target_fput_log('X')` で
   実出力を確認済み。

**デバッグ手法（winning technique・他移植にも適用可）**

- OpenOCD は stm32h5x.cfg 非搭載でも最小 cfg（stlink/swd/dap/cortex_m の
  target create のみ）で接続可。`openocd -f /tmp/h5.cfg`（gdb server :3333）→
  `gdb-multiarch`。ST-LINK_gdbserver は FW 要求で不可だった。
- 決め手は `monitor cortex_m vector_catch hard_err bus_err mm_err chk_err
  state_err nocp_err int_err`（reset は除外）→ `monitor reset halt` → `continue`
  で**フォルト発生瞬間に停止**させること。
- **「被フォルト文脈の IPSR と PC の所属関数が矛盾」したらベクタテーブル不正を
  疑う**（IPSR=15=SysTick なのに PC が svc_handler 内、が今回の証拠だった）。
- 外部SDKのリンカスクリプトを使う統合では `.vector` が orphan section になる
  → **整列属性をテーブル自体に付けるのが安全**（リンカスクリプト非依存）。
- OpenOCD 起動中は `STM32_Programmer_CLI` が DEV_CONNECT_ERR（ST-LINK 排他）。

#### NUCLEO-H563ZI 実機（2026-06-12 解決・動作OK）

- 旧症状「ログが一切出力されない」は、**整列修正を水平展開した
  `target/stm32cubemx/target_kernel.py` でのビルドで解消**（テーブルは
  `0x0800E000`。エントリ数はカーネル cfg の TMAX_INTNO で決まり H533 と同じ
  148＝必要整列 1024。チップの IRQ 総数ではない点に注意）。
  deskmini PC で CubeMX 6.17 GUI 生成 → ビルド → 書込みで sample1 動作
  （バナー・task1→2→3 切替）。旧症状も同じ整列違反だった可能性が高い
  （旧バイナリでの再現確認はしていない）。
- `target_fput_log` の USART3 直接ポーリング化も同時に展開済み。

#### test_porting（移植検証テスト・H563ZI 実機）

**6/6 全通過**（syslog_output / tick_timer_basic / task_create_activate /
semaphore_signal_wait / eventflag_set_wait / alarm_handler）。

実行方法（外側リポジトリのアプリ差し替え機構を整備した）：
`nucleo_*/CMakeLists.txt` の `ASP3_APPLDIR`/`ASP3_APPLNAME` 既定値を
`if(NOT DEFINED ...)` ガード化し、exe ソースも `${ASP3_APPLDIR}/${ASP3_APPLNAME}.c`
＋`${ASP3_EXTRA_APP_C_FILES}` 参照に変更（sample1 直書きを廃止）。これで
asp3_core 標準の `-D` 差し替えがそのまま使える：

```bash
cd nucleo_h563zi
CORE=$PWD/../asp3/asp3_core
cmake --preset Debug -B build/TestPorting \
  -DASP3_APPLDIR=$CORE/test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=$CORE/test/porting/tap.c
cmake --build build/TestPorting
# 書込み → /dev/ttyACM0 115200 に TAP（1..6, ok 1〜ok 6）が出る
```

#### testexec（機能テスト全件・H563ZI／H533RE 実機・2026-06-12）

標準機能テスト36本を**両ボードの実機で全件実行し、完全に同一の結果**
（あわせて H533RE は test_porting 6/6 も通過）。
**arm_m 規範（mps2 nightly）と同等以上**：

| 判定 | 件数 | 内訳 |
|---|---|---|
| PASS | **32** | 標準30本＋**hrt1・dlynse**（実機でのみ有意味な測定系。QEMU CI では除外されているものが実機で通った） |
| SKIP | 1 | cpuexc10（`This test program is not necessary.`＝正常） |
| FAIL | 2 | **cpuexc1・cpuexc4＝既知の上流 arm_m 特性**（PRIMASK ベース SIL_LOC_INT 中の UsageFault が HardFault 昇格。mps2/pico2_arm と同一挙動。`docs/dev/issue-cpuexc-armm.md` 参照・対処方針はユーザー判断待ち） |
| BUILD_FAIL | 1 | int1（`target_test.h` に INTNO1 等のテスト用ソフト割込み源が未整備。mps2 は予備 NVIC IRQ で対応済み＝STM32 でも同様に整備可能。今後の課題） |

- **実行機構**：外側リポジトリに実機用ランナー `scripts/testexec_stm32.py` を新設
  （asp3_core の test/testexec.py は asp3_core 単体 configure 前提のため）。
  外側プロジェクトを 1 テストずつ `-DASP3_APPLDIR/APPLNAME/APPCFGNAME` 差し替えで
  configure→build→STM32_Programmer_CLI 書込み→シリアル判定。判定は CI ランナー
  （`scripts/ci/run_testexec.py`）と同一仕様（PASS/SKIP マーカー・SPECIAL_SPEC）。
  `--rejudge` で保存済みログの再判定のみ可。
- **dlynse で実較正を実施**：H563ZI 初回 `sil_dly_nse(129): 124 NG`（境界ケース
  のみ 4% 短い）。実測（セットアップ≈68ns・ループ1周≈56ns）に対し
  `SIL_DLY_TIM1=79` が過大だったため **64 に較正**（`stm32cubemx.h`・両ターゲット。
  遅延が長くなる安全側）→ **両ボードで dlynse PASS（NG 0件）**（クロック構成の
  異なる H533RE でも安全側に成立することを実機確認）。

> **状態：H533RE・H563ZI とも実機ブリングアップ・検証完了**
> （sample1・test_porting 6/6・testexec 32 PASS/1 SKIP＝両ボード同一・規範同等）。
> 残：int1 用ソフト割込み源の整備（任意）・cpuexc1/4 の方針確定
> （issue-cpuexc-armm.md・ユーザー判断待ち）。
