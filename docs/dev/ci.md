# CI整備

## 項目

CI整備（AGENTS.md §1 機能追加計画、優先度：高）

## 内容

GitHub Actions で**全ターゲットのビルド＋POSIX/QEMUでのテスト実行**を自動化する
（リポジトリ：`github.com/toppers/asp3_core`）。

### 意義（何が嬉しいか）

1. **手動マージ後のリグレッション検知**（主目的）：本リポジトリは上流ASP3を
   手動マージで追従する。マージ・機能追加のたびに7ターゲットを手元で全件回すのは
   現実的でなく、「pushすれば全ターゲットの build→run→test が自動で走る」状態が
   上流追従の安全網になる。
2. **エージェントの作業完了条件の客観化**：AGENTS §4「検証の鉄則」（ビルドが
   通ることを確認してから報告）を、ローカル確認＋**CI green** の二段に格上げできる。
   テスト結果はTAP/JUnit形式で出力し、エージェントが解釈・自動修正に使える。
3. **環境の再現性**：GitHub-hostedランナー（ubuntu-24.04）＝開発機と同じ
   Ubuntu 24.04 系。apt のツールチェーン・QEMUバージョンがCI定義に明文化され、
   「開発機でしか通らないビルド」を検出できる。
4. **静的解析の常設**：AGENTS §6 が先行記述している clang-tidy / cppcheck の
   チェックをCIに実装し、MISRA-C意識の品質ゲートを実体化する。

### 前提の整理（2026-06-06 調査）

| 項目 | 状況 |
|---|---|
| `.github/workflows/` | **未作成**（AGENTS §11 が `ci.yml` を先行参照） |
| ランナー | `ubuntu-24.04`＝開発機と同系（CMake 3.28・QEMU 8.2） |
| arm-none-eabi | apt `gcc-arm-none-eabi`（13.2）✓ mps2/zybo/pico2 |
| aarch64 | apt `gcc-aarch64-linux-gnu`（13.3）✓ zcu102（-nostdlib対応済み）．**stm32の最終リンクは aarch64-none-elf が必要**→ARM公式tarballをDL＋`actions/cache` |
| riscv64 | apt `gcc-riscv64-unknown-elf`（13.2）✓ polarfire |
| QEMU | apt `qemu-system-arm`（mps2/zybo/zcu102 ✓ 開発機で8.2検証済み）・`qemu-system-misc`（riscv64）．**⚠️ microchip-icicle-kit は開発機ではQEMU 11で検証**：8.2での動作（-bios none・CLINT 1MHz）は**CI初回実行で要確認**．NGなら build-only に切替（または QEMU をソースビルド＋cache） |
| テスト実行基盤 | POSIX：ctest（JUnit出力可）＋testexec.py（ネイティブ・高速）／QEMU：testexec.py（セミホスティング自動終了）／cfg：testcfg.py（dummy・ホスト）— すべて整備済み |
| 合否判定 | `--tap`（POSIX）・「All check points passed.」（testexec）・check_events.py（slog照合）— CLIターゲット項目で整備済み |
| gh CLI | 開発機に未導入（CI結果の取得に導入推奨：`sudo apt-get install gh`） |

### ジョブ設計

**ワークフロー `ci.yml`**（push: main / pull_request / workflow_dispatch）

