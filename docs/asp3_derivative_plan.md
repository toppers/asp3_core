# TOPPERS/ASP3 Core 開発計画まとめ

> **位置づけ（2026-06-07更新）**：本ファイルはプロジェクト発足時の**原典計画
> （歴史文書）**である。機能追加計画の正本は `AGENTS.md` §1 に移行済みで、
> 進捗・実施記録は `docs/dev/README.md` を参照。
> **現役で参照されるのは「CFG_SPEC_MAP」関連の節（§8・§9）のみ**
> （AGENTS §7／§10 から参照）。それ以外の記述（コマンド例・プリセット名等）は
> 当時のままであり、現状とは異なる場合がある。

> 作成日：2026-06-04  
> 対象：TOPPERS/ASP3カーネルの派生型開発  
> 上流：https://www.toppers.jp/asp3-kernel.html

---

## 1. 基本方針

| 項目 | 内容 |
|---|---|
| 上流追従 | オリジナルASP3の開発継続に追従（手動マージ） |
| API互換 | ソース内識別子プレフィックス `asp3` を維持 |
| 位置づけ | 各社SDKとの協調動作を第一目的とした共通カーネル基盤 |
| ライセンス | TOPPERSライセンス準拠・改変版である旨を明示 |

### 名称（決定）

| 項目 | 決定 |
|---|---|
| ブランド名 | **TOPPERS/ASP3 Core** |
| リポジトリ/コードネーム | **`asp3_core`** |
| ソース識別子プレフィックス | `asp3`（変更なし） |

「Core」＝各SDK協調版（pico/fsp/stm32）が乗る**共通カーネル基盤**の意。第一目的（SDK協調・3リポジトリ統合）に合致。

#### 検討経緯（参考）
| 候補 | 伝わる意図 | SDK協調整合 | 後継誤読リスク |
|---|---|---|---|
| **ASP3-Core**（採用） | "他が依存するカーネル核" | ◎ | なし |
| ASP3-Common | "複数SDK間の共通層" | ○ | なし |
| ASP3-Evo | "近代化・進化したASP3" | △ | 低 |
| ASP3-Plus（不採用） | — | — | Ubiquitous AI「TOPPERS-Pro/ASP3」と意味衝突 |

#### TOPPERS名称規則上の留意（要確認）
「TOPPERS/」を冠する正式名は、TOPPERS「ロゴならびに名称の使用に関する規則」第4条の対象：
1. **運営委員会の事前承認**が必要（第4条1項）
2. **公式開発成果物（TOPPERS/ASP3カーネル）と誤認されない名称**であること（第4条2項）

「TOPPERS/ASP3 Core」は公式名に "Kernel" を足さない形で誤認リスクを下げているが、TOPPERS名を冠する以上 **運営委員会マター**。承認前は、リポジトリ/コードネーム `asp3_core` ＋ 説明文「based on TOPPERS/ASP3」で進行可能（同規則第3条）。

> ソース識別子 `asp3` はC識別子であり名称規則の対象外。維持する。

---

## 2. 既存派生物の統合

本カーネルを共通基盤として、以下の派生物を統合する。

