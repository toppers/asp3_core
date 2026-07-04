# ESP32-C6ターゲット

## 項目

RISC-V Hazard3ターゲット等と並ぶ新規ターゲット追加（AGENTS.md §1
「機能追加計画」には現時点で正式項目化していない＝ユーザー依頼による
先行調査・Phase A着手。ESP-IDF統合（ESP32-C3）の横展開候補）。

## 内容

ESP32-C3向けに構築したDirect Boot＋ベアメタルASP3ターゲット
（`arch/riscv_gcc/esp32c3/`・`target/esp32c3_gcc/`）と同じアプローチを
ESP32-C6（RISC-V HPコア＋LPコア，Wi-Fi 6/BLE5/IEEE802.15.4，実機
ESP32-C6FH4 rev v0.2）へ展開する。

事前調査（別セッションの調査フォーク）により，C6はC3と比べ以下が
異なることが判明している：

- 割込みコントローラがC3のINTMTX（割込みマトリクス＋ソフトウェア
  THRESHネスト）とは別方式の**CLIC**（Espressif独自の非標準CLIC．
  MTVT_CSR=0x307・MINTTHRESH_CSR=0x347・MINTSTATUS_CSR=0xFB1，
  ハードウェアベクタディスパッチ）。本プロジェクト4つ目の割込み方式
  （Xh3irq／PLIC／INTMTX+SWに続く）。
- クロック制御がSYSTEM_*レジスタから新設のPCR（Peripheral Clock and
  Reset）ペリフェラルへ移動。
- メモリマップが異なる：C6はSOC_DROM_LOW＝SOC_IROM_LOW（共に
  0x42000000）＝IROM/DROM分離なし（C3はIROM 0x42000000／DROM
  0x3C000000の分離があり，そのためのVMA==LMAリンカスクリプトの
  工夫が必要だった）。C6はこの分離が無い分，リンカスクリプトは
  むしろC3より単純になる。
- Wi-Fi blob（esp32-wifi-lib）はasp3_esp_idfが既にpin済みの
  esp-hal-3rdpartyコミットに同梱済み＝submodule更新不要。

## 実施プラン

Phase A（ベアメタル）を，CLIC実装（設計判断を要する＝ユーザー確認
必須）とそれ以外に分割し，まず**割込み完全OFFでのboot＋クロック＋
コンソール検証**を第1マイルストーンとする（C3の時と同じ順序：
boot/clock/consoleを固めてから割込みに着手）。

1. `arch/riscv_gcc/esp32c6/`・`target/esp32c6_gcc/`をC3の同名
   ディレクトリを雛形に作成。
2. レジスタアドレスはesp-hal-3rdparty（asp3_esp_idf/hal submodule，
   既にC6対応済み）から採用し，実機で逐次検証する。
3. 第1マイルストーンはASP3カーネル本体（cfg生成・
   chip_kernel_impl.c等）には未統合のスタンドアロン検証プログラムと
   する（ASP3カーネルは最低限タイマ割込み・ソフトウェア割込みによる
   ディスパッチを要するため，CLIC無しでは「カーネルとして動く」
   マイルストーンには本質的に到達できない。よってCLIC実装は次段階の
   前提条件であり，スコープを分けるのは必然）。
4. CLIC実装は別途，ユーザーと設計判断（ソフトウェアディスパッチで
   通すか，ハードウェアベクタ方式＝MTVTジャンプテーブルを採用するか）
   を確認してから着手する。

## 実施結果（第1マイルストーン．2026-07-04）

実機（ESP32-C6FH4 rev v0.2，`/dev/ttyACM1`）で以下を確認した：

- **Direct Boot成立**：C3と同じマジックナンバー
  （`0xaedb041d`×2＋flash+8へのジャンプ）で実機起動が成立。公式の
  `espressif/esp32c3-direct-boot-example`はC3/H2のみ対応と明記して
  いるが，C6でも同じ機構が動作することを実機で確認した（未文書化の
  知見）。
