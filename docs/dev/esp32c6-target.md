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
  原因特定（下記「調査継続（同日）」参照．次のセッションはここから
  再開すること）。
- PCR経由のCPUクロックPLL切替（未実施．リセット既定クロックのまま）。
- SYSTIMER（HRT）のタイミング精度検証（dlynse較正含む）。
- LPコアとの相互作用の確認（未確認）。
- 上記解消後，test_porting（6項目）での動作確認。

## 調査継続（同日）：logtask クラッシュの深掘り

第2マイルストーンの未解決バグについて，実機JTAG（OpenOCD＋GDB）と
一時的なコード計装の両方で追加調査した。**根本原因はまだ特定できて
いない**が，以下を確定させた。以降の作業は，この記録を前提に続ける
こと（同じ調査をやり直さないため）。

### 再現手順（100%再現）

```bash
cmake --build build/esp32c6
esptool --chip esp32c6 --port /dev/ttyACM1 write-flash 0x0 build/esp32c6/asp_flash.bin
# USB Serial/JTAG（/dev/ttyACM1，115200bps）をpyserialで開き，
# RTSトグルでリセットしてから読み出す（esptool自身のリセットでは
# 起動バナーの先頭を取りこぼすため）
```

バナー（複数行）が出力された直後，`logtask_main`の最初の
`syslog_1("System logging task is started on port %d.", ...)`の
出力が"S"または"Sy"（実行のたびに1〜2文字．非決定的）で止まる。
以降は無出力（ハングではなく，JTAGで確認すると割込み自体は生き続けて
いる＝下記参照）。

### JTAG接続の設定（実機固有の既知の問題）

`openocd-esp32`同梱の`target/esp32c6.cfg`は`_CPUTAPID 0x0000dc25`を
既定値としているが，本実機（rev v0.2）の実際のJTAG IDCODEは
`0x00005c25`（`part:0x0005`対`part:0x000d`）で一致せず接続できない。
対処：`target/esp32c6.cfg`のローカルコピーを作成し`_CPUTAPID`を
実機値に書き換え，`-s <ローカルコピーのディレクトリ>`を`-s <本来の
scriptsディレクトリ>`より前に指定してOpenOCDの`find`で優先させる：

```bash
mkdir -p /tmp/ocd_scripts/target
cp $OPENOCD_ESP32/share/openocd/scripts/target/esp32c6.cfg /tmp/ocd_scripts/target/
sed -i 's/0x0000dc25/0x00005c25/' /tmp/ocd_scripts/target/esp32c6.cfg
openocd -s /tmp/ocd_scripts -s $OPENOCD_ESP32/share/openocd/scripts \
  -f board/esp32c6-builtin.cfg -c "gdb_memory_map disable" -c "gdb_flash_program disable"
```

（`gdb_memory_map disable`／`gdb_flash_program disable`が無いと，GDB
接続時にOpenOCDがFreeRTOS前提のフラッシュ書込みアルゴリズムを実行
しようとして`get(csr_uie) failed`等で失敗する＝本ターゲットはASP3で
FreeRTOSではないため）。

### 重大な制約：ライブJTAGデバッグがこのボードでは実質使えない

本ボードはUSB Serial/JTAGの**同一物理USBポートがJTAGデバッグ用と
ターゲット自身のコンソール出力用を兼ねる**（milestone 1から既知）。
これが，ターゲットが**実際に動作中**（＝USB Serial/JTAGハードウェアを
コンソールとして能動的に使用中）にJTAGで`continue`＋ブレークポイント
待ちをすると，高確率で以下が発生することを確認した：

- `libusb_bulk_write error: LIBUSB_ERROR_NO_DEVICE`
- `esp_usb_jtag: device not found!` → `failed to revive USB device!`
- `[esp32c6] Hart unexpectedly reset!`
- GDBの`continue`が**数分単位でハングし，実行環境のタイムアウトで
  中断される**（ハードウェアブレークポイント`hbreak`使用時も同様に
  再現した＝ソフトウェアブレークポイントのフラッシュ書込み制約の
  問題ではなかった）。

このため，「リセット→ブレークポイント→continueでヒット」という
通常のライブデバッグフローはこのボード構成では信頼できない。
**有効だった代替手段**：ターゲットを（JTAGなしで）プレーンなpyserial
キャプチャで自然にクラッシュさせ，その**crashed/stuck状態のまま**
（新たにresetをかけずに）OpenOCD＋GDBを接続し，`monitor halt`＋
メモリ／レジスタの直接読出し（`x/`・`monitor reg`）だけを行う（`continue`
や新規ブレークポイントは使わない）。この方法は安定して機能した。

### JTAGで確認できた事実

- クラッシュ後に`monitor halt`すると，**PCが`0x42033da0`〜
  `0x42036800`付近（試行ごとに微妙に異なるがこの狭い帯域内）で安定
  する**。この番地は本プログラムの`.text`範囲（`0x42000000`〜
  `0x42006310`）の**外側**＝未使用（消去済み＝0xFF相殺）のフラッシュ
  領域であり，`nm`にも存在しないシンボル。→ **コードがプログラム
  範囲外の未初期化フラッシュへ迷い込んで実行し続けている**
  （不正命令として繰り返し例外化している可能性が高い＝いわゆる
  「ワイルドジャンプ」）。
- `mcause`を読むと，あるとき`0x00000002`（Illegal Instruction，例外），
  別のときは`0x80000001`（最上位ビット＝割込み，下位5bit=1＝CPU割込み
  線1＝SYSTIMER/FROM_CPU_0）。**両方が交互に観測される**ことから，
  「ワイルドジャンプ先で不正命令を実行→例外→（何らかの理由で）元の
  番地へ戻る／類似番地に留まる」というループの最中に，タイマ割込み
  （線1）も定期的に割り込んで処理されている，と解釈するのが自然
  （優先度上，SIO＝線2とTIMER＝線1は同じ内部優先度2のため互いに
  ネストしないが，タイマは別に定期発火し続けている）。
- `arch/riscv_gcc/esp32c6/esp32c6_usbjtag.c`のISR
  （`esp32c6_usbjtag_isr_siop`）・送信関数（`esp32c6_usbjtag_snd_chr`）
  に一時的なグローバルカウンタを仕込み（診断用．**リバート済み，
  現在のツリーには残っていない**），クラッシュ後にJTAGで
  `x/8xw &esp32c6_dbg_counters`を読んだところ**全カウンタが0**
  だった。しかしバナー（数百文字）は同じ`esp32c6_usbjtag_snd_chr`
  経路（`syssvc/serial.c`の`serial_snd_chr`→`sio_snd_chr`）を通って
  実際に出力されている（シリアルキャプチャで確認済み）。
  → **クラッシュ後のある時点で，カウンタを含むBSS領域の一部（또는
  全体）が再ゼロ化されている**と考えられる。最有力の解釈は，
  ワイルドジャンプの着地点が`arch/riscv_gcc/common/start.S`の
  BSSクリアループ（またはそれに類似する処理）のアドレス範囲に
  偶然重なり，完全なハードウェアリセットを経ずに部分的な
  再初期化が起きている，というもの（ただしこの仮説はアドレス
  レベルでは未検証＝start.Sのbssクリアループの実アドレスと
  0x42033da0〜0x42036800を突き合わせていない）。

### 除外できた仮説

