# NXP MCUXpresso SDK統合（i.MX RT685）

## 項目

NXP MCUXpresso SDK統合（AGENTS.md §1 機能追加計画、優先度：中。第一目的「各社SDKとの協調動作」の**第4弾**＝Pico SDK・FSP・STM32に続く）

## 内容

NXP MCUXpresso SDK と TOPPERS/ASP3 を協調動作させる。対象は
**EVK-MIMXRT685**（i.MX RT685＝Cortex-M33＋HiFi4 DSP。ASP3はCM33のみ使用）。
確立済みのA案（**外側リポジトリで SDK固有部を管理＋asp3_core を submodule＋
`ASP3_TARGET_DIR` で受け入れ**）を踏襲する。

## 現状（2026-06-12 調査）

### 既存移植：genuine ASP3 の MIMXRT685-EVK 移植が手元にある

`/home/honda/TOPPERS/FMP3/posix/asp3_3.7/`（TTSP3/back にも同系）：
`target/mimxrt685evk_gcc` ＋ `arch/arm_m_gcc/imxrt600`（target_user.txt付き・
実機ROM実行の動作実績・gcc 11.3）。ただし：

- **MCUXpresso SDK 未使用**＝ベアメタル自己完結（自前 `IMXRT600.h`・
  `flash_config.c`＝FlexSPI設定ブロック・専用ld・`start_imxrt600.S`・tUsart）。
  MCUXpresso IDE（SDK 2.11.0）はGUIデバッグ用の任意利用にとどまる。
- **旧世代ASP3（3.7.0）対応**＝TECS（tUsart.cdl・target.cdl）＋Ruby cfg（.trb）。
  **現 asp3_core（3.7.2・非TECS・Python cfg）ではそのまま使えない**。

→ 位置づけ：**直接流用不可。非TECS＋Python cfg＋3.7.2規約への変換ベース／
参照資料**（レジスタ定義・flash_config・ld・タイマ/UART実体・割込み番号・
実機手順）。同種変換は stm32mp2・asp3_stm32cube で確立済み（3例目）。

### 他SDK統合との比較（楽な点・特有の点）

| 観点 | NXP/MCUXpresso |
|---|---|
| ツールチェーン | **arm-none-eabi gcc（導入済・既存と同じ）**。SDKもarmgcc公式サポート |
| SDK取得 | **git/zipで取得可**（GitHub: nxp-mcuxpresso）。CubeMX/RASCのような**GUI生成必須ではない**（Config Toolsは任意）→ 生成依存は4SDK中最軽量の見込み |
| ボード特有 | **Flash XIP実行**＝FlexSPI設定ブロック（flash_config）必須・TrustZone（CM33）・HiFi4 DSPは不使用（停止のまま） |
| 実機書込み | オンボードMCU-Link（CMSIS-DAP/J-Link化可）。既存移植のuser.txt手順を踏襲 |

## 実施プラン（2段階）

### Phase A：ベアメタルターゲットを asp3_core 本体に追加
既存移植を変換して `arch/arm_m_gcc/imxrt600`＋`target/mimxrt685evk_gcc` を追加：

1. TECS→非TECS変換（tUsart→`chip_serial.[ch]`系・syssvcは既存非TECS版）・
   Ruby cfg→Python（.trb→.py）・3.7.0→3.7.2規約（TMAX_*）への更新
2. CMake化（target.cmake/chip.cmake・presets.json＝命名規則どおり `mimxrt685evk`）
3. **SDK生成依存がないため CI ビルドジョブ追加可**（CubeMX/RASCと違う利点）
4. 検証：ビルド（CI）→ 実機で test_porting 6/6 → sample1 → testexec → dlynse較正
5. OS Awareness（arm_m共通NVIC層が既存＝target層のみ）・osdebug

### Phase B：MCUXpresso SDK統合（外側リポジトリ）
0. リポジトリ名：**`asp3_mcuxsdk`** に決定（NXP公式SDKリポジトリ名 mcuxsdk に一致）。
   **作成済み（2026-06-12）**：https://github.com/toppers/asp3_mcuxsdk
   ＝骨格（README・asp3_core submodule・予定構成）を初期化済み。
   ローカル：`../asp3_mcuxsdk`
1. 外側リポジトリ：asp3_core submodule＋
   `ASP3_TARGET_DIR`＋`ASP3_LIBRARY_ONLY`（確立済みの受け入れ口）