- **ウォッチドッグ無効化**：無効化しないと`TG0_WDT_HPSYS`
  （TIMG0ウォッチドッグ）によって数秒以内にリセットが繰り返される
  （起動ログで確認）。TIMG0／TIMG1／LP_WDT／SWD（スーパーWDT）とも
  C3と同じ解錠キー（`0x50D83AA1`／`0x8F1D312A`）で無効化に成功。
  LP_WDTのレジスタヘッダは自動生成ドキュメントが不完全
  （`need_des`）で解錠キーの実値が読み取れなかったが，C3と同じ値を
  試したところ実機で無効化成功を確認した。
- **コンソール**：本ボードはUSB Serial/JTAGのみ（UART0は未配線．
  ポーリング送信を試みたところ無出力＝C3の時と同じ既知パターン）。
  USB Serial/JTAGのレジスタレイアウトはC3と完全に同一
  （EP1=+0x00，EP1_CONF=+0x04，WR_DONE/IN_DATA_FREEビットとも同じ），
  ベースアドレスのみ異なる（`0x6000F000`）。
- 上記3点を組み合わせた最小プログラムで，割込み完全OFF
  （`mstatus.MIE`を一切有効化しない）のまま，polled USB Serial/JTAG
  経由でのバナー文字列の連続出力が安定動作することを実機で確認した。

### 変更したファイル

なし（すべて新規）。

### 追加したファイル

| ファイル | 内容 |
|---|---|
| `arch/riscv_gcc/esp32c6/esp32c6.h` | 実機検証済みレジスタ定義（USB Serial/JTAG・TIMG WDT・LP_WDT・メモリマップ）。CLIC・PCRクロックは意図的に未定義（次段階） |
| `target/esp32c6_gcc/flash_header.S` | Direct Bootヘッダ（C3と同一） |
| `target/esp32c6_gcc/smoke/` | スタンドアロン検証プログラム（start.S・main.c・link.ld・README.md）。ASP3カーネル本体には未統合 |
| `docs/dev/esp32c6-target.md` | 本ファイル |

### 削除したファイル

なし。

### Git情報

- ベースブランチ：`feat/esp32c3`（`69a132d`時点）
- 作業ブランチ：`feat/esp32c6`
- ファイルリスト再現コマンド例：`git diff --stat feat/esp32c3 feat/esp32c6`

### 検証結果

| テスト | 実施 | 結果 |
|---|---|---|
| POSIX | − | 対象外 |
| QEMU | − | 未確認（Espressif版QEMUにesp32c6マシンがあるか未調査） |
| 実機（スタンドアロン検証プログラム） | ○ | boot＋WDT無効化＋polled USB Serial/JTAGコンソールの連続動作を確認 |
| ASP3カーネル本体（cfg統合） | − | 未着手（CLIC実装が前提条件） |

### DIVERGENCE_MAP との関連

新規ターゲットにつき該当なし（`kernel/`等PRISTINE領域への変更はゼロ）。

### 残作業（第1マイルストーン時点）

- ~~CLIC割込みコントローラの実装~~ → **重要な訂正（第2マイルストーン
  で判明）**：ESP32-C6のHPコアはCLICを使わない。詳細は下記。
- PCR経由のCPUクロックPLL切替（現状はリセット既定クロックのまま．
  周波数未計測・dlynse較正未実施）。
- SYSTIMER（HRT）の実機検証。
- LPコアとの相互作用の確認（本マイルストーンはHPコアのみ対象．
  未確認）。

## 実施結果（第2マイルストーン．割込みコントローラ＋ASP3カーネル本体統合．2026-07-04）

### 重要な訂正：ESP32-C6はCLICを使わない