- **受信（OUT_RECV_PKT）割込みが引き金**：
  `esp32c6_usbjtag_ena_cbr`のSIO_RDY_RCVケースを一時的に無効化して
  再現テストしたが，**同じ箇所でクラッシュが再現した**＝受信パスは
  無関係と判断できる。
- **THRESH優先度のネスト処理のオフバイワン**：TIMER割込みとSIO割込みは
  同じ内部優先度（2）のため，そもそも互いにネストできない設計になって
  おり，かつバナー出力の間だけで数百回分のTIMER/SIO割込みが問題なく
  処理されている（クラッシュが起きるのはロジック上「2回目のメッセージ」
  の冒頭のみ）ことから，`irc_begin_int`/`irc_end_int`の基本ロジックの
  単純なオフバイワンではないと考えられる（ただし完全には否定できない）。
- **レジスタオフセットの取り違え**（USB Serial/JTAG・PLIC_MX・INTMTXの
  各アドレス）：esp-hal-3rdpartyのヘッダと逐一突き合わせ済み・
  milestone 1の実機ポーリング動作でも実証済みのため疑わしさは低い。

### 有力な仮説（未検証・次に確認すべきこと）

`arch/riscv_gcc/common/core_kernel_impl.c`の`default_int_handler`
（**共有／未改変のコード**）は，未登録の割込みが発生すると
`syslog_0`/`syslog_1`を呼んでから`ext_ker()`（カーネル終了）を呼ぶ。
**もし何らかの理由でCPU割込み線1・2・3以外の線で割込みが発生して
いれば**，このパスに入る。`intmtx_initialize()`は全77ソースの
MAPレジスタを0に，全32 CPU線のENABLEを0に初期化しているため，
理論上は線1〜3以外が発火するはずはないが，以下は未確認：

1. `intmtx_initialize()`の全77ソースクリアが実際に実機で効いているか
   （ROM/ブートローダが起動時に独自にルーティングした古いMAP設定が
   残っていないか，レジスタ書込み自体が実機で有効か）。
2. `default_int_handler`が実際に呼ばれているかどうか（"Unregistered
   interrupt occurs."という文字列が出力されていれば直接の証拠になる
   が，これまでのキャプチャでは見えていない＝出力される前に自身が
   クラッシュしている可能性もある）。

次のセッションでの推奨アプローチ：
- `default_int_handler`（共有コードだが，一時的な診断計装は許容範囲）
  の先頭に，ISRカウンタと同様のグローバルカウンタ＋呼ばれたintno値を
  記録する処理を仕込み，クラッシュ後にJTAGの`monitor halt`＋
  メモリ直接読出しで確認する（`continue`は使わない．今回確立した
  「クラッシュさせてから安全にJTAG接続する」手順に従うこと）。
- 上記のワイルドジャンプ先アドレス（`0x42033da0`〜`0x42036800`付近）
  と，`arch/riscv_gcc/common/start.S`のbssクリアループの実アドレスを
  `nm`／`objdump`で突き合わせ，本当に部分再初期化が起きているのかを
  確認する。
- 実機の`.text`終端（`0x42006310`）から`0x42033da0`まではまだ広い
  ギャップがある（約180KB）。この間に何があるか（他のセクション・
  リンカスクリプトの割付）を再確認する価値がある。

## 調査継続（2セッション目）：真因は「wild jump」ではなく「割込み待ちハング」だった

前回セッションの2大仮説（BSS再クリアループ重複／defaultハンドラの
不正呼出し）を検証する前に，まず両方とも**却下**できた：

- **BSSクリアループ重複説は幾何学的に否定**：`readelf -S build/esp32c6/asp.elf`
  で`.text`は`0x42000000`〜`0x42006310`のみ．`start.S`のBSSクリアループは
  この`.text`内に完全に収まっており，観測されたワイルドジャンプ先
  （`0x42033da0`〜`0x42036800`）とは無関係（180KB以上離れている）。
- **JTAGはこのセッションの実行環境では一切使用不可**：`openocd`は
  実機・スタンドアロン問わず起動直後に無出力のままexit code 144で
  終了する（`dangerouslyDisableSandbox`を付けても同じ）。前回記録の
  CPUTAPID修正（`/tmp/ocd_scripts/target/esp32c6.cfg`）を適用しても
  起動しない＝今回のセッションのサンドボックス環境固有の制約。

そのため，JTAGに頼らない**ポーリング出力による生レジスタダンプ**で
代替調査を行った（`sample/sample1.c`の`cpuexc_handler`にEXCNO_IINST
発生時の即時ダンプを一時計装＝`ESP32C6_DIAG_EXC_DUMP`ビルドオプション，
`target/esp32c6_gcc/target.cmake`に追加）。

### 決定的な発見：CPU例外は一切発生していない

`cpuexc_handler`（`DEF_EXC(CPUEXC1={EXCNO_IINST}, cpuexc_handler)`＝
`sample1.cfg`で全ターゲット共通登録済み）の先頭に，`target_fput_log`
直呼び出しによる生レジスタダンプを仕込んだところ，**クラッシュ再現時に
一切出力されなかった**。`core_support.S`の`core_exc_entry`／
`nk_exc_entry_1/2`はどちらの分岐でも無条件に`exc_table[excno]`を
`jalr`で呼び出す実装（共有コード，C3と同一）であり，かつ例外処理は
必ず`istkpt`（非タスクスタック）上で実行される設計のため，
**logtaskのスタックオーバーフローがあってもこのダンプ自体は安全に
動作するはず**．にもかかわらず無出力＝前回セッションのJTAG観測
（`mcause=2`，PC=`0x42033da0`付近）は，**イリーガル命令例外が実際に
発生したのではなく，このボード固有のJTAG／USB共用機構が原因で
`monitor halt`時のPC/mcauseの読出し自体が信頼できなかった**可能性が
高い（WFI中のハルトでdpcを誤読するriscv debug moduleの既知の癖と
類似）。**「wild jump」は事実として撤回する**。

### 真因：`logtask_main`が「送信可能」割込みを待ったまま永遠にブロックする

`syssvc/logtask.c`の`logtask_main`に段階チェックポイント
（`[M1]`〜`[M4]`，`target_fput_log`直呼び出し）を仕込み，実機で
再現したところ：

```
[M1][M2][M3][M4]Sy[E]
```

`[M1]`=関数先頭，`[M2]`=`serial_opn_por()`直後，`[M3]`=`syslog_msk_log()`
直後，`[M4]`=最初の`syslog_1()`直後——**ここまで全て正常に完走している**
（`syslog_1(LOG_NOTICE, ...)`は`syslog_msk_log`でlowmask=EMERGのみに
絞ったため，実際には低レベル即時出力されずログバッファへの登録のみで
即座に戻る．これが`[M4]`まで一瞬で到達する理由）。

その後，`logtask_main`のメインループが`syslog_print(&syslog,
logtask_putc)`でバッファ済みメッセージを1文字ずつ`logtask_putc`→
`serial_wri_dat`（`syssvc/serial.c`，**キュー＋割込みコールバック駆動の
送信経路．banner表示や`[M1]`〜`[M4]`が使った直接ポーリング経路
＝`target_fput_log`とは別物**）で出力し始める．"S""y"の2文字は
`serial_wri_chr`内の直接書込み高速パス（`snd_count==0`かつ
`sio_snd_chr`が即座に成功）で出力できたが，3文字目（"s"，"System"の続き）
で初めてハードウェアFIFOが埋まっており`sio_snd_chr`が失敗，
`serial_snd_chr`が`sio_ena_cbr(p_siopcb, SIO_RDY_SND)`を呼び出した
（`esp32c6_usbjtag_ena_cbr`に一時計装した`[E]`マーカーで確認．
**これがこのボードで初めてUSB Serial/JTAGのIN_EMPTY割込みを有効化する
瞬間**）。文字はバッファに積まれ（`snd_bufsz`次第だが，このケースでは
すぐに満杯＝`buffer_full=true`），`sig_sem`が呼ばれないまま
`serial_wri_dat`が返り，**次の文字の送信で`wai_sem(snd_semid)`が
呼ばれてブロックし，そのまま永遠に戻ってこない**。