2. SDKのfsl_ドライバ・スタートアップとの調停：割込み登録の主導権
   （picoの`--wrap`方式 or FSP/STM32の「ASP3がNVIC掌握・SDKは素直に従う」方式の
   どちらが要るかをSDKのIRQ APIで判断）・SysTick/OSタイマの競合
3. SDKバージョンは**現行（最新）に合わせる**（STM32と同方針）
4. 実機検証＋移植skill（porting-asp3-to-nxp 等）を外側リポジトリに

#### Phase B 詳細プラン（2026-06-12 着手時点・調査済み事項を反映）

**SDK取得（調査結果）**：現行SDKは west マニフェスト方式（`mcuxsdk-manifests`
release/26.03.00）。west は使わず、マニフェストが指す**最小3リポジトリを
submodule 直参照**する（CI でもそのまま使える＝GUI生成依存なしの利点を実現）：

| submodule | 実体 | サイズ | 用途 |
|---|---|---|---|
| `sdk/core` | mcuxsdk-core | 46MB | 共通fslドライバ（flexcomm/usart・ctimer・common 等） |
| `sdk/devices-rt` | mcux-devices-rt | 13MB | `RT600/MIMXRT685S/`（ヘッダ・system・gcc startup/ld・fsl_clock/power/reset） |
| `sdk/cmsis` | mcu-sdk-cmsis | 14MB | CMSIS Core ヘッダ |

examples リポジトリ（200MB）は submodule にせず、必要なボードファイル
（`_boards/evkmimxrt685/` の board.c/h・clock_config・pin_mux・flash_config＝BSD-3）
をボードプロジェクトへ**コピー**する（CubeMX生成コードのコミットと同じ扱い）。

**リポジトリ構成（asp3_stm32cube と同型）**：

```
asp3_mcuxsdk/
├── asp3/
│   ├── asp3_core                    … submodule（既存）
│   ├── asp3_mcuxsdk.cmake           … glue（ASP3_CORE_DIR/ROOT_DIR/TARGET_DIR解決）
│   └── target/evkmimxrt685_mcuxsdk/ … SDK版ターゲット依存部（チップ層相当も同居）
├── sdk/                             … 上記3 submodule
├── evkmimxrt685/                    … ボードプロジェクト（CMakeLists・main.c・boardファイル）
└── docs/
```

**技術方針（調査で確定）**：

- **ブート**：SDK標準（`startup_MIMXRT685S_cm33.S`→`SystemInit`→`main()`）。
  `main()` が `BOARD_InitPins`/`BOARD_BootClockRUN` 後に `sta_ker()` を呼ぶ。
  リンカスクリプトも SDK の `MIMXRT685Sxxxx_cm33_flash.ld`。flash_config（FCFB）も
  SDK 版＝Phase A の自前ブート部材（start_imxrt600.S・mimxrt685.ld・自前flash_config）
  は使わない
- **ベクタ/割込み**：arm_m 共通部の `core_initialize()` が VTOR を ASP3 cfg 生成
  テーブルへ切り替える＝**ASP3がNVIC掌握・SDKは素直に従う**（STM32/FSPと同方式。
  pico の `--wrap` は不要と判断）。SDKドライバの割込みは cfg（`ATT_ISR`/`DEF_INH`）
  で登録し、ISR から fsl のハンドラ関数を呼ぶ（STM32 の `HAL_UART_IRQHandler` 呼び
  出しと同型）
- **クロック**：SDK既定 `BOARD_BootClockRUN` をそのまま使う（main_pll=528×18/19=
  500.21MHz・**CPU=250.105MHz**・frg_pll=main_pll/12=41.68MHz）。Phase A の
  FBB/PMIC/300MHz 設定は使わない（250MHzはPMIC既定で動作＝SDK examples実績）
- **HRTタイマ**：CTIMER0＋fsl_ctimer。クロックは main_clk をプリスケーラ500分周
  →**1.000421MHz**（+421ppm。整数分周で正確に1MHzにできないため許容し文書化）
- **シリアル**：FC0 USART＋fsl_usart（STM32の HAL_UART_*_IT 相当の transactional
  API＋コールバックで SIO 層を実装）
- **SysTick**：ASP3は使わない（USE_TIM_AS_HRT）。SDK側も基本未使用＝競合なし
- ターゲット依存部は外側リポジトリで自己完結させる（Phase A の
  `arch/arm_m_gcc/imxrt600` への参照はしない。必要な定義はコピー）

**検証計画**：ビルド → 実機 sample1 → test_porting 6/6 → testexec（代表テスト）→
記録（verification.md・本ファイル実施結果・AGENTS §14・README）。
移植skill（porting-asp3-to-nxp）は実機検証完了後に作成。

