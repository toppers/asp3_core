# docs/dev/ — 機能追加の実施記録

機能追加項目ごとの経緯・手順書を置くディレクトリ。
ルールの正本は `AGENTS.md` §1「機能追加の実施ルール」を参照。

> 目的：変更点を明らかにし、他のTOPPERS系RTOSで同様の作業をする際の参考とすること。

- 実施プランは**着手前**に書き、実施結果は**完了時**に書く。
- `DIVERGENCE_MAP.md`（ファイル単位の上流乖離台帳）とは役割分担し、PRISTINE領域に変更が及んだ項目は相互にリンクする。

## 索引

項目名は `AGENTS.md` §1「機能追加計画」の表と一致させる。

| 項目 | ファイル | 状態 |
|---|---|---|
| TECSレス | `tecs-less.md` | 完了 |
| cfgのPython化 | `cfg-python.md` | 完了 |
| .rbツールの.py化 | `rb-tools-python.md` | 完了 |
| CMake対応 | `cmake.md` | 完了 |
| ファイルの削除 | `file-cleanup.md` | 完了 |
| QEMUターゲット(ARMv8-M) | `mps2-an505.md` | 完了（mps2-an521→an505/IoTKit 置換でハードFPU有効化。testexec 5/5・TTSP3 staticAPI 退行なし） |
| QEMUターゲット(ARMv7-M) | `mps2-an386.md` | 完了（Cortex-M4・非TZ・FPv4-SP。`__TARGET_ARCH_THUMB=4` 経路の検証。testexec 6/6・TTSP3 staticAPI an505と同一〔PASS 91/120〕。FP命令〔vpush/vpop {s16-s31}〕実行確認） |
| QEMUターゲット(ARMv8.1-M) | `mps3-an547.md` | 完了（Cortex-M55/SSE-300。MVE VPR 退避を asp3_fsp@30bf318 から共通archへ復活。testexec 5/5・VPR保持テスト PASS・非MVE回帰 an505 退行なし） |
| QEMUターゲット(ARMv8-A) | `qemu-target-a64.md` | 完了 |
| QEMUターゲット(RISC-V) | `qemu-target-riscv.md` | 完了 |
| CLIターゲット | `cli-target.md` | 完了 |
| CI整備 | `ci.md` | 完了 |
| OS Awareness 対応 | `os-awareness.md` | 完了 |
| 移植検証テスト | `porting-test.md` | 完了（polarfire QEMUのみCIで確認） |
| RISC-V Hazard3ターゲット | `pico2-riscv.md` | 完了（dlynse較正・testexec 36/36・OS Awareness実機確認まで完了） |
| ドキュメントMarkdown化 | `docs-markdown.md` | 完了 |
| devcontainer / Docker | `devcontainer.md` | 完了 |
| Pico SDK統合 | `pico-sdk-integration.md` | 完了（asp3_core側＝`ASP3_TARGET_DIR`＋`ASP3_LIBRARY_ONLY`＋pico2ターゲット改称／SDK側＝submodule移行・ARM-S/RISC-V とも PICO2実機動作確認・タイマ競合定量検証（ALARM0 vs ALARM3＝競合なし）／GitHub再編済。SDK統合版でのOS Awareness確認等は後続） |
| FSP統合 | `fsp-integration.md` | 完了（外側リポジトリ asp3_fsp＝A案submodule化・RASC6.2.0+ATfE clang。EK-RA6M5/EK-RA8M2 実機動作確認済み＝RA8M2はM85 exc_return整列・SCI起動化け・GPT HRTラップの3件を修正のうえ95秒連続走行で警告0） |
| STM32 HAL統合 | `stm32-integration.md` | 完了（外側リポジトリ asp3_stm32cube＝A案submodule化＋非TECS+Python cfg化。NUCLEO-H563ZI/H533RE 実機検証済み＝test_porting 6/6・testexec。H533REのVTOR整列が重要知見） |
| NXP MCUXpresso SDK統合 | `nxp-integration.md` | 完了（Phase A＝mimxrt685evk・Phase B＝asp3_mcuxsdk とも実機検証済＝testexec全件33/36 PASS。asp3_mcuxsdk側のCI・移植skillも消し込み済） |
| ESP-IDF統合 | `esp-idf-integration.md` | 実施中（Phase A完了＝esp32c3ターゲット・Direct Boot・QEMU＝test_porting 6/6・testexec 35/36〔dlynseはQEMU想定NG〕・CI追加／**実機検証済**＝rev v0.4・160MHz化・USB Serial/JTAGコンソール・test_porting 6/6・**testexec 36/36**・dlynse較正。Phase B（外側リポジトリasp3_esp_idf）＝esp-hal統合（B-1）・Wi-Fi os_adapter shim（B-2a scan／B-2b WPA2接続）とも実機成功。Phase C＝lwIP統合（DHCP＋ゲートウェイping）も実機成功。残＝OS Awareness実機確認） |
| skillパッケージ | `skill-package.md` | 完了（移植ガイドskillとして各SDKリポジトリ内に実装＝asp3_fsp/asp3_stm32cube。picoは不要と判断。当初計画からの変更点は本ファイル参照） |
| メモリ保護 | `memory-protection.md` | 計画中（arm_m 静的MPU スタックガード。PSPLIMは実装済み） |
| SAFEG（ARMv8-M TrustZone） | `safeg.md` | 完了（SafeG-M デュアルOS。M1〜M4＋Phase B。an505(QEMU)/pico2/mimxrt685evk。既定OFF＝素ASP3不変） |
| TTSP3 適合性テスト | `ttsp3-conformance.md` | 実施中（zybo_z7 functional 1813/1813＋staticAPI 138/138 PASS。横断＝mps系3ターゲット〔an505/an386/an547＝M33/M4/M55〕で functional 全1813件通し走行・いずれも FAIL0、staticAPI も3ターゲットで FAIL29件が完全一致＝cfgエンジン差分と確定／polarfire(RV64) build-only／zcu102 資産済。nightly CI実装済〔TTSP3 public化待ちで停止中〕。HWタイマ/割込み本対応は後続） |