`esp32c6_usbjtag_isr_siop`（実際のISR本体）の先頭に軽量マーカー
（`'{'`）を仕込んで確認したところ，**`[E]`以降このマーカーは一度も
出力されない＝IN_EMPTY割込みのISRが一度も実行されていない**。
つまり，`sio_ena_cbr`でペリフェラルのINT_ENAビットを有効化しても，
**CPU側に実際の割込みが配送されない**．

### レジスタダンプで確認した状態（1回限りの計装．現在のツリーには残っていない）

`[E]`直後の1回限りのダンプで確認した値（`sio_ena_cbr(SIO_RDY_SND)`が
`INT_ENA`へ書き込んだ直後）：

| レジスタ | 値 | 意味 |
|---|---|---|
| USBJTAG INT_RAW | `0x0000310a` | bit3（IN_EMPTY）は**既に立っている**＝banner出力中（ポーリング経路．INT_CLRに一切触れない）で発生した送信完了イベントが，一度もクリアされずずっと残留していたと推定 |
| USBJTAG INT_ENA | `0x0000000c` | bit2（OUT_RECV_PKT，`serial_opn_por`のSIO_RDY_RCV登録で既に有効）＋bit3（IN_EMPTY，今回有効化）＝期待通り |
| USBJTAG INT_ST | `0x00000008` | bit3のみ＝RAW&ENA．ペリフェラル自身は「IN_EMPTY割込み中」と認識している |
| PLICMX_ENABLE | `0x00000006` | bit1（TIMER）＋bit2（SIO）＝CPU側の線2許可も期待通り有効 |
| PLICMX_THRESH | `0x00000001` | 内部優先度0をブロックする閾値（`irc_begin_int`等で昇格していない素の状態） |
| PLICMX_EIP | `0x00000004` | bit2（線2）が**CPUに到達している状態として見えている** |
| mstatus | `0x00000001` | この時点ではCPUロック中（`loc_cpu()`内）につきMIE=0（bit3）は想定通り |

`unl_cpu()`直後（CPUロック解除後）の再ダンプでは`mstatus=0x00000009`
（MIE=1，正常に復帰）・`PLICMX_EIP=0x00000004`（線2が**そのまま
pendingで残り続ける**）を確認した。つまり，**MIE=1かつEIPで
pending表示のまま，CPUは一切トラップを起こさない**。

（注：この2つ目のダンプを`syssvc/serial.c`の`serial_wri_chr`内に
仕込んだ際，同じ1バイトFIFO/1パケットのUSB Serial/JTAGハードウェアを
診断出力自身が奪い合う形になり，本来の文字送信と診断出力が競合して
何十回も`[U:...]`が連続出力される副作用が観測された＝**診断計装自体が
Heisenbugを作った**．この重い計装は既にリバート済み．現在のツリーに
残る計装は`[E]`（`ena_cbr`のSIO_RDY_SND時）と`{`（ISR先頭）の軽量
マーカーのみ）。

### 未検証・次の一手（最有力仮説）

`arch/riscv_gcc/esp32c6/chip_support.S`の設計コメントは「全割込み
ソースをlevel型で使用するためCLEAR操作は不要」と明記しているが，
**PLICMX_TYPEレジスタ（`PLICMX_BASE+0x004`）を明示的にlevel（0）へ
初期化しているコードが見当たらない**（`intmtx_initialize()`
＝`arch/riscv_gcc/esp32c6/chip_kernel_impl.c`を要再確認）。もしCPU線2の
TYPEがリセット既定でedge型になっていた場合：
- banner出力中に発生した大量のIN_EMPTY「立上りエッジ」は，ENAが0の
  間は（edge型でも）ラッチされずCPUには一切伝わらない可能性がある
  一方で，**EIPレジスタ自体がedge型でもいつまで「pending」を保持する
  実装か**（一度ラッチしたらCLEARするまで残るのか，該当エッジの
  タイミングでのみ一瞬だけ見えるのか）を実機で確認できていない。
- そもそも本当にlevel型のままなら，MIE=1＋EIP=pendingの状態で
  CPUがトラップを起こさない理由が別に必要になる（優先度関連の
  レジスタは全て確認済みで矛盾はない＝THRESH=1はTIMER/SIOとも
  優先度1超なら通過するはず．未確認なのはCPU線2に割り当てた
  実際のPRIORITYレジスタ値そのもの＝`PLICMX_PRI_BASEOFF+4*2`の
  実機読出し値．もしここが0のままなら，THRESH=1で線2は永久にブロック
  されている可能性がある＝**次に読むべき最有力候補**）。

次のセッションはここから再開すること：
1. `PLICMX_PRI_BASEOFF + 4*2`（線2の優先度レジスタ）の実機値を確認する
   （0であればTHRESH=1によって永久にブロックされている説が濃厚）。
   もし0なら，線2の優先度をCFG_INT登録時に明示的に1以上へ設定する
   コード（`irc_set_priority`相当の呼出し）が実機到達前に行われているか
   確認し，抜けていれば追加する。
2. `PLICMX_TYPE`（線2のedge/level設定）の実機読出し値を確認し，
   期待通りlevel（0）になっているか確認する。
3. 上記いずれも問題なければ，`esp32c6.h`のPLICMX_BASE
   （`0x20001000`）自体が実機で正しいアドレス空間にマップされているか
   （別バス／別クロックドメインでアクセス自体は成立するか）を疑う。
4. 診断計装は`ESP32C6_DIAG_EXC_DUMP`（`target/esp32c6_gcc/target.cmake`
   の`option()`）で有効化できる．診断コードを追加する際は，実際の
   メッセージ送信と同じUSB Serial/JTAGの1バイトFIFOを奪い合わない
   よう注意すること（本セッションで一度Heisenbugを作った教訓）。

### 続報（同セッション内）：TIMER（線1）も含めCPUに割込みが一切配送されない．PRI[3]／ENABLE bit3への書込みが実機で無視される

上記「次の一手」1〜3は本セッション内で実施済み。結果は当初の予想と異なる，
より根深いものだった。

**まず，`logtask_main`の`[M4]`直後に`target_hrt_handler`用のグローバル
カウンタ（`esp32c6_diag_hrt_count`，`target/esp32c6_gcc/target_timer.c`に
追加．現在も`ESP32C6_DIAG_EXC_DUMP`配下に残存）を仕込み，約2,000,000回の
空ループ（`[M4]`→`[M5]`間）を挟んで確認したところ，`hrt_count`は
**0のまま**だった（`PLICMX_EIP`も0のまま）。これは「TIMER割込みは
既に実証済み」という前回までの前提が誤りだったことを意味する
（カーネル起動直後の最初のタスク切替え＝logtaskへのディスパッチは
`sta_ker`からの直接呼出しで成立し，割込みを一切必要としないため，
本当に「1回もCPU割込みが配送されていない」状態のままここまで到達
できてしまう）。

