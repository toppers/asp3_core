# 移植検証テスト（test/porting）

## 項目

移植検証テスト（AGENTS.md §1 機能追加計画、優先度：高）

## 内容

新ターゲット移植時の**最初の動作確認**として、カーネル基本機能6項目を
TAP形式で機械判定する `test/porting/` を整備する。
プロトタイプ（AI提案）が旧ツリーにあり、これを取込み・修正して実体化する：
**`/home/honda/TOPPERS/asp3core/asp3_core_org/test/porting/`**
（tap.[ch]＝最小TAPフレームワーク・test_porting.c＝テスト本体・
test_porting.cfg／test_porting_cfg.h・README.md。計約290行）

### 意義（何が嬉しいか）

1. **新ポートの「最初の灯」を機械判定**：sample1（目視）・testexec全件（重い）の
   前段として、6項目のTAPで合否が出る。項目の並びが**故障切り分けの順序**に
   なっている：
   ①syslog_output（ブート・UART）→②tick_timer_basic（タイマ歩進）→
   ③task_create_activate（ディスパッチャ）→④⑤sem/flg（カーネル本体）→
   ⑥alarm_handler（**タイマ割込み経路**）
2. **PORTING_GUIDE Step 8-2 の実体化**：ガイドが先行記述している
   `test/porting/` の移植確認テスト（期待TAP出力例まで一致）が実在する状態になる
3. **直近の利用先＝pico2-riscv**：Xh3irq（新割込みコントローラ）・新ブートの
   切り分けに①②⑥が直接効く。以降のポート（FSP統合等）でも定番手順になる

### プロトタイプからの修正ポイント（取込み時に対応）

| 箇所 | 問題 | 対処 |
|---|---|---|
| `tap_done()` | `qemu_exit(int)` 依存（arm32セミホスティング前提・全ターゲットでは通用しない） | 基本は **`# N/N passed` のパース判定**（run_testexec.py方式・全ターゲット共通）＋ `ext_ker()`。終了コードは linux（ネイティブ）でのみ活用（ctest直結） |
| `test_porting.cfg` | 優先度・スタックサイズ等の定数 | 現行の test/ 慣例（target_test.h）と突合 |
| `expected/sample1.json` | 旧ツリー版 | 現リポジトリ版（CLIターゲットで作成済み）を正とし、重複は取込まない |
| ビルド方法 | README記載の `--target test_porting` | 専用ターゲットは作らず、既存の `ASP3_APPLNAME/APPLDIR` 機構を使用（`-DASP3_APPLDIR=test/porting -DASP3_APPLNAME=test_porting`）。READMEを実手順に更新 |

## 実施プラン

1. **取込み・修正**：`asp3_core_org/test/porting/` → `test/porting/`
   （上記修正ポイントを反映。tap.[ch] は test/porting/ 配下に置き
   syssvc/test_svc.c とは独立のまま）
2. **既存5実行ターゲットで検証**（テスト自体の検証）：
   linux（ネイティブ・ctest登録＝終了コード判定）＋
   QEMU 4機種（mps2／zybo／zcu102／polarfire）で **6/6 passed** を確認
3. **ドキュメント整合**：
   - `PORTING_GUIDE.md` Step 8-2 を実手順（APPLDIR方式のコマンド）に更新
   - `docs/dev/pico2-riscv.md` の実機検証手順を
     「**test_porting → sample1 → testexec**」の順に更新（本コミットで実施済み）
   - `docs/building.md` テストランナ節に追記
4. **CI**（任意・小）：linuxジョブのctestに test_porting が乗る（2の登録で自動）。
   QEMUスモークの置換/追加は効果と時間を見て判断
5. **記録**：README索引・本ファイル実施結果

### スコープ外

- テスト項目の追加（割込み応答時間等の性能系）— 必要になったら拡張
- 実機ターゲット（pico2/stm32）での実行確認 — 各ポートの実機検証時に実施
  （→ pico2-riscv・stm32とも実施済み．実施結果を参照）

## 実施結果

（2026-06-06・実機接続PC（Ubuntu 24.04）で実施）

### 変更したファイル

| ファイル | 内容 |
|---|---|
| `test/porting/tap.[ch]`（新規） | 最小TAPフレームワーク（syslog出力・`tap_done()`＝`# N/N passed`＋`syslog_fls_log()`＋`ext_ker()`．`qemu_exit`依存なし） |
| `test/porting/test_porting.c`（新規） | テスト本体（6項目．②⑥の待ちループは時間＋回数の二重バウンド） |
| `test/porting/test_porting.cfg`・`test_porting_cfg.h`（新規） | 静的構成（タスク4・SEM・FLG・ALM）・共有ヘッダ |
| `test/porting/README.md`（新規） | 項目表（故障切り分け対応）・ビルド/実行手順・判定基準 |
| `CMakeLists.txt`（改変） | ①`ASP3_APPLDIR`/`ASP3_EXTRA_APP_C_FILES`の相対パス指定をソースルート基準で解決 ②linux_gcc＋test_portingビルドでctest登録（`# 6/6 passed`照合） |
| `docs/porting/PORTING_GUIDE.md`（改変） | Step 8-2/8-3を実手順（APPLDIR方式＋ctest）に更新 |