| ジョブ | ツールチェーン | 内容 |
|---|---|---|
| `posix` | （ホストgcc） | build → `ctest --preset linux --output-junit` → **testexec.py 全機能テスト**（ネイティブで高速）→ testcfg.py（dummy） |
| `mps2-qemu` | arm-none-eabi＋qemu-arm | mps2-an505（M33/v8-M）build → testexec.py スモーク（task1/sem1/flg1/tmevt1/hrt1） |
| `mps2-an386-qemu` | 同上 | mps2-an386（M4/**v7-M**・非TZ）build → testexec.py スモーク（task1/sem1/flg1/**int1**/tmevt1/hrt1）〔2026-06-16 追加〕 |
| `mps3-an547-qemu` | 同上 | mps3-an547（M55/**v8.1-M+MVE**）build → testexec.py スモーク（task1/sem1/flg1/tmevt1/hrt1/**mvevpr**＝MVE VPR保持）〔2026-06-16 追加〕 |
| `zybo-qemu` | 同上 | build → sample1スモーク（バナーgrep・タスク切替） |
| `zcu102-qemu` | aarch64-linux-gnu＋qemu-aarch64 | build → sample1スモーク → testexec.py スモーク（sem1） |
| `polarfire-qemu` | riscv64-unknown-elf＋qemu-misc | build → sample1スモーク（**icicle-kit/QEMU8.2の検証を兼ねる**） |
| `pico2` | arm-none-eabi | **build only**（実機ターゲット） |
| `stm32mp257` | aarch64-none-elf（DL+cache） | **build only**（実機ターゲット・リンクまで） |
| `static-analysis` | cppcheck（＋clang-tidy） | EXTENDED/NEW領域（syssvc/ target/ arch/ cfg/ scripts/）。**kernel/・include/・library/（PRISTINE）は対象外** |

**ワークフロー `nightly.yml`**（schedule＋workflow_dispatch）

- `testexec-full`：QEMU 3ターゲット（mps2／zcu102／polarfire）で **testexec.py 全件**
  （PR毎に回すには重いため夜間に分離。mps2は36本×約30秒≒20分規模）
- `ttsp3-mprofile`（matrix・fail-fast=false）：**M-profile 3ターゲット**
  （mps2-an505／mps2-an386／mps3-an547）で TTSP3 **functional 全件**（gate＝FAIL 0）
  ＋ **staticAPI**（non-gating＝ターゲット依存の cfg エラーコード差あり）を build+run
  〔2026-06-16 に単独 `ttsp3-mps2`〔an505のみ〕から an386/an547 を加えてマトリクス化〕。
  詳細は `docs/dev/ttsp3-conformance.md`。TTSP3 は public のため認証不要、ruby（ttg）が要る。

### 設計方針

- **スモークとフルの2段構え**：PR/pushは「全ターゲットbuild＋各QEMUスモーク」で
  10分以内を目標。全件テストはnightly（および手動dispatch）
- **結果はartifacts化**：テストログ（testexecのOBJ-\*出力・slogのjsonl・
  ctestのJUnit XML）をアップロードし、失敗時にエージェント/人間が解析できる形に
- **キャッシュ**：aarch64-none-elf tarball（〜200MB）のみ`actions/cache`。
  aptパッケージはランナー既載＋apt installで十分高速
- **fail-fast無効**：ターゲット独立で全件の成否を見せる（回帰の全体像把握）
- **concurrency**：同一ブランチの古い実行をキャンセル

## 実施プラン

1. **`ci.yml` 骨格＋posixジョブ**（最小で green にする）
   - checkout → build → ctest（JUnit）→ testexec.py全件 → testcfg.py →
     artifacts（JUnit・ログ）
2. **QEMUジョブ追加**（mps2 → zybo → zcu102 → polarfire の順）
   - 各スモークの合否判定スクリプトは `scripts/ci/` に置く
     （バナーgrep・「All check points passed.」カウント等．ローカルでも実行可能に）
   - **polarfireはQEMU 8.2での初回検証を兼ねる**：NGの場合は build-only に
     落とし、対処（QEMUソースビルド＋cache等）を本ファイルに記録
3. **build-onlyジョブ**（pico2／stm32mp257）
   - stm32：ARM公式 aarch64-none-elf tarball のDL＋cache＋フルリンク
4. **static-analysisジョブ**
   - cppcheck（apt）：syssvc/ target/ arch/ の C ソース（PRISTINE除外・
     `--error-exitcode=1` は当面warning運用＝non-blocking から開始）
   - clang-tidy は posix の compile_commands.json を使い対象を絞って段階導入
5. **nightly.yml**（QEMU全件テスト）
6. **検証**：ブランチ `feat/ci` でActions実行を確認（AGENTS §9 のブランチ基準
   「テスト/CI大幅変更」に該当）→ green後にmainへ
   - 開発機に gh CLI が無いため、実行結果の確認はWeb UIまたは
     `gh run watch`（gh導入後）。**CI検証はpush駆動**になる点に注意
7. **記録**：AGENTS §6（静的解析の実体化を反映）・§11手順4のci.yml実体化確認・
   README索引・本ファイル実施結果。READMEバッジ追加（任意）

### スコープ外

- 実機テストの自動化（セルフホストランナー）
- devcontainer / Docker によるツールチェーンピン留め（derivative planに記載
  あり。CIがaptバージョンを明文化するため当面不要。必要になったら別項目）
- JUnit集約ダッシュボード・カバレッジ計測

### リスク・確認事項

- **microchip-icicle-kit の QEMU 8.2 動作**（上記）— 計画段階での最大の不確定要素
- GitHub Actions の実行時間（publicリポジトリのため無料だが，
  nightlyの頻度・対象は必要に応じて調整）
- testexec.py はテスト毎に cmake configure を回すため、CI上の実行時間は
  ローカルより伸びる可能性（必要なら configure 共有等の高速化は別途）

## 実施結果

（2026-06-06 実施・完了）

実施プランどおり、①ci.yml骨格＋posixジョブ、②QEMUジョブ（mps2／zybo／
zcu102／polarfire）、③build-onlyジョブ（pico2／stm32mp257）、
④static-analysisジョブ、⑤nightly.yml、⑥feat/ciブランチでのActions検証を
実施した。**2回目の実行で全8ジョブgreen**（run 27054333051・計2分56秒＝
目標10分以内を大幅クリア）。

### 設計上の決定事項（プランからの具体化）

- **合否判定スクリプト**（`scripts/ci/`・CI/ローカル共用）：
  - `run_testexec.py`：testexec.pyのラッパ。TARGET_OPTIONS／TARGET_RUNの
    生成→実行→出力解析を行い、TAPサマリと終了コードを返す。
    判定は「`All check points passed.` あり・失敗行（TAPの`not ok`／
    非TAPの`## `）なし」。ターゲット構成で不要なテスト
    （`This test program is not necessary.`）は**SKIP扱い**。
    測定系テスト（`check_finish(0)`で完了しPASS_MARKを出力しない
    hrt1・dlynse）は完走マーカ＋異常出力の**個別判定**（SPECIAL_SPEC）
  - `run_testcfg.sh`：testcfg.pyのラッパ（全9テスト明示指定・
    期待値不一致`#TODO#`等の検出）
  - `smoke_sample1.sh`：sample1スモーク（バナー`TOPPERS/ASP3 Kernel`と
    `task1 is running`のgrep。timeout打ち切りは正常扱い）
- **テストリストの選定**（開発機での全件実行結果に基づく）：
  - POSIX：機能テスト36本中34本（dlynse・hrt1を除外）
  - nightly mps2：32本（dlynse・int1・cpuexc1・cpuexc4を除外）
  - nightly zcu102：35本（dlynseを除外）
  - 除外理由はnightly.yml冒頭コメントおよび下記「判明した既知事項」参照
- **apt クロスツールチェーンの注意（1回目の実行で判明）**：
  `--no-install-recommends` 使用時は **`libnewlib-arm-none-eabi`**
  （arm-none-eabi系）・**`libc6-dev-arm64-cross`**（aarch64-linux-gnu）が
  Recommendsのため入らず、ホストの`/usr/include`への流れ込みや`-lc`不在で
  ビルドが失敗する。明示インストールで解決
- **polarfireはubuntu:26.04コンテナで実行**（計画段階の最大リスクが発現
  →コンテナで解決）：
  ランナーのQEMU 8.2は `microchip-icicle-kit` の直接Mモードブート
  （`-bios none -kernel`）**未対応**。8.2ではリセットベクタがeNVM固定で、
  `-bios none`だと何もロードされない（QEMU 8.2の
  `hw/riscv/microchip_pfsoc.c` で確認。カーネルエントリへジャンプする
  リセットベクタROM生成は**QEMU 10.1で追加**）。開発機でもapt版8.2の
  debを展開して無出力を再現済み。いったんbuild-onlyに切替えた後、
  `runs-on: ubuntu-24.04`＋**`container: ubuntu:26.04`（QEMU 10.2.1）**で
  スモーク・nightly全件とも復活（開発機のdockerで build→smoke→testexec
  を事前検証）。GitHub Actionsにubuntu-26.04ランナーイメージが提供されたら
  `runs-on` 切替でコンテナを外せる。
  **26.04の注意**：riscvは `qemu-system-misc` から **`qemu-system-riscv`**
  パッケージに分離。コンテナはroot実行（sudo不要）・checkout前に
  git＋ca-certificatesの導入が必要
- **static-analysisは当面warning運用**（non-blocking・
  `continue-on-error`）：cppcheck＝syssvc/ target/ arch/（PRISTINE除外）、
  clang-tidy＝POSIXのcompile_commands.jsonからsyssvc／linux_gcc／
  tracelogの9ファイル。初回所見はcppcheck 45件（大半は`uninitvar`・
  SILマクロ起因の誤検出を含む）・clang-tidy 1件。レポートはartifacts化
- **README索引**＝`docs/dev/README.md`（ルートREADME.mdは存在しないため
  バッジ追加（任意）は見送り）

### 判明した既知事項（テスト実行基盤関連・要調査含む）

| 事項 | 内容 |
|---|---|
| dlynse | `sil_dly_nse`の実時間較正テスト。QEMU（サイクル精度なし）・POSIX（ホストスケジューリング）では成立しない。**実機でのみ有意味**のためCI対象外 |
| hrt1（POSIX） | **解消済**：itimerの残り時間がμs境界で量子化され近接読出しで1μs逆行していた。HRTは単調非減少が要件のため `arch/posix_gcc/posix_timer_itimer.c` の `get_current_abstim()` に単調クランプを追加（実HRTカウンタ同様・他ターゲットのget_currentと同方針）。POSIX CI対象に復帰 |
| int1 | **mps2＝対応済**（予備NVIC IRQ60をソフト割込み源に整備＝21/21 PASS・CI/nightly対象）。**polarfire＝非適用**（PLICは`ras_int`非対応＝QEMUのSiFive PLICもpending書込み拒否。意図的除外）。zcu102/pico2_arm/pico2_riscvは元から対応 |
| cpuexc1・cpuexc4（arm_m全般） | **原因判明・対処未決**：`SIL_LOC_INT`（arm_mはPRIMASK＝上流とバイト一致）中の `udf` がUsageFault→HardFault昇格し，UsageFault用ハンドラで捕捉できない＝**上流arm_m固有の特性**（我々のバグではない）。BASEPRIロックにすれば通ることは実験確認済みだが上流乖離。詳細・選択肢・実験結果は **`docs/dev/issue-cpuexc-armm.md`**（ユーザー判断待ち） |

### 変更したファイル

| ファイル | 変更内容概要 |
|---|---|
| `docs/dev/README.md` | 索引のCI整備を完了に更新 |
| `AGENTS.md` | §6 MISRA-C：静的解析の実体（static-analysisジョブ・warning運用）を反映 |

### 追加したファイル

- `.github/workflows/ci.yml` — push(main/feat/**)・PR・dispatch。8ジョブ
  （posix／mps2-qemu／zybo-qemu／zcu102-qemu／polarfire-qemu
  (ubuntu:26.04コンテナ)／pico2(build)／stm32mp257(build・ARM公式tarball
  ＋cache)／static-analysis）。fail-fast無効・concurrency・ログartifacts
- `.github/workflows/nightly.yml` — schedule（03:00 JST）＋dispatch。
  QEMU 3ターゲット（mps2 32本／zcu102 35本／polarfire 34本
  (ubuntu:26.04コンテナ)）のtestexec全件
- `scripts/ci/run_testexec.py` — testexec.py CIラッパ（合否判定付き）
- `scripts/ci/run_testcfg.sh` — testcfg.py CIラッパ（合否判定付き）
- `scripts/ci/smoke_sample1.sh` — sample1スモーク判定

> **2026-06-16 追記（M-profile 拡充）**：上記「8ジョブ」は当初構築時の記録。その後
> M-profile QEMU ターゲットを拡充し、`ci.yml` に `mps2-an386-qemu`（M4/v7-M）・
> `mps3-an547-qemu`（M55/v8.1-M+MVE・testexec に `mvevpr` 含む）の build+testexec
> ジョブを追加、`nightly.yml` の TTSP3 ジョブを `ttsp3-mprofile` マトリクス
> （an505／an386／an547 で functional＋staticAPI）化した。経緯は
> `docs/dev/mps2-an386.md`・`docs/dev/mps3-an547.md`・`docs/dev/ttsp3-conformance.md`。

> **2026-06-16 追記（CI使用量の削減）**：Actions 使用量を抑えるため以下を実施。
> - `ci.yml` の `push` を **main のみ**に（`feat/**` を除外）。feat ブランチは
>   `pull_request` で検証＝「PRのある feat push の二重実行」「PR前WIP pushの実行」を停止。
> - `ci.yml` の push/pull_request に **`paths-ignore: ['**.md','docs/**','LICENSE']`**
>   ＝ドキュメントのみの変更では CI スキップ（コードとdoc混在コミットは実行）。
> - `nightly.yml` を **daily → weekly**（cron `0 18 * * 0`＝月曜 03:00 JST）。全件
>   testexec＋TTSP3 functional/staticAPI（3ターゲット）は重いため週次に集約。
>   緊急時は `workflow_dispatch` で即時実行可。
> - 留意：branch 保護で必須チェックを設定している場合、doc専用 PR は当該チェックが
>   走らないため、必要なら保護設定側で除外（path）を調整する。

### Git情報

- ベースコミット：`815938c`（docs(dev): add CI整備 plan）
- 関連コミット：`3f14e30`（workflows＋scripts/ci追加）→
  `3ae173e`（クロスlibc明示＋polarfire build-only切替）→ 記録コミット
- ファイルリスト再現：
  `git diff --stat 815938c main -- .github scripts/ci docs/dev AGENTS.md`

### 検証結果

| 範囲 | 結果 |
|---|---|
| ローカル POSIX（testexec 36本） | 34本パス＋dlynse/hrt1が判定どおり失敗（除外根拠） |
| ローカル QEMU mps2（36本・QEMU 11） | 32本パス（除外4本の根拠取得） |
| ローカル QEMU zcu102（36本・QEMU 8.2） | 35本パス（dlynseのみ失敗） |
| ローカル QEMU polarfire（36本・QEMU 11） | 34本パス（dlynse・int1失敗） |
| ローカル testcfg（9本）・ctest（2本）・pico2ビルド | すべてパス |
| **GitHub Actions CI**（run 27054333051・feat/ci） | **全8ジョブ success**（2分56秒） |
| **GitHub Actions CI**（run 27054531029・mainマージ後） | **全8ジョブ success** |
| **GitHub Actions nightly**（run 27054576183・手動dispatch） | **success**（mps2：32本 2分14秒／zcu102：35本 2分26秒。hrt1は完走マーカ判定・zcu102のcpuexc10はSKIP） |
| **GitHub Actions CI**（run 27055128761・polarfireコンテナ化後） | **全8ジョブ success**（polarfire＝ubuntu:26.04コンテナでQEMUスモーク復活） |
| **GitHub Actions nightly**（run 27055164787・3ターゲット体制） | **success**（mps2 2分8秒／zcu102 2分29秒／**polarfire：34本 2分13秒**＝QEMU 10.2コンテナ） |

> 補足：開発機からのpushはSSH鍵がエージェント経由のため、
> AIセッション内からは実行不可（ユーザーがpushを実施）。gh CLIは導入済み
> だが現在のPATはread権限のみ（run watch／log取得は可能）。

### 後続変更：開発コンテナイメージへの統一（2026-06-06）

devcontainer / Docker 項目（`devcontainer.md`）の一環として、ci.yml・
nightly.yml の全ジョブを開発コンテナイメージ
（`ghcr.io/toppers/asp3_core-dev`・日付タグ参照）での実行に切り替えた。

- ランナーでの apt install／ARM tarball＋actions/cache の手順を削除し
  `container:` 参照に置換（ci.yml＋nightly.ymlで計124行→76行）
- zcu102 はイメージ同梱の aarch64-none-elf を使用（CIだけの
  `A35_TOOLCHAIN_PREFIX=aarch64-linux-gnu-` オーバーライドを廃止し、
  開発機／CI のツールチェーン乖離を解消）
- polarfire の ubuntu:26.04 素コンテナ＋apt の暫定対処（`82bd565`）を
  本イメージに統一。nightly では polarfire を mps2／zcu102 と同一
  マトリクスに統合（別ジョブを廃止）
- イメージの既定ユーザが vscode（devcontainer用）のため、
  `options: --user root` が必要（無いと actions/checkout がランナー
  ファイル `/__w/_temp/...` への書込みで EACCES。run 27057634659 で実測）

**所要時間の実測**（feat/devcontainer・同一コミット相当の連続run）：

（注：2026-06-16 に QEMU ターゲットを mps2-an505/IoTKit へ置換。FPU が CPU0 に実装されハード浮動小数点で再検証可能になった。以降の M33 QEMU 検証は mps2-an505 を使用。下表の `QEMU mps2-an521` 行は置換前の実測記録。）

| ジョブ | 従来（apt install）run 27057520949 | コンテナ run 27057740846 |
|---|---|---|
| POSIX (linux) | 163s | 187s |
| QEMU mps2-an521 | 54s | 61s |
| QEMU zybo_z7 | 68s | 77s |
| QEMU zcu102 | 67s | 81s |
| QEMU polarfire | 92s | 133s |
| pico2 build | 36s | 48s |
| stm32 build | 27s | 48s |
| static-analysis | 43s | 49s |
| **全体（wall）** | **2m47s** | **3m10s** |

イメージpull（非圧縮4.7GB級）のオーバーヘッドは＋10〜40s/ジョブ・
全体では＋23s。再現性の対価として許容と判断し、折衷案（QEMUのみ
イメージから取り出す等）は不採用。