> Phase A が完成すればハード知識（XIP・クロック・タイマ・UART）が
> 3.7.2/CMake環境で検証済みになり、Phase B はSDK協調の差分に集中できる
> （pico＝ベアメタル先行→SDK統合、と同じ段取り）。

## 必要な環境

| 要素 | 状況 |
|---|---|
| arm-none-eabi-gcc | 導入済 |
| **EVK-MIMXRT685 実機** | **保有（2026-06-12確認）** |
| MCUXpresso SDK | Phase B。git/zip・現行版に合わせる |
| MCUXpresso IDE / VS Code拡張 | 任意（デバッグGUI） |

## スコープ外

- HiFi4 DSP（マルチコア・別OS連携）
- RT685以外のNXPボード（LPC55S69等＝既存移植はあるが別項目）
- QEMU対応（RT685のQEMUモデルは想定しない）

## リスク・確認事項

- **XIP（Flash実行）**：flash_config（FlexSPI設定）・キャッシュ・XIP中のフラッシュ
  書換え不可制約。既存移植が動作実績を持つ構成を踏襲する
- TrustZone（CM33）：既存移植のSecure設定を踏襲（pico2_arm/mps2 と同様の論点）
- 3.7.0→3.7.2 の規約差（TMAX_*・割込み管理）＝stm32mp2変換で確立した変換規則を適用
- Phase B のSDK startup（ResetISR・SystemInit）と ASP3 ブートの調停

## 実施結果

### Phase A：ベアメタルターゲット追加（2026-06-12 実装・ビルド検証済）

`/home/honda/TOPPERS/FMP3/posix/asp3_3.7/` の genuine ASP3 3.7.0 移植
（`target/mimxrt685evk_gcc`＋`arch/arm_m_gcc/imxrt600`・TECS＋Ruby cfg）を
非TECS＋Python cfg＋CMake構成へ変換し，asp3_core本体に追加した。
ブランチ：`feat/mimxrt685evk`。

#### 追加したファイル

**`arch/arm_m_gcc/imxrt600/`（チップ依存部・18ファイル）**

| ファイル | 由来・変更内容 |
|---|---|
| `IMXRT600.h` | 3.7.0版ほぼそのまま（`FIFOSTAT_TXEMPTY` ビット定義のみ追加） |
| `start_imxrt600.S` | 3.7.0版そのまま（セクションテーブル駆動のDATA/BSS初期化＝XIP用） |
| `imxrt600_usart.[ch]` | **新規**：`tUsart.c`（TECS）のレジスタ操作ロジックを rp2350_uart.[ch] と同構造の非TECSドライバへ変換。クローズ時の送信FIFOドレイン待ちを追加（pico2と同趣旨） |
| `chip_serial.{c,h,cfg}` | **新規**：非TECS SIO 中継層（rp2350の同名ファイルと同形）。`target_fput_log` 含む |
| `chip_kernel.h`・`chip_sil.h`（TBITW_IPRI=3）・`chip_stddef.h`・`chip_cfg1_out.h` | 3.7.0版そのまま |
| `chip_kernel_impl.h`・`chip_syssvc.h` | 3.7.0版から旧logtrace includeを削除（asp3_core方式）。`TNUM_PORT` 4→1 |
| `chip_rename.def`／`chip_rename.h`・`chip_unrename.h` | `INCLUDE "core"` 形式に変更し `utils/genrename.py` で再生成 |
| `chip_os_awareness.py` | **新規**：core層（NVIC）APIの再エクスポート |
| `chip.cmake` | **新規**：Makefile.chipのCMake版。`arch.cmake` include後に `start.S`→`start_imxrt600.S` を差し替え |

**`target/mimxrt685evk_gcc/`（ターゲット依存部・29ファイル）**