状態：計画中 → 実施中 → 完了（各項目の進行に合わせて更新する）

### 参照リファレンス（機能単位の経緯とは別の常設台帳）

| 台帳 | ファイル | 内容 |
|---|---|---|
| CFG_SPEC_MAP | `cfg-spec-map.md` | cfg 上流追従の対応台帳（層①api-table／層②エンジン／層③テンプレート・最終確認バージョン）。AGENTS.md §7・§10 から参照 |

### 調査メモ（機能追加項目とは別の課題）

| 課題 | ファイル | 状態 |
|---|---|---|
| arm_mでcpuexc1/4が失敗する件（SIL_LOC_INT=PRIMASKによるHardFault昇格・上流固有） | `issue-cpuexc-armm.md` | 原因判明・対処未決（ユーザー判断待ち） |

## テンプレート

各.mdは以下の構成で作成する。

```markdown
# <項目名>

## 項目

（AGENTS.md §1 機能追加計画の項目名・優先度）

## 内容

（何を・なぜ行うか。上流ASP3からの変更観点）

## 実施プラン

（着手前に記載。手順・影響範囲・リスク）

## 実施結果

（完了時に記載）

### 変更したファイル

| ファイル | 変更内容概要 |
|---|---|
|  |  |

### 追加したファイル

### 削除したファイル

### Git情報

- ベースコミット：
- 関連コミット範囲：
- ファイルリスト再現コマンド例：`git diff --stat upstream main -- <paths>`

### 検証結果

| テスト | 実施 | 結果 |
|---|---|---|
| POSIX | ○/− |  |
| QEMU (mps2-an505) | ○/− |  |
| 実機 | ○/− |  |

### DIVERGENCE_MAP との関連

（PRISTINE領域に変更が及んだ場合、該当エントリへのリンク）
```
