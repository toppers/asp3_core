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