**さらにタイミング非依存の決定的テスト**として，`sample1.c`の
`intno1_isr`（線3＝`INTNO1`＝`FROM_CPU_1`，`CFG_INT(INTNO1,
{TA_ENAINT, INTNO1_INTPRI=-2})`で自動有効化設定済み）にもカウンタ
（`esp32c6_diag_intno1_count`）を追加し，`logtask_main`から直接
`ESP32C6_INTPRI_CPU_INTR_FROM_CPU_1`（`0x600C5094`）へ`1`を書き込んで
**ソフトウェアから強制的に**線3の割込みを発生させた（`ras_int`同等の
生レジスタ操作．タイミング・周辺デバイスの状態に一切依存しない）。
結果：

```
[M6:00000000,00000000,00000000,00000001,00800000,00000003,00000006,00000003]
```

（フォーマット：`before,after,EIP,FROM_CPU_1読返し,INTMTX_STATUS0,
INTMTX map[src23],PLICMX_ENABLE,PLICMX_PRI[3]`）

読み解き：
- `FROM_CPU_1`レジスタ自体は書き込んだ`1`を正しく読み返せている
  （`0x00000001`）＝ソース側のトリガ自体は機能している。
- `INTMTX_STATUS0`のbit23（source23=FROM_CPU_1に対応）が**立っている**
  （`0x00800000`）＝ソースの生ステータスはINTMTXレベルで正しく
  アサートされている。
- `INTMTX`のsource23用MAPレジスタは`3`（線3への割当て）で正しい。
- しかし**`PLICMX_ENABLE`が`0x00000006`（bit1＋bit2のみ）＝bit3
  （線3）が有効になっていない**。`CFG_INT(INTNO1, {TA_ENAINT, ...})`
  により`_kernel_initialize_object()`→`_kernel_initialize_interrupt()`
  （`intmtx_config_int()`経由）で起動時に自動的に有効化されるはずが，
  実機では有効になっていない。
- **`PLICMX_PRI[3]`も`3`**（`INTPRI_TIMER`/`INTPRI_SIO`と同じ内部表現
  `INT_IPM(-2)=2`になるはずが，`3`になっている＝期待値と不一致）。
  比較のため同時に読んだ`PLICMX_PRI[1]`（TIMER）・`PLICMX_PRI[2]`
  （SIO）は共に期待通り`2`だった（`[M7:00000002,00000002,00000003]`
  ＝`PRI[1],PRI[2],tnum_cfg_intno`．`tnum_cfg_intno=3`で
  `_kernel_intinib_table`の全3件がループ対象になっていることも確認済み。
  生成された`kernel_cfg.c`の`_kernel_intinib_table`を直接確認したが，
  3件とも`INTPRI`引数は同じ`-2`であり，コード上は3件とも同じ
  `intmtx_config_int(intno, intatr, INT_IPM(-2)=2)`が呼ばれるはずで，
  線3だけ`3`になる理由はソースコードからは説明できない）。
- 当然，`intno1_isr`は**before/afterともに0のまま＝一度も呼ばれて
  いない**。

**さらに踏み込んで**，`logtask_main`から直接
`PLICMX_PRI[3]`に`2`を強制書込みし，`PLICMX_ENABLE`に`|= 0x8`を
強制実行した直後（同一箇所で間に他の命令を一切挟まない，同時刻の
即時読返し）でも：

```
[M6a:00000003,00000006]
```

**書き込んだはずの値（PRI[3]=2, ENABLEのbit3）が反映されず，元の値
（PRI[3]=3, ENABLEはbit3なし）のまま読み返された**。ディスアセンブル
（`riscv64-unknown-elf-objdump`）で該当の`sw`命令自体が正しいアドレス
（`PLICMX_BASE+0x1C`＝PRI[3]，`PLICMX_BASE+0x0`＝ENABLE）に対して
生成されていることは確認済み（コンパイラ側の問題ではない）。

**現状の解釈**：CPU割込み線3（`PLICMX`の`ENABLE`bit3・`PRI[3]`）への
書込みが実機で***恒常的に無視される***（直後の読返しですら反映され
ない）。線1（TIMER）・線2（SIO）は書込みが正しく反映される（値も
一致）。この非対称性から，以下のいずれかを疑う：

1. **線3特有のハードウェア制限／errata**：本実機（ESP32-C6FH4 rev
   v0.2）が量産前/初期ステッピングであるため，`PLIC_MX`の一部の線
   （特に3以降？）が未実装／別の目的に予約されている可能性。
   `esp-hal-3rdparty`のドキュメント／soc_caps.hに，実装済みCPU割込み
   線数の上限が明記されていないか要確認（`SOC_CPU_INTR_NUM`や類似の
   マクロを検索）。
2. **`PLIC_MX`ブロック自体，あるいはこの書込み経路が，実は`M`権限
   （machine mode）以外からのアクセスを要求する，または何らかの
   保護ビット（PMPなど）で書込みがサイレントに落ちている**：
   ただし線1・2は同じアドレス空間内で正常に書き込めているため，
   単純な全面ブロックではなく，線3（または線3以降）に限定した
   何らかの制限と考えられる。
3. **未検証**：線4，5等，線3以外の「未使用の」線でも同じ問題が
   起きるか（線3固有の問題か，あるいは「まだ一度もCFG_INT登録
   以外の方法で正しく初期化されていない全ての線」に共通する問題か）
   の切り分けができていない。

**次のセッションへの申し送り**：
- まず**線4または線5**（`INTNO1`を一時的に3から変更するのではなく，
  診断コード側で直接`PLICMX_BASE+0x10+4*4`等を触るだけで良い）に対して
  同じ強制書込み＋即時読返しテストを行い，「線3固有」か「線3以降
  すべて」かを切り分けること。
- `esp-hal-3rdparty`（`asp3_esp_idf/hal`）で，ESP32-C6のCPU割込み線の
  実装数上限（`SOC_CPU_INTR_NUM`相当）を確認し，本ポートが仮定している
  「31本（線1〜31）」全てが実際に存在するか裏を取ること。
- 上記のいずれもクリアなら，**「実は線1・2ですら，これまでCPUへの
  実配送は一度も証明されていない」**という前提に立ち返り，PLIC_MXの
  ENABLE／THRESH／PRIレジスタ操作だけでは実機で本当にCPU外部割込みを
  トリガできるのか，ESP-IDFの実機ログ／トレース，または
  `esp32c6_intmtx_route`以外に必要な初期化ステップ（例：
  `PLIC_MXINT_CONF_REG`＝`0x200013FC`，本セッションではsleep-retention
  専用と判断したが実は機能的な意味を持つかもしれない）が無いか，
  再度`esp-hal-3rdparty`の`riscv/vectors.S`・起動コード
  （`interrupt.c`ではなくCPU初期化そのもの）を確認すること。
- 診断計装（`ESP32C6_DIAG_EXC_DUMP`）は現在，`syssvc/logtask.c`
  （M1〜M7），`sample/sample1.c`（`cpuexc_handler`のダンプ・
  `intno1_isr`のカウンタ），`arch/riscv_gcc/esp32c6/esp32c6_usbjtag.c`
  （`[E]`・`{`マーカーとIN_EMPTYクリア），`target/esp32c6_gcc/
  target_timer.c`（`esp32c6_diag_hrt_count`）に分散している。いずれも
  `ESP32C6_DIAG_EXC_DUMP`未定義時は完全に無効化される（既定OFF）ため
  他ターゲット・通常ビルドへの影響はない。