| ファイル | 由来・変更内容 |
|---|---|
| `flash_config.[ch]` | 3.7.0版そのまま（NXP BSD-3-Clause・FlexSPI設定ブロック＝`.flash_conf`） |
| `mimxrt685.ld`・`mimxrt685-evk.h`・`target_asm.inc`・`target_sil.h`・`target_stddef.h`・`target_kernel.h`・`target_kernel.cfg`・`target_kernel_impl.[ch]`・`target_cfg1_out.h`・`target_timer.{c,h,cfg}` | 3.7.0版そのまま（CTimer0 HRT・FBB/PMIC/PLL初期化・`raise_int` は3.7.2にも存在） |
| `target_kernel.py`・`target_check.py` | **`.trb`→`.py`変換**：`GenResVectVal`（ベクタ9＝イメージタイプ。プレーンイメージ=bit14）をPython関数化 |
| `target_syssvc.h` | 非TECS用に書き換え：`SIO_USART_BASE`/`SIO_FLEXCOMM_BASE`/`SIO_USART_INDEX`（FC0）・`INTNO_SIO` 等を追加 |
| `target_serial.{h,cfg}` | **新規**（chip層への中継） |
| `target_test.h` | `INTNO1`（CTIMER1 IRQ11＝クロック未供給の空きIRQ・`intno1_clear()`空）を追加 |
| `target_rename.def`／`target_rename.h`・`target_unrename.h` | `INCLUDE "chip"` 形式で再生成 |
| `target_os_awareness.py` | **新規**：chip層APIの再エクスポート |
| `target.cmake`・`presets.json`（プリセット名 `mimxrt685evk`） | **新規**：FPU_LAZYSTACKING相当のdefs・`-Wl,--undefined=flexspi_config`・**`TOPPERS_ENABLE_TRUSTZONE`は未定義**（プレーンイメージ起動＝EXC_RETURN 0xFFFFFFBC の実績構成。stm32h5xxと同論点） |
| `target_user.md` | `target_user.txt`（3.7.0）をMarkdown化・CMake手順に更新（JP22注意・メモリマップ・XIP解説含む） |

**その他**：ルート`CMakePresets.json`にinclude追記。削除ファイルなし
（TECS関連の `tUsart.{c,cdl}`・`target.cdl`・`tSIOPortTarget*`・`.trb`・
`Makefile.*`・`MCUXpresso/` は持ち込まない）。

#### Git情報

- ベースコミット：`96b8be5`
- 関連コミット：`feat/mimxrt685evk` ブランチの `feat(target): add mimxrt685evk` 以降
- ファイルリスト再現：`git diff --stat main feat/mimxrt685evk -- arch/arm_m_gcc/imxrt600 target/mimxrt685evk_gcc`

#### 検証結果

| テスト | 実施 | 結果 |
|---|---|---|
| ビルド sample1（`--preset mimxrt685evk`） | ○ | 警告ゼロ・リンク成功（FLASH 27KB/SRAM 12KB） |
| ビルド test_porting（+`tap.c`） | ○ | 警告ゼロ・リンク成功 |
| ビルド test_int1（+`test_svc.c`） | ○ | 警告ゼロ・リンク成功 |
| ELF構造検査 | ○ | FCFBタグ@0x08000400・ベクタテーブル@0x08001000・ベクタ9=0x4000（プレーンイメージ）を確認 |
| 実機 sample1 | ○ | バナー・task1周期実行・`r`（rot_rdq）でtask1→2→3切替を確認 |
| 実機 test_porting | ○ | **6/6 passed** |
| 実機 testexec（機能テスト35件） | ○ | **33/35 PASS**（cpuexc10=SKIP．cpuexc1/4は arm_m 共通の既知FAIL＝上流由来，`issue-cpuexc-armm.md`．mps2/pico2_armと同一症状を本ターゲットでも確認） |
| 実機 dlynse較正 | ○ | `SIL_DLY_TIM1/2`＝**23/16** で全項目OK（NG=0）．3.7.0実績値79/50は現gcc 13.2では過大（ループ実測16.7ns＝5cyc@300MHz・呼出実測23ns＝7cyc．pico2_armの46/33@150MHzと同一サイクル数） |
| 実機 OS Awareness | ○ | `osdebug`（J-Link GDB Server＋gdb-multiarch）で atask/stask/sem/cyc/intr（NVIC ena/pend実読出し）を確認 |

QEMUにRT685モデルは無いため，POSIX/QEMU回帰は本ターゲットには適用外
（既存ターゲットへの影響なし＝既存ファイルの変更はCMakePresets.jsonのinclude追記のみ）。

#### DIVERGENCE_MAP との関連

`target/mimxrt685evk_gcc/`・`arch/arm_m_gcc/imxrt600/` のNEW行を追加
（PRISTINE領域への変更なし）。

#### 実機検証（2026-06-12 実施・完了）

上記検証結果表の実機行はすべて完了。確定した手順は `target_user.md` の
「実行方法（実機）」に反映済み。要点と追加実装：

