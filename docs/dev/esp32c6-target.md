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

### 残作業（次のマイルストーン）

- **CLIC割込みコントローラの実装**（最大の残課題．ソフトウェア
  ディスパッチ方式かハードウェアベクタ方式かはユーザーと設計判断が
  必要＝着手前に確認すること）。
- PCR経由のCPUクロックPLL切替（現状はリセット既定クロックのまま．
  周波数未計測・dlynse較正未実施）。
- SYSTIMER（HRT）の実機検証。
- CLIC実装後，ASP3カーネル本体（cfg生成・chip_kernel_impl.c・
  target_kernel_impl.c・target_timer.c・target.cmake等）を本格的に
  統合し，test_porting（6項目）での動作確認まで進める。
- LPコアとの相互作用の確認（本マイルストーンはHPコアのみ対象．
  未確認）。