| リポジトリ | 対象SDK/環境 |
|---|---|
| [asp3_pico_sdk](https://github.com/toppers/asp3_pico_sdk) | Raspberry Pi Pico SDK（RP2350） |
| [asp3_fsp](https://github.com/toppers/asp3_fsp) | Renesas FSP |
| [asp3_stm32cube](https://github.com/toppers/asp3_stm32cube) | STM32 + CubeMX HAL |

---

## 3. 機能追加計画

**正本は `AGENTS.md` §1「機能追加計画」へ移行した**（項目の追加・変更はAGENTS側で
行う）。各項目の実施プラン・実施結果は `docs/dev/README.md` の索引から辿れる。

---

## 4. ターゲット

| アーキテクチャ | コア | ボード | 状態 |
|---|---|---|---|
| ARMv8-M | Cortex-M33 | PICO2（RP2350） | 移植済み |
| RISC-V | Hazard3（RV32） | PICO2（RP2350） | 計画中 |
| ARMv8-A | Cortex-A35 | STM32MP257F-DK | 移植済み |
| POSIX | - | Linux / macOS | 計画中（CLIターゲット） |
| QEMU | Cortex-M33（CPU0使用） | mps2-an505 | 計画中 |
| QEMU | RISC-V / ARMv8-A | virt | 計画中 |

> **注意**：PICO2（RP2350）はARMv8-M（Cortex-M33）とRISC-V（Hazard3）を切り替え可能な構成。1ボードで2 ISAを賄える。

> **QEMUの選定根拠**：`mps2-an521`（デュアルCortex-M33）を採用。他プロジェクトで実装済み。ZephyrがM33のQEMUおよびユニットテスト用途として最も使い込んでいるモデルで検証マイレージが厚い。ASP3（シングルコア）はCPU0で動作、CPU1は停止。
>
> （2026-06-16 追記：その後 FPU 検証のため mps2-an505/IoTKit（シングルM33・CPU0にFPU）へ置換。an505 と an521 はメモリマップ等が同一で移植コストは僅少だったため、FPU が使える an505 を採用した。）

---

## 5. 追加推奨項目（開発運用面）

### QEMUターゲット
CLIターゲットとは別に用意。ハードなしでISA別にbuild→run→testを回せ、CI・AIエージェントループの基盤になる。

### CI整備（GitHub Actions）
全ターゲットbuild＋POSIX/QEMUでtest実行を自動化。  
**手動マージ後のリグレッション検知**が主目的。  
テスト結果はJUnit/TAP形式でエージェントが解釈できる形にする。

### devcontainer / Docker
ツールチェーン（arm-none-eabi-gcc、Pico SDK、Pythonバージョン）をピン留めし再現性を確保。  
CI実行環境とローカル環境を一致させる。AIエージェントの実行環境としても機能する。

### 上流追従の運用管理

- 上流ファイルに手を入れた箇所の**CHANGELOGを維持**（マージ時の衝突箇所が即座に分かる）
- 上流コアとの差分を**パッチ列として管理**（git format-patch / quilt相当）するとリベースが機械的になる
- `kernel/` 以下のコアは可能な限りバイト一致を保ち、変更はオーバーレイ・新規ファイル側に寄せる

### 取得機構
`asp3_pico`・`asp3_fsp`・`asp3_stm32cube` がカーネルをどう参照するかの統一。  
CMake FetchContent か git submodule かを決め、バージョン紐付けを明確にする。

### 静的解析CI統合
clang-tidy / cppcheck をCIに組み込み、MISRAサブセット相当のルールをエージェントにも強制する。  
AIが生成したコードの品質ゲートとして機能する。

### API/ABI互換の明文化
どこまでTOPPERS API互換を保証するかを明文化。  
TINET・FatFs for TOPPERS等の既存ミドルウェアが流用できる範囲を決める。

---

## 6. AIコーディング時代に必要な機能・ファイル

### 6.1 機能（システムとして備えること）

#### ① 閉ループ検証（最優先）

エージェントが人手なしで **build → run → test** を回せることが前提。  
3点セットで揃えて初めて機能する：

1. **ワンコマンドビルド** — `CMakePresets.json` でターゲット×ツールチェーンを選択可能に
2. **ハードなし実行** — QEMU（mps2-an505）/ POSIX sim ＋ **クリーンな終了**  
   セミホスティング `SYS_EXIT`（または所定アドレス書き込み）でQEMUが終了コードを返すよう配線。これがないとCIで止まる。
3. **機械可読なテスト結果** — TAP / JUnit XML、終了コードで成否判定

#### ② compile_commands.json の出力

`CMAKE_EXPORT_COMPILE_COMMANDS=ON` で生成。  
clangd・各種エージェント・静的解析ツールがコード構造を正確に把握する土台。  
有無で補完・解析の精度が桁違いに変わる。

#### ③ OS awareness（GDB Pythonスクリプト）

タスク一覧・スタック・状態をGDBから取得できるようにする。  
人のデバッグだけでなくエージェントの自律デバッグにも効く。  
シンボル公開の設計を最初から入れておくと後付けより安い。

#### ④ 構造化ログ

syslogを機械パース可能な形式で出力。  
エージェントが実行結果を解釈するための入力になる。

---

### 6.2 ファイル（リポジトリに置くもの）

#### エージェント誘導ファイル

| ファイル | 配置 | 内容 |
|---|---|---|
| `AGENTS.md` | ルート＋`kernel/`・`target/`・`syssvc/` | ビルド/実行/フラッシュ手順、規約、**禁止事項2件**（後述） |
| `CLAUDE.md` | ルート | `AGENTS.md` へのポインタのみ（薄いラッパー） |
| `.clinerules` | ルート | Cline向け。集約元はAGENTS.mdに統一 |
| `.cursorrules` | ルート | Cursor向け。同上 |
| `CMakePresets.json` | ルート | ターゲット×ツールチェーンの組み合わせを宣言 |

#### AGENTS.md に必ず書く禁止事項2件

> これらは**手動マージ戦略・ASP3の安全設計方針と直結**するため必須。

**禁止 ①：`kernel/` コアファイルの直接編集禁止**  
変更はオーバーレイ・新規ファイル側へ。  
上流との手動マージコストを守るための最重要ルール。  
エージェントは黙って上流ファイルを書き換えるため、明示しないと追従が破綻する。

**禁止 ②：カーネル内での動的メモリ確保（malloc等）禁止**  
オブジェクトは静的生成のみ（ASP3の安全設計方針）。  
一般C学習済みのエージェントは放っておくとmallocを入れる。  
ISO 26262 / IEC 61508等の安全規格への適合性を守るために必須。

#### 機械可読メタデータ

| ファイル | 内容 |
|---|---|
| `docs/api/<サービスコール名>.md` | 1サービスコール＝1ファイル形式のAPIリファレンス（RAG・検索精度向上） |
| `docs/errors.md` | `E_*` エラーコード辞書（エージェントが失敗を解釈できる） |
| 宣言的構成スペック | cfg Python化済みの入力スペック（利用可能オブジェクトの知識として活用） |

#### skillパッケージ（別リポジトリ）

| skill | 内容 |
|---|---|
| build | ターゲット別ビルド手順 |
| flash | ターゲット別書き込み手順 |
| debug | GDB/OpenOCD接続・OS awareness活用 |
| cfg生成 | 静的構成ファイル生成の操作手順 |

カーネル本体とバージョン軸を分離し、モデル・ツール更新に独立して追従できる。  
本体には参照ポインタのみ置く。

---

## 7. 優先順位まとめ

```
高 ┌─ 閉ループ検証（QEMU clean exit + テストランナ + CI）
   ├─ compile_commands.json
   ├─ AGENTS.md（禁止事項2件 + マージ支援手順）
   ├─ DIVERGENCE_MAP.md（マージAI容易化の核心）
   ├─ UPSTREAM_VERSION / UPSTREAM_PRISTINE.txt
   │
中 ├─ devcontainer / Docker
   ├─ CFG_SPEC_MAP.md（cfg Python追従管理）
   └─ APIリファレンス Markdown

低 └─ skillパッケージ（merge-review / cfg-diff 含む）
       静的解析CI統合
       動的生成拡張（要件定義から）
```

---

## 8. 手動マージのAI容易化

### 基本考え方

手動マージを「AIに何を渡せば機械的に支援できるか」という視点で設計する。  
必要なのは**差分の意味的な文脈（なぜ変えたか）**と**上流との対応関係の明文化**。  
これがないとAIはテキスト差分しか見えず、セマンティクスの衝突を見逃す。

---

### 8.1 追加するファイル

#### `UPSTREAM_VERSION`（ルート）

現在のベースとなっている上流バージョン・コミットを記録。マージ完了後に更新する。  
AIが「どこまで追従済みか」を判断する基準になる。

```
ASP3_UPSTREAM_VERSION = 3.7.0
ASP3_UPSTREAM_COMMIT  = <commit hash>
ASP3_LAST_MERGED      = 2026-01-15
```

---

#### `DIVERGENCE_MAP.md`（ルート or `docs/`）【最重要】

上流からの**意図的な乖離をすべて一覧化した台帳**。  
AIはマージ時にこのマップを読み、上流の変更がどの行に影響するかを判定する。

| ファイル | 変更種別 | 理由 | 上流変更時のリスク | 担当レイヤ |
|---|---|---|---|---|
| `syssvc/serial.c` | TECS-less化（プレーンC置換） | TECSへの依存除去 | serial API変更時に要確認 | syssvc |
| `cfg/cfg.py` | cfg Python化 | Ruby→Python移行 | 静的API追加時にスペックを追従要 | cfg |
| `CMakeLists.txt` | CMake対応 | ビルドシステム近代化 | Makefile変更は参照のみ | build |
| `target/<name>/` | 新規ターゲット追加 | SDK協調 | 上流ターゲット追加時に命名衝突確認 | target |

> 上流ファイルに手を入れるたびにこのマップに追記する運用にする。  
> AIへのコンテキストとして渡すことで「この上流diffのうちどの変更が我々のローカル改変と衝突しうるか」を自動判定できる。

---

#### `UPSTREAM_PRISTINE.txt`（ルート or `docs/`）

上流から**一切変更していないファイルの一覧**。  
AIへの「このファイルは上流版をそのまま使ってよい」指示リスト。  
DIVERGENCE_MAPに載っていないファイルはすべてここに分類される。

```
kernel/task.c
kernel/task_manage.c
kernel/semaphore.c
kernel/eventflag.c
...
```

これにより、新バージョンの上流diffで変更されたファイルを2グループに瞬時に仕分けできる：
- UPSTREAM_PRISTINE → そのまま上書き更新
- DIVERGENCE_MAP → 人間の確認が必要

---

#### `CFG_SPEC_MAP`（現役の正本は `docs/dev/cfg-spec-map.md`）

> **2026-06-17 更新**：当初この節に書かれていた CFG_SPEC_MAP の台帳（層①静的API定義／
> 層②cfgエンジン／層③生成テンプレートの対応表と「最終確認バージョン」）は、
> 現役の参照ドキュメント **[`docs/dev/cfg-spec-map.md`](dev/cfg-spec-map.md)** へ独立・移設した。
> 以降の追従管理はそちらを正本とする。本節は発足時の構想記録として凍結。

---

### 8.2 AI支援マージワークフロー

```
1. 上流リリースノートを確認（変更箇所の概要把握）
        ↓
2. git diff <UPSTREAM_VERSION>..<new_version> を取得
        ↓
3. AIに【上流diff + DIVERGENCE_MAP + UPSTREAM_PRISTINE】を渡す
   「このdiffでDIVERGENCE_MAPのどの行が影響を受けるか列挙せよ」
        ↓
   ┌─ UPSTREAM_PRISTINE のみ → そのまま上書き更新（人手不要）
   └─ DIVERGENCE_MAP に該当 → 人間が確認・マージ判断
        ↓
4. cfgに関わる変更は CFG_SPEC_MAP と照合
   「この上流cfg変更はCFG_SPEC_MAPのどのAPIに影響するか」
        ↓
5. CI（QEMU/POSIX）で回帰確認 ← 手動マージ後の必須ゲート
        ↓
6. UPSTREAM_VERSION を更新してコミット
```

> **ポイント**：ステップ3でAIが出力する「影響ファイル一覧」が人間の確認スコープを絞る。  
> DIVERGENCE_MAPが整備されているほど、AIが絞り込める精度が上がる。

---

### 8.3 AGENTS.md への追記内容（マージ支援用）

```markdown
## マージ支援手順

上流ASP3の新バージョンとのマージを支援する場合、以下の手順に従うこと：

1. `UPSTREAM_VERSION` で現在のベースバージョンを確認する
2. `DIVERGENCE_MAP.md` を読み、変更種別・影響範囲を把握する
3. 上流diffと DIVERGENCE_MAP を照合し、影響ファイルを列挙する
4. `UPSTREAM_PRISTINE.txt` に記載されているファイルは上流版で上書き更新してよい
5. DIVERGENCE_MAP に該当するファイルは「要人間確認」とマークして停止する
6. cfgに関わる変更は `CFG_SPEC_MAP.md` を参照する
7. すべての変更後、CI（QEMU/POSIXターゲット）で回帰テストを実行する
```

---

### 8.4 skillパッケージへの追加

既存の build / flash / debug / cfg生成 skill に加えて：

| skill名 | 入力 | 出力 |
|---|---|---|
| `merge-review` | 上流diff + DIVERGENCE_MAP | 影響ファイル一覧・確認優先度レポート |
| `cfg-diff` | 上流cfg変更 + CFG_SPEC_MAP | Python実装の更新要否と対象箇所リスト |

---

## 9. 詳細設計：AIコーディング時代の主要機能

### ① 閉ループ検証（詳細）

#### CMakePresets.json 構造

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "m33-qemu",
      "displayName": "Cortex-M33 / QEMU (mps2-an505)",
      "generator": "Ninja",
      "cacheVariables": {
        "ASP3_TARGET": "mps2-an505_qemu",
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/toolchain-arm-none-eabi.cmake",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    {
      "name": "posix",
      "displayName": "POSIX CLI target",
      "generator": "Ninja",
      "cacheVariables": {
        "ASP3_TARGET": "linux_gcc",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    }
  ],
  "buildPresets": [
    { "name": "m33-qemu", "configurePreset": "m33-qemu" },
    { "name": "posix",    "configurePreset": "posix"    }
  ],
  "testPresets": [
    { "name": "posix", "configurePreset": "posix",
      "command": "./asp", "args": ["--tap"] }
  ]
}
```

#### QEMU 起動コマンド（mps2-an505）

```bash
qemu-system-arm \
  -M mps2-an505 \
  -cpu cortex-m33 \
  -kernel build/m33-qemu/asp.elf \
  -semihosting \
  -semihosting-config enable=on,target=native \
  -nographic
```

> mps2-an505 はシングルM33（IoTKit）で CPU0 に FPU を実装。ASP3はCPU0で動作。
> （2026-06-16 追記：当初は mps2-an521〔デュアルM33・CPU1停止〕を採用していたが、FPU 検証のため mps2-an505/IoTKit へ置換した。）

#### セミホスティングによるクリーン終了（Cortex-M33）

QEMUがテスト後に終了コードを返すための実装。**これがないとCIでタイムアウト待ちになる。**

```c
/* syssvc/qemu_exit.c */
#include <stdint.h>

void qemu_exit(int code) {
    /* ARM Cortex-M semihosting: SYS_EXIT (0x18) */
    volatile struct {
        uint32_t reason;   /* ADP_Stopped_ApplicationExit */
        uint32_t subcode;
    } args = { 0x20026U, (uint32_t)code };

    register uint32_t r0 asm("r0") = 0x18U;
    register uint32_t r1 asm("r1") = (uint32_t)&args;
    __asm volatile("bkpt #0xAB" :: "r"(r0), "r"(r1) : "memory");
    while(1); /* unreachable: suppress noreturn warning */
}
```

テスト完了時に `qemu_exit(pass ? 0 : 1)` を呼ぶ。GitHub ActionsはExitコードで成否を判定。

#### テストランナ（TAP形式）

```
TAP version 13
1..4
ok 1 - tsk_create_delete
ok 2 - sem_signal_wait
not ok 3 - flg_timeout
  ---
  expected: E_TMOUT
  actual:   E_OK
  ...
ok 4 - dtq_send_receive
```

C実装骨格：

```c
static int tap_total = 0, tap_run = 0;

void tap_plan(int n)  { tap_total = n; syslog_printf("1..%d\n", n); }
void tap_ok(const char *msg) {
    syslog_printf("ok %d - %s\n", ++tap_run, msg);
}
void tap_ng(const char *msg, const char *exp, const char *got) {
    syslog_printf("not ok %d - %s\n  ---\n  expected: %s\n  actual:   %s\n  ...\n",
                  ++tap_run, msg, exp, got);
}
void tap_done(void)   { qemu_exit(tap_run == tap_total ? 0 : 1); }
```

#### GitHub Actions 骨格

```yaml
name: CI
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install toolchain
        run: sudo apt-get install -y gcc-arm-none-eabi qemu-system-arm ninja-build

      - name: Build & Test (POSIX)
        run: |
          cmake --preset posix -B build/posix
          cmake --build build/posix
          ./build/posix/asp --tap | tee results_posix.tap

      - name: Build (M33 QEMU)
        run: |
          cmake --preset m33-qemu -B build/m33-qemu
          cmake --build build/m33-qemu

      - name: Test (QEMU mps2-an505)
        run: |
          timeout 30 qemu-system-arm \
            -M mps2-an505 -cpu cortex-m33 \
            -kernel build/m33-qemu/asp.elf \
            -semihosting -semihosting-config enable=on,target=native \
            -nographic 2>&1 | tee results_qemu.tap
```

---

### ② compile_commands.json（詳細）

#### 生成と配置

`CMAKE_EXPORT_COMPILE_COMMANDS=ON`（CMakePresets.jsonに既定済み）でビルド時に自動生成。  
ルートへシンボリックリンクを張るとclangd・エージェントが自動検出する。

```bash
cmake --preset m33-qemu -B build/m33-qemu
ln -sf build/m33-qemu/compile_commands.json compile_commands.json
```

#### クロスコンパイル時の clangd 対応

組み込みクロス環境ではそのままでは arm-none-eabi のインクルードパスが解決されない。  
`.clangd`（ルートに配置）で補正する：

```yaml
# .clangd
CompileFlags:
  Compiler: arm-none-eabi-gcc
  Add:
    - "--target=arm-none-eabihf"
    - "-isystem/usr/lib/arm-none-eabi/include"
  Remove:
    - "-mfpu=*"
    - "-mfloat-abi=*"
```

または clangd 起動オプションで解決（VS Code settings.json等）：

```
clangd --query-driver="/usr/bin/arm-none-eabi-gcc*"
```

#### エージェントへの効果

| 効果 | 内容 |
|---|---|
| 型解決 | `T_CTSK`・`ER`・`ID` 等の組み込み型を正確に把握 |
| API補完 | `act_tsk`・`sig_sem` 等のシグネチャを即時解決 |
| インクルード解決 | 架空ヘッダのハルシネーションを抑制 |
| 静的解析連携 | clang-tidy がコンパイルフラグを正確に引き継ぐ |

> compile_commands.json がない状態でのエージェントコーディング＝「インクルードパスを知らずにコードを書く」状態。  
> 未定義シンボルの埋め込みやMISRA違反コードが大量生成される原因になる。

---

### ④ 構造化ログ（詳細）

#### フォーマット設計

行指向・キーバリュー形式。JSONより軽量でUARTバッファを圧迫しない。

```
T=<tick_us>,EV=<event>[,<key>=<val>]*\r\n
```

ログ例：

```
T=1000,EV=TSK_STA,ID=1
T=1005,EV=SEM_SIG,ID=2,CNT=1
T=1010,EV=TSK_WAI,ID=1,SEM=2
T=1015,EV=SEM_WAI,ID=1,SEM=2,CNT=0
T=1020,EV=TSK_RUN,ID=1
T=1500,EV=ALM_HND,ID=1
T=2000,EV=ERR,API=wai_sem,CODE=-18
```

#### ログ対象イベント

| カテゴリ | イベント | 主要パラメータ |
|---|---|---|
| タスク | TSK_STA / TSK_RUN / TSK_WAI / TSK_SLP / TSK_TRM | ID |
| セマフォ | SEM_SIG / SEM_WAI | ID, CNT |
| イベントフラグ | FLG_SET / FLG_WAI | ID, FLGPTN |
| データキュー | DTQ_SND / DTQ_RCV | ID |
| アラームハンドラ | ALM_STA / ALM_STP / ALM_HND | ID |
| ISR | ISR_ENT / ISR_EXT | INTNO |
| エラー | ERR | API, CODE（`E_*` 値） |

#### 実装（syssvc/syslog.c TECSレス版への追加）

```c
/* fch_hrt() でマイクロ秒タイムスタンプを取得 */
static void slog_put_uint(uint32_t v) { /* 10進ASCII出力 */ }
static void slog_puts(const char *s)  { /* UART or semihosting */ }

void slog_event(const char *ev, ID id) {
    slog_puts("T=");    slog_put_uint((uint32_t)fch_hrt());
    slog_puts(",EV=");  slog_puts(ev);
    slog_puts(",ID=");  slog_put_uint((uint32_t)id);
    slog_puts("\r\n");
}

void slog_error(const char *api, ER er) {
    slog_puts("T=");     slog_put_uint((uint32_t)fch_hrt());
    slog_puts(",EV=ERR,API="); slog_puts(api);
    slog_puts(",CODE="); slog_put_uint((uint32_t)er);
    slog_puts("\r\n");
}
```

カーネルのタスクディスパッチャ・各サービスコール入口に `slog_event()` を挿入する。

#### Python ログパーサ（`scripts/parse_slog.py`）

```python
import re, sys

def parse_line(line):
    m = re.match(r'T=(\d+),EV=(\w+)(.*)', line.strip())
    if not m:
        return None
    fields = dict(kv.split('=') for kv in m[3].lstrip(',').split(',') if '=' in kv)
    return {"tick": int(m[1]), "ev": m[2], **fields}

events = [e for line in sys.stdin if (e := parse_line(line))]

# 例：コンテキスト切り替えレイテンシ集計
runs = [(e["tick"], e["ID"]) for e in events if e["ev"] == "TSK_RUN"]
for i in range(1, len(runs)):
    dt = runs[i][0] - runs[i-1][0]
    print(f"context switch: {dt} us  (-> task {runs[i]['ID']})")
```

#### CI での活用

```bash
# テスト実行 → ログ取得 → 期待イベント列と比較
./asp 2>&1 | python3 scripts/parse_slog.py > actual.json
python3 scripts/check_events.py expected/sample1.json actual.json
```

`check_events.py` は期待イベント列（順序・パラメータ）が actual に含まれるかを検証する。  
タイミング異常（コンテキスト切り替えが規定μs超等）の回帰検知にも使える。

---

## 10. 参考リンク

- [TOPPERS/ASP3公式](https://www.toppers.jp/asp3-kernel.html)
- [asp3_pico_sdk](https://github.com/toppers/asp3_pico_sdk)
- [asp3_fsp](https://github.com/toppers/asp3_fsp)
- [asp3_stm32cube](https://github.com/toppers/asp3_stm32cube)
- [QEMU mps2/mps3ドキュメント](https://www.qemu.org/docs/master/system/arm/mps2.html)