## 解決（同セッション内）：真因は`mie` CSRが実機で一度も有効化されていなかったこと

上記の「線3固有の書込み拒否」自体は，より大きな謎（線1・2も含め
CPUへの割込み配送が一切成立しない）の**副次的な事象**であり，本筋
ではなかった。本筋は次の通り確定した。

### 決定的テスト：`mie` CSRへの実際のアクセス

`arch/riscv_gcc/esp32c6/chip_kernel_impl.h`は「ESP32-C3はmie/mip CSRを
実装せずアクセスすると不正命令例外になる」という前提から
`TOPPERS_OMIT_MIE_INIT`を定義し，共通部`start.S`でのmie/mipクリアを
抑止していた。この前提はC3からの類推であり，**C6実機で実際に検証
されたことがなかった**（milestone 1時点のコメントでも「未検証」と
明記されていた）。

`logtask_main`に一時的な診断計装を追加し，`csrr mie`を実機で直接
発行して確認したところ，**不正命令例外にはならず正常に読み出せ，
値は`0x00000000`（リセット直後は全ビット無効）だった**。この1点が
すべてを説明する：PLIC_MX側（ソースルーティング・ENABLE・PRI・
THRESH・EIP）をどれだけ正しく設定しても，標準RISC-VのCSRである
`mie`自体が全ビット0のままでは，CPUコアはそもそも外部割込みトラップ
を一切認識しない。これが「TIMER（線1）・SIO（線2）・線3（ソフト
ウェア強制トリガ）のいずれも実機でCPUへの割込み配送が一度も成立
しなかった」現象の真因であり，「線3のPRI／ENABLE書込みが無視される」
という奇妙な副次的観測（線3固有のerrataの可能性が高いが未追跡）とは
別の，より根本的な問題だった。

### 修正内容

- `arch/riscv_gcc/esp32c6/chip_kernel_impl.h`：`TOPPERS_OMIT_MIE_INIT`
  の`#define`を削除（C6では定義しない）。これにより共通部`start.S`の
  早期`mie`/`mip`クリアが有効化される（実害なし＝リセット直後の
  `mie`は既に0であることを確認済み）。
- `arch/riscv_gcc/esp32c6/chip_kernel_impl.c`：`chip_initialize()`で
  `csrw mie, ~0`を**QEMU限定から実機でも無条件に実行するよう変更**。
  従来はQEMUのみ想定した処置だったが，実機でこそ必要だった。

### 検証結果（実機，ESP32-C6FH4 rev v0.2，`/dev/ttyACM1`）

修正後，フルクリーンビルド（`rm -rf build/esp32c6 && cmake --preset
esp32c6 -B build/esp32c6 && cmake --build build/esp32c6`，診断計装は
すべて削除済み＝`git diff e57a7a6 -- <診断対象ファイル群>`が空である
ことを確認）で実機書込み・起動したところ：

```
System logging task is started on port 1.
Sample program starts (exinf = 0).
task1 is running (001).   |
task1 is running (002).   |
task1 is running (003).   |
no time event is processed in hrt interrupt.
（以下，"no time event is processed in hrt interrupt." が周期的に
　多数出力され，task1のカウントも進み続ける）
```

`logtask_main`のクラッシュ（"Sy"で停止）は完全に解消し，`sample1`の
並行タスクが正常に動作し，`"no time event is processed in hrt
interrupt."`（HRT割込みハンドラ内で処理すべき時間イベントが無い場合
に出力される，正常系のメッセージ）が周期的に出力され続けることから，
**SYSTIMER（HRT）割込みが実機で正しく・継続的に配送されていることを
確認した**。第2マイルストーンの記述にあった「実機でSYSTIMER割込みが
機能した」という記載は，本セッションの調査により**誤りだったことが
判明した**（実際には一度も配送されておらず，カーネル起動直後の
最初のタスク切替えが`sta_ker`からの直接ディスパッチで割込み非依存に
成立していたために，これまで問題が露見しなかった）。

### 未解決のまま残った副次的な謎（本筋の解決には無関係）

- 線3（`PLICMX_PRI[3]`・`PLICMX_ENABLE`のbit3）への書込みが，本修正
  （`mie`有効化）の**前**の実機診断では直後の読返しでも反映されない
  という現象を観測した（詳細は前節）。`mie`修正後にこの副次的な現象
  自体が再現するかは**未確認**（本筋の解決を優先し，診断計装は全て
  リバート済みのため）。もし今後，線3（`INTNO1`／`ras_int`用の
  テスト割込み）を実際に使う場面（`test/porting`等）で同様の問題が
  再発した場合は，本ドキュメントのこの節を参照し，線4・5等の別の
  線でも同じ現象が起きるか切り分けること。

### `test/porting`（6項目）実機結果：6/6 PASS

`mie`修正後，フルクリーンビルドで`test_porting`を実機（同一ボード，
`/dev/ttyACM1`）にビルド・書込み・実行した：

```bash
cmake --preset esp32c6 -B build/test_porting-esp32c6 \
  -DASP3_APPLDIR=test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=test/porting/tap.c \
  -DESP32C6_PORT=/dev/ttyACM1
cmake --build build/test_porting-esp32c6
esptool --chip esp32c6 --port /dev/ttyACM1 write-flash 0x0 \
  build/test_porting-esp32c6/asp_flash.bin
```

結果：

```
# test_porting: kernel porting verification
1..6
ok 1 - syslog_output
ok 2 - tick_timer_basic
ok 3 - task_create_activate
ok 4 - semaphore_signal_wait
ok 5 - eventflag_set_wait
ok 6 - alarm_handler
# 6/6 passed
```

**6/6 PASS**（`alarm_handler`＝タイマ割込み経路の項目も含む）。これで
ESP32-C3の時と同じ「Phase A完了」の基準を満たした。

### 残作業（`test/porting` 6/6 達成後）

1. **PCR経由のCPUクロックPLL切替（未着手．次セッションへ明示的に
   持ち越し）**。`asp3_esp_idf/hal/components/esp_hw_support/port/
   esp32c6/rtc_clk.c`（450行）・`rtc_clk_init.c`（124行）を確認した
   ところ，`rtc_clk_bbpll_configure()`はアナログBBPLLを`regi2c_write`
   系（I2C風の内部レジスタ経由）で校正・ロック確認する複雑な
   シーケンスであり，**単純なレジスタ2〜3本の書換えでは済まない**
   （`docs/dev/esp32c6-target.md`第1マイルストーンで「誤った
   レジスタ操作はハング等のリスクがあるため」と判断していたのは
   正しい）。本セッションでは着手しなかった（`mie`修正・
   `test_porting` 6/6達成を優先し，実機を壊すリスクのある新規の
   複雑な操作を拙速に試すべきではないと判断）。次セッションで
   着手する場合は，上記2ファイルを丁寧に移植し，各ステップごとに
   実機で確認しながら進めること（現状の`CORE_CLK_MHZ=40`固定・
   `SIL_DLY_TIM1/2=100`は変更不要で動作は継続する＝機能面のブロッカー
   ではなく精度面の課題）。