第1マイルストーンの調査時点ではCLIC（Espressif独自の非標準CLIC）を
想定していたが，これは誤りだった。実際にはesp-hal-3rdparty
（`asp3_esp_idf/hal` submodule）のchip別`soc_caps.h`を直接確認した
ところ，**`SOC_INT_CLIC_SUPPORTED`はESP32-C6には定義されていない**
（定義があるのはC5/C61/H4/H21/P4rev2+/S31のみ）。ESP32-C6は
**`SOC_INT_PLIC_SUPPORTED`を定義**しており，そのレジスタ実体
（`plic_reg.h`）はEspressifが「PLIC」と命名しているが，中身は
**C3の割込みマトリクス（INTMTX）CPU側制御ブロックと全く同じ方式**
（ENABLE／TYPE／CLEAR／EIP_STATUS／PRI×32／THRESHの単純な
メモリマップトレジスタ．標準RISC-V PLIC＝claim/completeレジスタ方式
ではない）。ソースルーティングもC3と同じ`INTMTX_BASE + 4*source`書込み
方式（`esp-hal-3rdparty`のriscv/interrupt_plic.c・
hal/interrupt_plic_ll.hで実装確認済み）。mtvecモードも
`MTVEC_MODE_CSR=1`＝C3と同じ標準RISC-Vベクタドモード。

相違点は次の3点のみ：
1. ソースルーティング（`INTMTX_BASE=0x60010000`）とCPU割込み線制御
   （`PLIC_MX_BASE=0x20001000`．異なるアドレス空間）が分離している
   （C3は単一の`INTMTX_BASE`に両方が同居）。
2. ペリフェラル割込みソースが77本（C3は62本）のため生ステータス
   レジスタが3ワード（C3は2ワード）。
3. ソフトウェア割込み（FROM_CPU_n）が`INTPRI_BASE=0x600C5000`
   ペリフェラルにある（C3の`SYSTEM_BASE`相当）。

したがって`intmtx_kernel_impl.h`・`chip_support.S`はC3のロジックを
ほぼそのまま踏襲し，レジスタオフセットのみ差し替えた（新規の割込み
方式の実装ではない）。

### ASP3カーネル本体の統合状況

`arch/riscv_gcc/esp32c6/`・`target/esp32c6_gcc/`一式をC3の同名
ディレクトリを雛形に作成し，`git diff --stat feat/esp32c3 feat/esp32c6`
で全差分を再現できる。主な変更点：

- `esp32c6.ld`：C6はSOC_DROM_LOW＝SOC_IROM_LOW（共に0x42000000）で
  IROM/DROM分離が無いため，C3のVMA==LMAトリック（`.text`と`.rodata`を
  別ORIGINに分離）は不要と判明。`.text`セクション1つに
  flash_header／entry／code／rodataをまとめる，より単純なリンカ
  スクリプトにした。
- `target_kernel_impl.c`：WDT無効化（第1マイルストーンで実機検証済み
  のロジックをそのまま流用）。**CPUクロックのPLL切替は未実施**（C6は
  クロック制御がPCRペリフェラルに移動しており，C3のSYSTEM_CPU_PER_CONF
  相当の単純な2bit選択ではなくSPLL起動シーケンス自体が必要．誤った
  レジスタ操作のリスクを避け，リセット既定クロックのまま動作させて
  いる＝CORE_CLK_MHZ=40固定・SIL_DLY_TIM1/2は暫定値・未較正）。
- CMakePresets.jsonに`target/esp32c6_gcc/presets.json`を追加登録。

**ビルド結果**：`cmake --preset esp32c6 -B build/esp32c6 && cmake --build
build/esp32c6`が0エラーで成功（サンプルsample1，FLASH 25360B・RAM
12848B）。レジスタオフセットの逆算（esp-hal-3rdpartyのヘッダを
1つずつ確認）が正確だったため，初回のフルビルドでコンパイル・
リンクとも一発で成功した。

**実機起動結果（部分成功）**：esptoolで書込み・実機起動したところ，
ASP3カーネルのバナー（複数行）が実機のUSB Serial/JTAGコンソールに
正しく出力された。これはDirect Boot・WDT無効化に加え，**実機で
SYSTIMER割込み・INTMTX/PLIC_MXベースの割込みルーティング・
ソフトウェア優先度昇格（THRESH）・USB Serial/JTAGの割込み駆動
コンソール出力（ISRベース，本マイルストーンで初めて統合）が
実際に機能したことを意味する**（すべて割込みが絡む機能であり，
mie/mip CSR非搭載の前提＝TOPPERS_OMIT_MIE_INITがここまでは
実機で成立していることも確認できた）。