- **書込み構成**：オンボードプローブ（LPC-Link2）は J-Link ファームウェア化済みの
  個体で，ホスト側に SEGGER J-Link Software V9.30 を導入（MCUXpresso Installer の
  deb を `sudo dpkg -i`＋`udevadm trigger`）。LinkServer/MCUXpresso IDE は未使用。
  書込みは `JLinkExe -device MIMXRT685S_M33`＋コマンドファイル（loadfile/r/g）。
- **追加実装**：`scripts/ci/run_board_mimxrt685evk.sh`（testexec用ボードランナ＝
  UARTキャプチャ→J-Link書込み→完走マーカ待ち。pico2版が雛形）・
  `target/mimxrt685evk_gcc/run.cmake`＋`jlink-debug.gdb`（jlink-run／jlinkgdbserver／
  jlink-debug／osdebug／console の CMakeターゲット。stm32/pico2 と同体験）
- **較正**：`SIL_DLY_TIM1/2`=79/50→**23/16**（`mimxrt685-evk.h`。検証結果表参照）
- **教訓**：VCOM（/dev/ttyACM0）を読むプロセスが複数あるとバイトを奪い合い
  「無出力・文字化け」に見える（test_porting 検証時に残留 `cat` で原因究明に難航。
  `fuser -v /dev/ttyACM0` で確認）。target_user.md に注意書きを追加

### Phase B：MCUXpresso SDK統合（2026-06-12 実装・実機検証済）

詳細プラン（上記）どおり `asp3_mcuxsdk` リポジトリに実装した。
構成・ビルド/実行手順・検証状況は asp3_mcuxsdk の README.md を参照。

#### 実装内容（asp3_mcuxsdk リポジトリ側）

- **SDK submodule（4本・release/26.03.00 のマニフェストが指すSHAでピン留め）**：
  `sdk/core`（mcuxsdk-core）・`sdk/devices-rt`（mcux-devices-rt）・
  `sdk/cmsis`（mcu-sdk-cmsis）・`sdk/components`（mcux-component。
  プランの3本に加え，board.c が参照する fsl_debug_console.h 用に追加）
- **ASP3受け入れ層**：`asp3/asp3_mcuxsdk.cmake`（glue）＋
  `asp3/arch/arm_m_gcc/imxrt600_mcuxsdk/`（チップ層）＋
  `asp3/target/evkmimxrt685_mcuxsdk/`（ターゲット層）。
  プランでは「ターゲット層に同居」としたが，stm32cube と同じ
  **チップ層／ターゲット層の2層構成**に変更（3例目の同型変換として一貫性優先）
- **SIO**＝`target_serial.c`（fsl_usart・FIFO割込み．構造はPhase Aの
  imxrt600_usart.cと同形でレジスタ操作をfsl APIに置換）／
  **HRT**＝`target_timer.{c,h}`（fsl_ctimer で初期化，高頻度パスは
  CMSISレジスタ直アクセス＝STM32のLLマクロ使用と同位置付け）
- **ボードプロジェクト** `evkmimxrt685/sample1/`（asp3_fsp の `ek_ra6m5/sample/` と同型のボード/アプリ2階層）：SDK examples のボードファイル
  （board.c/clock_config/pin_mux/flash_config＝BSD-3）をコピー，
  main.c＝`BOARD_InitBootPins/Clocks`→`sta_ker()`．SDK ソースは
  必要な fsl ドライバのみ直接コンパイル（SDKのKconfig/CMakeビルドシステムは不使用）

#### プランからの差分・判明事項（重要）

1. **TOPPERS_ENABLE_TRUSTZONE を定義する**：SDK startup（ベクタ9＝イメージ
   タイプ0）でブートした CPU は **Secure 状態**で実行される．未定義
   （EXC_RETURN 0xFFFFFFBC）だと最初のディスパッチで **INVPC（UsageFault，
   CFSR=0x40000）**．mps2_an521 と同じ定義ありで解決．
   （注：2026-06-16 に QEMU M33 ターゲットを mps2-an505/IoTKit へ置換〔現ターゲット名 mps2_an505_gcc〕。本記述は置換前の調査時点の記録。）
   **Phase A（プレーンイメージ＝ベクタ9 bit14）とは逆**：ブートイメージ
   タイプで EXC_RETURN 構成が決まる（重要な移植知見）
2. **ASP3ベクタテーブルの配置**：cfg生成の `.vector` は SDK の ld では
   orphan となり `m_interrupts`（0x130＝SDKブートベクタでちょうど満杯）に
   入りきらずリンクエラー．ld をボードプロジェクトにコピーし
   `.kernel_vector`（ALIGN(512)→m_text）を追加して解決