2. SYSTIMERのタイミング精度検証（`ESP32C6_SYSTIMER_TICKS_PER_US=16`
   の実測較正）は，PCRクロック切替と表裏一体（実クロックが確定しないと
   较正できない）のため，1とあわせて次セッションで実施するのが自然。
   割込み配送自体（本セッションで解決した本筋）と，周期の実測精度
   （このタスク）は独立した課題であることに注意。
3. LPコアとの相互作用の確認（未確認．本セッションでは対象外＝HPコア
   のみ）。

### まとめ（本セッション終了時点）

- **解決**：`logtask_main`クラッシュ（旧称「wild jump」）の真因は
  `mie` CSRが実機で一度も有効化されていなかったこと。修正は
  `arch/riscv_gcc/esp32c6/chip_kernel_impl.{c,h}`の2ファイルのみ
  （`TOPPERS_OMIT_MIE_INIT`削除＋`csrw mie,~0`を実機でも実行）。
- **検証**：`sample1`実機動作（task1〜3のループ・HRT割込みの継続的な
  発火を確認）／`test_porting` 6/6 PASS（実機）。
- **未着手（次セッションへの明示的な持ち越し）**：PCR経由のCPUクロック
  PLL切替（アナログBBPLL校正．リスクが高いため拙速に着手しなかった）・
  それに伴うSYSTIMER精度較正・LPコアとの相互作用確認。
- **副次的な未解決の謎（本筋とは無関係，優先度低）**：CPU割込み線3
  （`PLICMX_PRI[3]`・`ENABLE`のbit3）への書込みが`mie`修正前の診断で
  直後の読返しでも反映されなかった現象。`mie`修正後に再現するかは
  未確認。

## 解決（同日・別セッション）：PCRクロック切替とSYSTIMER較正

C3port同等の「Phase A完了」基準（160MHz実機動作・`SIL_DLY_TIM1/2`
較正済み・`dlynse`相当テストPASS）を満たすため，残っていたPCR
クロック切替とSYSTIMER精度較正を実施した。

### 重要な発見：PCRクロック切替の実装は不要だった

当初はESP-IDFの参照実装（`asp3_esp_idf/hal/components/esp_hw_support/
port/esp32c6/rtc_clk.c`ほか）を元に，analog BBPLLのregi2c較正
シーケンス（`regi2c_ctrl_ll_bbpll_calibration_start/stop/is_done`・
`clk_ll_bbpll_set_config`等，MODEM_LPCONのI2Cマスタクロック有効化＋
PMU ICGマップの事前設定を含む，見積りで450行超）を移植する前提で
調査を進めた。しかし，**この調査の途中で，実機の`PCR_SYSCLK_CONF`
（`0x60096110`）・`PCR_CPU_FREQ_CONF`（`0x60096118`）を安全な読出し
専用の診断（一時的に`software_init_hook`へ計装．`ESP32C6_DIAG_CLK`
ビルドオプション．すでにリバート済み）で確認したところ**：

```
[CLKDIAG] modem_lpcon_clk_conf=00000000 pcr_sysclk_conf=28010200
          pcr_cpu_freq_conf=00000000 i2c_ana_mst_conf2=00000000
```

- `pcr_sysclk_conf=0x28010200`：bit[17:16]=`SOC_CLK_SEL`＝**1（SPLL）**，
  bit[15:8]=`HS_DIV_NUM`＝2（clk_hprootはSPLLの÷3固定），
  bit[30:24]=`CLK_XTAL_FREQ`＝40（40MHz，想定通り）。
- `pcr_cpu_freq_conf=0x00000000`：`CPU_HS_DIV_NUM`＝0（clk_cpuは
  clk_hprootの÷1）。

すなわち，**ROMブートローダがDirect Boot到達前に既にSOC_CLK_SEL=SPLL・
480MHz÷3÷1＝160MHzへ設定済み**であることが判明した（C3のBBPLLが
`SPI_FAST_FLASH_BOOT`経路でROMにより既に有効化されているのと全く
同じパターン）。`modem_lpcon_clk_conf=0`（I2Cマスタクロック無効）
であることから，起動後にソフトウェアが独自にBBPLLを再較正した形跡は
なく，**ROM自身がBBPLLの電源投入・regi2c較正を済ませ，その出力を
PCR経由でCPUに供給する配線だけを行った状態でDirect Bootへジャンプ
している**と解釈できる。

この読出しだけでは「レジスタの値がそう見えるだけで実際のCPU動作
クロックは別」という可能性も残るため，**壁時計を用いた実測**で
二重に検証した：

1. 4,000万回の空ループ（`volatile`変数のインクリメント＋比較，
   コンパイラによる最適化除去なし．ディスアセンブルで確認）の
   壁時計時間をホスト側（pyserial，マーカー到達タイムスタンプ）で
   計測：**1.7098秒**→23.39M回/秒。ループ本体は`lw/addi/sw/lw/bgeu`
   の5命令（依存ロード×2を含む）で，160MHz説（約6.8サイクル/回）に
   整合し，40MHz説（1回あたり1.7サイクル未満を要求＝物理的に不可能）
   とは矛盾する。
2. `sil_dly_nse(1,000,000,000)`（1秒要求）を較正前の暫定値
   （TIM1=100,TIM2=100，40MHz仮定で設計された値）のまま実行し，
   実測184.6ms（壁時計）だったことからループ1回あたりの実コストを
   逆算：約18.38ns/回＝160MHzで約2.9〜3サイクル/回に相当（40MHzでは
   1サイクル未満になり物理的に不可能）。

以上2つの独立した実測により，**CPUは起動直後から一貫して160MHzで
動作している**ことを確定させた。したがって，**analog PLLの起動・
較正コードを新規に書く必要はなく**（すでに実施済みのものを流用する
だけで足りる），`hardware_init_hook()`は一切のPCRレジスタ書換えを
行わない（書き換えると，ROMが設定した既に正しい状態を壊すリスクが
あるだけで得るものがない）。これは，最初の見積りで「アナログPLL
較正はリスクが高い」と判断したことと矛盾しないが，**そのリスクの
高い操作自体が実は不要だった**という結論になる。

### `sil_dly_nse`の較正

上記の壁時計実測を用いて，`SIL_DLY_TIM1`／`SIL_DLY_TIM2`（`sil_dly_nse`
のループ較正定数．`arch/riscv_gcc/common/core_support.S`参照：
`a0 -= TIM1; if (a0>0) { do { a0 -= TIM2; } while(a0>0); }`という
実装）を反復的に実機較正した：

| 試行 | TIM1 | TIM2 | `sil_dly_nse(1e9)`実測（要求1000ms） | 誤差 |
|---|---|---|---|---|
| 較正前（暫定値） | 100 | 100 | 184.6 ms | −81.5% |
| 比例外挿（誤り） | 30 | 18 | 693.5 ms | −30.6% |
| 反復1 | 30 | 12 | 1038.5 ms | +3.9% |
| 反復2 | 30 | 13 | 962.5 ms | −3.8% |

比例外挿（TIM2を「1ループの実測ns」にそのまま丸めるだけの単純な
方法）が大きく外れた理由は，TIM2の即値が小さいほどRISC-V圧縮命令
（RVC）にエンコードされる等，命令列自体が変化しループの実行コスト
（命令フェッチ幅・整列等）に影響する余地があるためと考えられる
（詳細な命令レベルの原因分析は未実施）。反復的な実機較正で収束させ，
**TIM2=12を採用**（周辺機器ドライバのリトライ待ち用途であり，
過少より過多の方が安全なため，やや長め側の値を選んだ）。TIM1は
比例外挿値の30のまま採用（通常の呼出しではTIM2由来のループ時間が
支配的で影響が小さいため，厳密な単独較正は行っていない）。