**未解決のバグ**：バナー出力後，`logtask_main`が起動して最初の
`syslog_1("System logging task is started on port %d.", ...)`を
呼び出す直前後で処理が停止する（コンソール出力が"Sy"の2文字で
途切れる）。OpenOCD＋JTAGで追跡したところ，`mcause=2`
（Illegal Instruction）の例外が発生していることを確認した
（実機JTAG接続にはtarget/esp32c6.cfgのCPUTAPID既定値
`0x0000dc25`が実機の実際のJTAG IDCODE`0x00005c25`と一致せず接続
できない問題があり，`target/esp32c6.cfg`のローカルコピーを作成して
IDCODEを実機に合わせて上書きすることで接続した）。ただし例外発生後の
mepc/mtvecの値が想定外のアドレス（プログラム範囲外・ROM領域寄り）を
指しており，複数回の例外が連鎖した後の状態を観測している可能性が
高く，**「最初の」不正命令が実際にどこで発生したかは未特定**。
次回の調査はリセット直後に`logtask_putc`／`logtask_main`へ
ブレークポイントを張った状態で`continue`し，最初の到達点で止めて
そこから数命令ずつ追う方針が有効と考えられる（本セッションでは
JTAGセッション中に長時間ブロックする`continue`コマンドが実行環境の
制約で中断されたため未完了）。

### 変更したファイル（第2マイルストーン）

| ファイル | 内容 |
|---|---|
| `arch/riscv_gcc/esp32c6/esp32c6.h` | CLIC誤解を訂正．INTMTX/PLIC_MX両ベースアドレス・PCR・INTPRI・SYSTIMER・割込みソース番号を追加 |
| `CMakePresets.json` | `target/esp32c6_gcc/presets.json`の登録 |

### 追加したファイル（第2マイルストーン）

`arch/riscv_gcc/esp32c6/`（chip_kernel_impl.c/h・intmtx_kernel_impl.h・
chip_support.S・chip.cmake・chip_kernel.h/.py・chip_os_awareness.py・
chip_rename.def/.h・chip_unrename.h・chip_serial.c/.h/.cfg・
chip_sil.h・chip_stddef.h・chip_asm.inc・esp32c6_uart.c/.h・
esp32c6_usbjtag.c/.h）＋`target/esp32c6_gcc/`
（esp32c6.ld・presets.json・run.cmake・target.cmake・
target_kernel_impl.c/.h・target_timer.c/.h・target_kernel.cfg/.h/.py・
target_timer.cfg・target_serial.cfg/.h・target_test.h・target_sil.h・
target_stddef.h・target_syssvc.h・target_asm.inc・target_cfg1_out.h・
target_check.py・target_os_awareness.py・target_rename.def/.h・
target_unrename.h）。すべてC3の同名ファイルを雛形に作成。

### 検証結果（第2マイルストーン）

| テスト | 実施 | 結果 |
|---|---|---|
| ビルド（`cmake --preset esp32c6`） | ○ | 0エラーで成功 |
| 実機起動（バナー表示） | ○ | 複数行のバナー出力を確認＝割込み（SYSTIMER・PLIC_MXルーティング・THRESH昇格・USB Serial/JTAG ISR）が実機で機能 |
| 実機起動（logtask以降） | ✗ | `logtask_main`の初回syslog呼出し付近でIllegal Instruction例外＝原因未特定 |
| test_porting | − | 未実施（上記バグ解消が前提） |

### 残作業（第2マイルストーン時点）

- **最優先**：`logtask_main`起動直後のIllegal Instruction例外の
  原因特定（JTAG単一ステップでの追跡を継続すること）。
- PCR経由のCPUクロックPLL切替（未実施．リセット既定クロックのまま）。
- SYSTIMER（HRT）のタイミング精度検証（dlynse較正含む）。
- LPコアとの相互作用の確認（未確認）。
- 上記解消後，test_porting（6項目）での動作確認。
