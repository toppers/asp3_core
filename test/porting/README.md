# 移植検証テスト（test_porting）

新ターゲット移植時の**最初の動作確認**テスト。カーネル基本機能8項目を
TAP形式で機械判定する。sample1（目視）・testexec全件（重い）の前段に置く。
経緯・設計は `docs/dev/porting-test.md`、移植手順上の位置付けは
`docs/porting/PORTING_GUIDE.md` Step 8-2 を参照。

## テスト項目（＝故障切り分けの順序）

| # | 項目 | 確認できること | 落ちたら疑う場所 |
|---|---|---|---|
| 1 | `syslog_output` | ブート〜低レベル出力（UART） | スタートアップ・リンカスクリプト・UART初期化 |
| 2 | `tick_timer_basic` | 高分解能タイマの歩進（割込み不要） | タイマのクロック供給・カウンタ設定 |
| 3 | `task_create_activate` | タスク起動とディスパッチャ | コンテキストスイッチ・スタック設定 |
| 4 | `semaphore_signal_wait` | セマフォ同期（カーネル本体） | （3まで通っていれば稀） |
| 5 | `eventflag_set_wait` | イベントフラグ同期（同上） | 同上 |
| 6 | `alarm_handler` | **タイマ割込みの経路** | 割込みコントローラ設定・ハンドラ登録 |
| 7 | `isr_delayed_dispatch` | **割込みハンドラ出口での高優先度タスクへの遅延ディスパッチ** | 割込み出口のディスパッチ経路（CLIC等，mret非経由出口でHW優先度状態が固着し得るチップで重要） |
| 8 | `wake_from_idle` | **IDLEに割り込んだ割込みからのディスパッチ復帰** | 同上（idle復帰の経路） |

2と6の待ちループは時間上限＋回数上限の二重バウンドで、タイマが
停止していてもハングしない。7・8は項目6までの経路（実行中タスクへの
mret復帰／タスクコンテキストからの自発的dispatch()）が構造的に踏まない
「割込み出口でのディスパッチ」を意図的に踏む（背景は
`docs/dev/porting-test.md`のCLIC出口正規化に関する追記を参照）。

## ビルドと実行

既存の `ASP3_APPLDIR`/`ASP3_APPLNAME` 機構を使う（専用ビルドターゲットは無い）。
TAPフレームワーク `tap.c` を `ASP3_EXTRA_APP_C_FILES` で追加すること。

```bash
# POSIX（linux・ネイティブ実行）
cmake --preset linux -B build/test_porting-linux \
  -DASP3_APPLDIR=test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=test/porting/tap.c
cmake --build build/test_porting-linux
timeout 10 ./build/test_porting-linux/asp
# ctest登録済み（linuxのみ）：
ctest --test-dir build/test_porting-linux
```

QEMUターゲットはプリセット名を替えるだけ（実行コマンドは AGENTS.md §4 と同じ）：

```bash
cmake --preset mps2_an505-qemu -B build/test_porting-mps2 \
  -DASP3_APPLDIR=test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=test/porting/tap.c
cmake --build build/test_porting-mps2
timeout 30 qemu-system-arm -M mps2-an505 -cpu cortex-m33 \
  -kernel build/test_porting-mps2/asp.elf \
  -semihosting -semihosting-config enable=on,target=native -nographic
```

実機ターゲット（pico2_arm 等）は同様にビルドして
`ninja -C <builddir> run`（書込み）＋ `console`（シリアル）で実行する。

## 期待出力と判定

```
# test_porting: kernel porting verification
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

- **合格＝`# 8/8 passed` 行があること**（全ターゲット共通の機械判定。
  QEMU等の終了コードには依存しない）
- 失敗項目は `not ok N - <項目名>` で示される

## ファイル構成

| ファイル | 内容 |
|---|---|
| `test_porting.c` | テスト本体（8項目） |
| `test_porting.cfg` | 静的構成（タスク5・セマフォ2・フラグ・アラーム2） |
| `test_porting_cfg.h` | .c/.cfg共有ヘッダ（優先度・プロトタイプ） |
| `tap.[ch]` | 最小TAPフレームワーク（test_svcとは独立） |
| `expected/` | 構造化ログの期待イベント列（sample1用。AGENTS.md §8） |
