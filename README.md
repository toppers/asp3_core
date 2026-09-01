# TOPPERS/ASP3 Core

[![CI](https://github.com/toppers/asp3_core/actions/workflows/ci.yml/badge.svg)](https://github.com/toppers/asp3_core/actions/workflows/ci.yml)

A bare-metal RTOS kernel based on TOPPERS/ASP3, restructured for AI-driven development.

[TOPPERS/ASP3](https://www.toppers.jp/) 3.7.2 をベースとした**改変版**カーネルです。
上流を手動マージで追従しながら、各社SDK（Pico SDK / Renesas FSP / STM32）と
協調動作する共通カーネル基盤を目指しています。
（「TOPPERS/ASP3 Core」の名称は仮称です）

## 特徴

- **AI開発フレンドリ**：TECSレス（プレーンC）・Pythonコンフィギュレータ・
  構造化ログ（`T=,EV=`）・TAP出力・gdb OS Awareness — エージェントが
  build→run→test→debug をテキストで完結できる
- **ハードなしで全ISA検証**：POSIXシミュレーション＋QEMU 6機種
  （Cortex-M33 / M4 / M55 ・ A9 / A53 / RV64GC）。push毎にCIが全ターゲットを回帰し、
  nightlyで TTSP3（TOPPERSテストスイート）適合性テストも実行
- **CMake一本**：`cmake --preset <名前>` だけでcfg 3パス〜ELF生成まで自動
- **上流追従を前提とした構造**：kernel/はPRISTINE維持・乖離はDIVERGENCE_MAPで台帳管理

## クイックスタート

```bash
# 最速の動作確認（ハードなし・ホスト実行）
cmake --preset linux -B build/linux && cmake --build build/linux
./build/linux/asp

# QEMU（Cortex-M33）
cmake --preset mps2_an505-qemu && cmake --build --preset run-mps2_an505-qemu
```

プリセット一覧は `cmake --list-presets`。詳細は [docs/building.md](docs/building.md)。

### 開発環境（Docker / devcontainer）

ツールチェーン・QEMU・Pythonをピン留めした開発コンテナ
（`ghcr.io/toppers/asp3_core-dev`）を用意しています。
**CIも同一イメージで実行**するため、コンテナ内で通ればCIでも通ります。

- VS Code / Claude Code：リポジトリを開いて「Reopen in Container」（[.devcontainer/](.devcontainer/)）
- 手順の詳細：[docs/building.md §6「開発コンテナでのビルド」](docs/building.md#6-開発コンテナでのビルド)
- 設計・経緯：[docs/dev/devcontainer.md](docs/dev/devcontainer.md)

## 対応ターゲット

| ターゲット | ISA | 実行環境 |
|---|---|---|
| linux_gcc | (host) | POSIXシミュレーション |
| mps2_an505_gcc | ARMv8-M (Cortex-M33) | QEMU |
| mps2_an386_gcc | ARMv7-M (Cortex-M4) | QEMU |
| mps3_an547_gcc | ARMv8.1-M (Cortex-M55 / MVE) | QEMU |
| zybo_z7_gcc | ARMv7-A (Cortex-A9) | 実機 / QEMU |
| zcu102_arm64_gcc | ARMv8-A (Cortex-A53) | 実機 / QEMU |
| polarfire_soc_kit_gcc | RISC-V (RV64GC) | 実機 / QEMU |
| pico2_arm_gcc | ARMv8-M (Cortex-M33) | 実機 |
| pico2_riscv_gcc | RISC-V (RV32IMAC / Hazard3) | 実機 |
| mimxrt685evk_gcc | ARMv8-M (Cortex-M33) | 実機 |
| stm32mp257f_dk_arm64_gcc | ARMv8-A (Cortex-A35) | 実機 |

## ドキュメント

| 知りたいこと | 場所 |
|---|---|
| **開発規約・手順の正本（AI/人間共通）** | [AGENTS.md](AGENTS.md) |
| プロジェクト全体像・上流からの変更点 | [docs/OVERVIEW.md](docs/OVERVIEW.md) |
| ビルド・テスト手順 | [docs/building.md](docs/building.md) |
| 新ターゲット移植ガイド | [docs/porting/PORTING_GUIDE.md](docs/porting/PORTING_GUIDE.md) |
| 機能追加の経緯・実施記録 | [docs/dev/README.md](docs/dev/README.md) |
| 上流との乖離台帳 | [DIVERGENCE_MAP.md](DIVERGENCE_MAP.md) |

AIコーディングツール（Claude Code / Cline / Cursor等）で開発する場合は
**まず [AGENTS.md](AGENTS.md) を読んでください**（CLAUDE.md等は薄いポインタです）。

## 上流との関係・ライセンス

- ベース：TOPPERS/ASP3 Release 3.7.2（`UPSTREAM_VERSION` 参照）
- 本リポジトリは上流の**改変版**です。乖離は [DIVERGENCE_MAP.md](DIVERGENCE_MAP.md) で管理し、
  発見した上流の不具合は報告しています（例：[docs/dev/upstream-report-tracelog.md](docs/dev/upstream-report-tracelog.md)）
- ライセンス：[TOPPERSライセンス](https://www.toppers.jp/license.html)