最終値：`CORE_CLK_MHZ=160`・`SIL_DLY_TIM1=30`・`SIL_DLY_TIM2=12`
（`arch/riscv_gcc/esp32c6/esp32c6.h`）。

### SYSTIMER（HRT）の較正確認

SYSTIMERはCPU_CLKとは独立したクロックドメイン（esp-hal-3rdparty
`hal/esp32c6/include/hal/systimer_ll.h`の`systimer_ll_set_clock_source`
＝`PCR.systimer_func_clk_conf.systimer_func_clk_sel`でXTAL／RC_FASTを
選択．CPU_CLKの分周とは無関係）であることをヘッダで確認したうえで，
実機でSYSTIMERの生カウンタを壁時計と突き合わせて実測した（1億回の
空ループの前後でカウンタ差分を取得）：

```
SYSTIMER delta ticks = 70005446（壁時計4.3689秒に対応）
implied SYSTIMER ticks/us = 16.024
```

既存の`ESP32C6_SYSTIMER_TICKS_PER_US=16`と0.15%以内で一致＝
**変更不要，既に正しく較正されていたことを実機確認した**。

### `test/porting`（6/6）・`test_dlynse`実機再検証

上記較正後，フルクリーンビルドで再検証した：

```bash
# test_porting（6/6，前回セッションからの再検証）
rm -rf build/test_porting-esp32c6
cmake --preset esp32c6 -B build/test_porting-esp32c6 \
  -DASP3_APPLDIR=test/porting -DASP3_APPLNAME=test_porting \
  -DASP3_EXTRA_APP_C_FILES=test/porting/tap.c -DESP32C6_PORT=/dev/ttyACM1
cmake --build build/test_porting-esp32c6
# → # 6/6 passed（再確認）

# test_dlynse（sil_dly_nse較正の正式テスト．tecsgen.cfgの
# INCLUDE解決はtest_porting.cfg等と同じ仕組みで問題なく動作した）
cmake --preset esp32c6 -B build/test_dlynse-esp32c6 \
  -DASP3_APPLDIR=test -DASP3_APPLNAME=test_dlynse \
  -DASP3_EXTRA_APP_C_FILES="syssvc/test_svc.c;syssvc/histogram.c" \
  -DESP32C6_PORT=/dev/ttyACM1
cmake --build build/test_dlynse-esp32c6
```

`test_dlynse`実機結果（全17ケース中17ケースとも"OK"＝実測遅延が
要求値以上）：

```
sil_dly_nse(0): 43 OK
sil_dly_nse(30): 43 OK
sil_dly_nse(42): 81 OK
... （中略，全て OK）
sil_dly_nse(630): 693 OK
-- for checking boundary conditions --
sil_dly_nse(31): 81 OK
sil_dly_nse(43): 93 OK
sil_dly_nse(55): 106 OK
```

（この後，`check_finish(0)`→`test_finish()`→`ext_ker()`により
プログラムが静かに終了する．`count=0`のため"All check points
passed."メッセージは出力されない仕様＝これは正常終了であり
ハングではない．`syssvc/test_svc.c`の`check_finish`実装を確認済み）。

### まとめ（PCR/SYSTIMER較正セッション終了時点）

- **CPUクロック**：160MHz実機動作を確認（ROMが既に設定済み．
  ソフトウェアによる追加のPLL起動・regi2c較正コードは**不要かつ
  実施していない**）。
- **`SIL_DLY_TIM1/2`**：30／12に実機較正済み（`test_dlynse`全17ケース
  OK）。
- **SYSTIMER**：既存の`TICKS_PER_US=16`が実機実測（16.024）と0.15%
  以内で一致，較正済みと確認。
- **`test/porting`**：6/6 PASS（160MHz動作下で再確認）。
- これでC3ポートと同等の「Phase A完了」基準を満たした。
- **未着手のまま**：LPコアとの相互作用の確認（本セッションでも対象外
  ＝HPコアのみ）。線3の副次的な謎（前節）も引き続き未確認（`mie`
  修正後の再現有無は未検証）。

## Phase A「正式ターゲット化」（同日・別セッション）：target_spec.yaml・testexec・QEMU確認

C3ポートの`docs/porting/PORTING_GUIDE.md`に沿った新ターゲット移植プロセス
（`target_spec.yaml`の作成・`testexec`実機実行・QEMU対応可否確認・
`DIVERGENCE_MAP.md`／`docs/porting/IMPL_INDEX.md`更新）を実施した。

### `target_spec.yaml`

`target/esp32c6_gcc/target_spec.yaml`を新規作成した（`docs/porting/
target_spec.yaml.template`を元に，実機診断で確認済みの値のみ記入．
「実機未検証」等の推測混じりの記述はしていない）。なお，本リポジトリの
既存ターゲット（esp32c3含む）はいずれも`target_spec.yaml`を持っておらず
（`PORTING_GUIDE.md`の運用が本ファイル作成以降に確立されたため），
ESP32-C6が最初の事例となる。

### QEMU対応確認：Espressif版QEMUはesp32c6マシンを実装していない

第1マイルストーンの調査時点で「C6用QEMUマシンの有無は未確認」として
残っていた項目を確認した。本リポジトリにピン留め済みのEspressif版
QEMU（`~/TOPPERS/ASP3CORE/tools/qemu-esp/qemu/bin/qemu-system-riscv32`，
バージョン`9.2.2（esp_develop_9.2.2_20260417）`）で確認したところ：

```
$ qemu-system-riscv32 -M help
Supported machines are:
esp32c3              Espressif ESP32-C3 machine
none                 empty machine
opentitan            RISC-V Board compatible with OpenTitan
sifive_e             RISC-V Board compatible with SiFive E SDK
sifive_u             RISC-V Board compatible with SiFive U SDK
spike                RISC-V Spike board (default)
virt                 RISC-V VirtIO board
```

**`esp32c6`マシンは存在しない**（`esp32c3`のみ）。実際にビルド済み
イメージで起動を試みても明確なエラーになる：

```
$ qemu-system-riscv32 -M esp32c6 -nographic -drive file=asp_flash.bin,if=mtd,format=raw -semihosting
qemu-system-riscv32: unsupported machine type: "esp32c6"
Use -machine help to list supported machines
```

（ビルド自体は`esp32c6-qemu`プリセットで問題なく成功する＝コード側の
問題ではなく，QEMU側にesp32c6マシンの実装が存在しないことが原因）。

**結論**：ESP32-C6は現状のツールチェーン（ピン留め済みEspressif版
QEMU）では**QEMUでの動作確認・CI回帰が不可能＝実機専用ターゲット**
である。したがって：
- `test/porting`のQEMU実行は不可（実機のみで6/6 PASSを確認済み）。
- `.github/workflows/ci.yml`への`esp32c6-qemu`ジョブ追加は**見送った**
  （動かせない環境でジョブを追加する意味がないため）。将来Espressif
  がQEMUにesp32c6マシンを追加した場合は，`esp32c3-qemu`ジョブを
  雛形に追加を検討すること。

### `testexec`実機結果：35/36 PASS（`int1`が唯一の失敗＝未解決issue）