3. **DbgConsole_Init スタブ**：board.c が `BOARD_InitDebugConsole` の
   「アドレス」をXIP判定マクロに使うためGCで消えず参照が残る．
   debug_console コンポーネント一式の代わりに main.c のスタブで充足
4. target_stddef.h には `<stdint.h>`・`tool_stddef.h`・`TOPPERS_assert_abort`
   が必要（stm32cubemx 版の写しだけでは不足．Phase A版で補完）
5. EXC_RETURN_PREFIX が ASP3 arm_m.h と CMSIS core_cm33.h で二重定義
   （同値）→ fsl ヘッダ include を push/pop マクロで挟み警告ゼロ化

#### 検証結果（実機 EVK-MIMXRT685・2026-06-12）

| 項目 | 結果 |
|---|---|
| ビルド（sample1／test_porting） | 警告ゼロ（newlib nosys の注記を除く）・FCFB@0x400/SDKベクタ@0x1000/ASP3ベクタ@0x1200 を確認 |
| sample1 | バナー・task1周期実行・`r`（rot_rdq）で task1→2→3 切替 |
| test_porting | **6/6 passed** |
| testexec 全件（36テスト・`scripts/testexec_mcuxsdk.py`） | **33 PASS／1 SKIP（cpuexc10）／2 FAIL（cpuexc1/4＝arm_m共通の既知FAIL＝`issue-cpuexc-armm.md`．Secure実行でも同症状）**．dlynse NG=0＝較正値27/19有効 |

#### Git情報

- asp3_mcuxsdk：初期骨格 `fabb1a0` ベースに Phase B 実装を積む
- asp3_core 側の変更なし（受け入れ口は既存の ASP3_TARGET_DIR／ASP3_LIBRARY_ONLY で充足．
  本ファイル等のドキュメント更新のみ）

#### 仕上げ（2026-06-12・Phase A の main マージ後）

asp3_core 側の Phase A が PR #1（squash）で main にマージされた（`32f9749`）のを受け、
asp3_mcuxsdk の残課題を消し込んだ（コミットは asp3_mcuxsdk リポジトリ側）：

| 項目 | 内容 | コミット（asp3_mcuxsdk） |
|---|---|---|
| **asp3_core submodule bump** | submodule `asp3/asp3_core` を、削除済み feat ブランチ上の `ad50d96` から **マージ済み main `32f9749`** へ更新（PR #1 で取り込まれたため）。これで submodule が消えないブランチ上のコミットを指す | `6f1dc4b` |
| testexec 全件ランナ | `scripts/testexec_mcuxsdk.py`（configure差替え→build→書込み→シリアル判定．tty占有チェック・`--rejudge`付き）。全36テスト実績は上記検証結果表のとおり | （Phase B 実装時） |
| **CI（GitHub Actions）** | `.github/workflows/ci.yml` を新設．asp3_core と同じ開発コンテナで `submodules:recursive`→`cmake --preset Debug` の build-only（テストは実機要）。**初回CI成功（47秒）**。SDK が submodule 直参照＝GUI生成不要ゆえCIビルド可（CubeMX/RASC と違う利点） | `544a63b` |
| **移植skill** | `.claude/skills/porting-asp3-to-nxp`（SKILL.md＋reference 3本〔boot-vector-pitfalls・sdk-acquisition・flash-debug-tools〕＋checklists 2本＋snippet）．asp3_stm32cube の porting-asp3-to-stm32 と同構成 | `2966fcd` |

> CIで共有コンテナ `ghcr.io/toppers/asp3_core-dev` を使うには、GHCRパッケージの
> Actions access に asp3_mcuxsdk を追加する必要があった（新規リポジトリは既定で
> pull 不可＝「Initialize containers」で docker pull 失敗）。権限付与後に成功。

これで asp3_mcuxsdk 側 TODO（`docs/TODO.md`）の §1 CI・§2 移植skill・§5 submodule追従が
完了。残りは「必要に応じて」項目（SVD・追加アプリプロジェクト）のみ。

#### Git情報（仕上げ）

- asp3_mcuxsdk：`fabb1a0`（初期骨格）→ Phase B 実装 → `3a630f3`（実機検証・全体レビュー）
  → 仕上げ `6f1dc4b`（submodule bump）・`544a63b`（CI）・`2966fcd`（skill）
- asp3_core 側の変更は本ファイル等のドキュメント更新のみ（コードは PR #1 で main 反映済み）