プロトタイプ（旧ツリー）は実機接続PCに存在しなかったため，計画の
修正ポイントを仕様として新規作成した（結果的に取込みと同等）．

### 検証結果

（注：2026-06-16 に QEMU ターゲットを mps2-an505/IoTKit へ置換。FPU が CPU0 に実装されハード浮動小数点で再検証可能になった。下表の mps2-an521（M33）は置換前の検証記録。以降の M33 QEMU 検証は mps2-an505 を使用。）

| 環境 | 結果 |
|---|---|
| linux（ネイティブ） | **6/6 passed**＋ctest 1/1 Passed |
| QEMU mps2-an521（M33） | **6/6 passed** |
| QEMU xilinx-zynq-a9（zybo） | **6/6 passed** |
| QEMU xlnx-zcu102（A53） | **6/6 passed** |
| QEMU microchip-icicle-kit（polarfire） | ビルド成功．実行は**実施PCのQEMU 8.2.2でsample1含め無出力**（本変更と無関係の環境制約）→ ピン留めコンテナのCIで確認する |
| 実機 raspberrypi_pico2_riscv | **6/6 passed**（`docs/dev/pico2-riscv.md` 参照） |
| 実機 stm32mp257f_dk_arm64 | **6/6 passed**（swd-runでロード実行．2026-06-06） |

### Git情報

- ベースコミット：`7f8213b`
- ファイルリスト再現：`git diff --stat <base> main -- test/porting CMakeLists.txt docs/porting/PORTING_GUIDE.md`

### 知見

- 実機（pico2）で**テスト末尾の出力（`# 6/6 passed`）が欠落**する事象が
  あり，原因は終了処理（ATT_TER→`sio_terminate`）が送信FIFOのドレイン
  前にUARTをディスエーブルすることだった．`rp2350_uart_cls_por()` に
  FR.BUSYの待ちを追加して解消（ARM版にも共通の潜在不具合．
  `DIVERGENCE_MAP.md` 参照）．QEMUでは送信が瞬時のため顕在化しない．

### 追記（2026-07-15）：項目7・8追加＝CLIC出口正規化（RISC-V共通部）の回帰検出穴を解消

asp3_esp_idf側のESP32-C5移植（CLIC搭載チップ）で，RISC-V共通部
（`arch/riscv_gcc/common/core_support.S`）の割込み出口のうち2経路
（idle復帰の`j dispatcher_0`／遅延ディスパッチの`j dispatcher`）が
mretを経由しないことが実機で致命的な固着バグ（CLICのmintstatus.mil
がmretでしか降格できずハングする）として顕在化した
（`asp3_esp_idf/docs/c5-clic-exit-fix-review.md`）。原因調査の過程で，
**旧来の6項目はこの2経路を構造的に一度も踏まないため検出できない**
ことが判明した（項目6までは実行中タスクへのmret復帰か，タスク
コンテキストからの自発的`dispatch()`のみ）。

この2経路をFMP3と同じ「出口正規化型」（mepc設定＋MPP=M＋MPIEクリア＋
`mret`。非CLICチップのirc_end_intはmcauseに触れないため無害）へ変更
したのに合わせ，本項目7（`isr_delayed_dispatch`）・8（`wake_from_idle`）
を追加し，6項目→8項目・`# 6/6 passed`→`# 8/8 passed`へ変更した
（`CMakeLists.txt`のPASS_REGULAR_EXPRESSIONも同時に更新）。

- 項目7：割込みコンテキスト（`alarm2_handler`）から`sig_sem()`で自分より
  高優先度のタスクを起床させ，「割込み出口での遅延ディスパッチ」を踏む。
- 項目8：`dly_tsk()`でCPUを真にIDLEにし，満了タイマ割込みが自タスクを
  起床させる「IDLEに割り込んだ割込みからのディスパッチ復帰」を踏む。
  復帰後もtick進行が続くことまで確認し，固着（全割込み永久ブロック）の
  非再発を機械判定する。

検証：QEMU esp32c3（非CLIC）8/8・実機ESP32-C3/ESP32-C6（非CLIC）8/8・
POSIX linux 8/8＋ctest。CLIC実機（ESP32-C5）での等価性確認は
`asp3_esp_idf/docs/c5-bringup.md`側の実施記録を参照。
**他リポジトリ（asp3_pico_sdk/asp3_fsp/asp3_stm32cube/asp3_mcuxsdk/
asp3_esp_idf）がドキュメントで参照している「test_porting 6/6」は，
次回submodule bump時に「8/8」へ読み替えが必要**（本変更のみでは
他リポジトリのドキュメントは自動更新されない）。