実機テストランナ`scripts/ci/run_board_esp32c6.py`を新規作成した
（`run_board_esp32c3.py`と完全に同一構造，chip名のみ差替え）。
C3の実機Phase A完了時と同じ36件のテストセット（`cpuexc1`〜`10`・
`dlynse`・`dtq1`・`exttsk`・`flg1`・`hrt1`・`int1`・`mpf1`・`mutex1`〜
`8`・`notify1`・`pdq1`・`raster1`〜`2`・`sem1`〜`2`・`suspend1`・
`sysman1`・`sysstat1`・`task1`・`tmevt1`）を実機（`/dev/ttyACM1`）で
実行した：

```bash
ESP32C6_TTY=/dev/ttyACM1 ESPTOOL=<esptoolのパス> \
  python3 scripts/ci/run_testexec.py \
  --options "--preset esp32c6" \
  --run "ESP32C6_TTY=/dev/ttyACM1 ESPTOOL=<esptoolのパス> python3 $(pwd)/scripts/ci/run_board_esp32c6.py 90" \
  --workdir build/testexec-esp32c6 \
  cpuexc1 cpuexc2 ... task1 tmevt1
```

結果：**35/36 PASS**（`cpuexc10`は「対象外」でSKIP扱いPASS，`dlynse`
含む）。**唯一の失敗＝`int1`**：

```
Check point 1 passed.
## Unexpected check point 4.
```

#### `int1`失敗の原因調査（新規の実機固有issue．未解決のまま記録）

`test_int1.c`は，`task1`が`ras_int(INTNO1)`で割込みを要求した直後に
`ISR2`→`ISR1`が実行されチェックポイント2,3を経てから`task1`が
チェックポイント4へ進むことを期待する。実機ではチェックポイント1の
直後にいきなりチェックポイント4に到達しており，**`ras_int(INTNO1)`
はE_OKを返すものの，対応するISR（`isr1`/`isr2`）が一度も呼ばれて
いない＝ソフトウェア割込み要求がCPUへ実際に配送されていない**。

本ポートの`INTNO1`は，前々節「線3固有の書込み拒否」で報告した
CPU割込み線3（`FROM_CPU_1`ソース）に割り当てている。この失敗が
「線3固有」の問題なのか，より一般的な問題なのかを切り分けるため，
`target_test.h`／`target_kernel_impl.c`を一時的に書き換えて以下を
実機で確認した（いずれも実験後は元の設定＝線3・`FROM_CPU_1`に
リバート済み。`git diff HEAD`で確認済み）：

| 実験 | ソース | 宛先CPU割込み線 | 結果 |
|---|---|---|---|
| 元の設定 | `FROM_CPU_1` | 線3 | **失敗**（チェックポイント1→4） |
| 実験1 | `FROM_CPU_1` | 線4 | **失敗**（同上，線を変えても再現） |
| 実験2 | `FROM_CPU_2` | 線3 | **失敗**（同上，ソースを変えても再現） |

すなわち，**特定の線・特定のFROM_CPUソースの組合せに固有の問題では
なく，「CFG_INTで3番目に登録される割込み要求ライン」が一貫して
実機で配送されないという，より一般的な問題**であることを確認した
（前々節で報告した「PLICMX_PRI[3]・ENABLEのbit3への直接強制書込みが
即座の読返しでも反映されない」という観察と整合する＝ソフトウェアの
書換えでは解決しない，ハードウェア側の制限である可能性が高い）。

**新たに判明した重要なリスク**：`target_timer.h`の
`target_hrt_set_event()`は，SYSTIMERコンパレータの設定完了時点で
既に目標時刻を過ぎていた場合のみ，`target_timer_force_int()`
（`FROM_CPU_0`を線1へ多重アサートするフォールバック経路）を使う。
これは`SYSTIMER_TARGET0`（線1の主ソースであり，これまでの全ての
成功していたテストで実際に使われてきた経路）とは**別の，これまで
一度も実機で検証できていない経路**である。上記の調査により
「CFG_INT登録順3番目以降」に問題があることが分かったが，
`FROM_CPU_0`は線1のCFG_INT登録の一部（1番目のTIMERエントリに
紐づく）であるため直接は影響を受けないと推測されるものの，
**`FROM_CPU_x`系のソフトウェア割込み要求機構全般に何らかの共通の
問題がある可能性も排除できていない**。今後，`target_hrt_set_event`
のこの分岐で実際にハングやタイムイベント消失が疑われた場合は，
真っ先にこのissueを疑うこと。

**次のセッションへの申し送り**：
1. CFG_INTの登録順（`intinib_table`のインデックス）と実機での
   配送成否の対応関係をさらに検証する（例：`test_int1.cfg`の
   CFG_INT登録順を変更し，INTNO1を1番目または2番目にした場合に
   配送されるかを確認すれば，「インデックス依存」説と「線固有」説を
   完全に切り分けられる）。
2. `PLIC_MXINT_CONF_REG`（`0x200013FC`．前々節でsleep-retention専用と
   判断したが未確定）に加え，esp-hal-3rdpartyの`riscv/interrupt_plic.c`
   がCFG_INT登録数が3件以上になる場合に何か特別な初期化を行って
   いないか再確認する。
3. 上記1・2で原因が特定できない場合は，実機JTAG（本ドキュメント
   前半に記載のCPUTAPID修正・halt-after-crash手法）でPLICMX関連
   レジスタの全ビットを`monitor halt`後に読み出し，ソフトウェアからの
   書込みでは見えない実機固有の挙動がないか確認する。

### ドキュメント整備

- `DIVERGENCE_MAP.md`：`arch/riscv_gcc/esp32c6/`・`target/esp32c6_gcc/`
  のエントリを追加（esp32c3のエントリに準拠．QEMU未対応・実機
  160MHz・test_porting 6/6・testexec 35/36を明記）。
- `docs/porting/IMPL_INDEX.md`：ESP32-C6の割込みマトリクス制御
  （int1 issueの注記付き）・Direct Boot起動・SYSTIMER HRT・
  USB Serial/JTAGコンソール・実機テストランナの各行を追加
  （esp32c3の対応行に準拠）。
- `docs/dev/README.md`：索引表のESP32-C6行を「Phase A完了」に更新
  （test_porting 6/6・testexec 35/36・int1 issue・QEMU未対応の要旨）。

### まとめ（本セッション終了時点）

- **完了**：`target_spec.yaml`作成・`DIVERGENCE_MAP.md`／
  `IMPL_INDEX.md`更新・実機テストランナ（`run_board_esp32c6.py`）
  新規作成・testexec実機実行（35/36 PASS）。
- **確定した事実**：Espressif版QEMU（本リポジトリピン留め版）は
  esp32c6マシンを実装していない＝実機専用ターゲット。CIジョブ追加は
  見送り（正しい判断＝動かない環境にジョブを追加しない）。
- **新規に発見・記録した未解決issue**：CFG_INTの3エントリ目以降が
  実機で配送されない（`int1`テスト失敗）。線・ソースの組合せを
  変えても再現する一般的な問題であることを確認済み。ハードウェア
  側の制限の可能性が高いが未確定。`target_timer_force_int()`
  （SYSTIMERの稀なフォールバック経路）も同じ問題の影響を受ける
  可能性があり未検証＝将来のハング調査で真っ先に疑うべき候補として
  記録した。
- 上記issueを除き，C3ポートと同等の「Phase A完了」水準
  （起動・クロック・割込み・タイミングの中核機能はすべて実機検証
  済み）に到達した。
