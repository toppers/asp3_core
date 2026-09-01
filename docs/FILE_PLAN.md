# FILE_PLAN.md — 上流マージの実コマンドとリポジトリ運用

> 初期計画（フォルダ構成・Git運用の設計）は実現済みであり、**正本は移行済み**：
> - フォルダ構成・変更種別 → `AGENTS.md` §3
> - ブランチ構成・コミット規則 → `AGENTS.md` §9
> - 上流マージの手順概要 → `AGENTS.md` §10（本ファイルはその**実コマンド集**）
>
> 本ファイルに残すのは、AGENTSに収まらない実務詳細（上流マージの具体コマンド・
> 既存リポジトリのsubmodule移行構想・タグ方針）のみ。

## 1. 上流マージの実コマンド（AGENTS §10 の実務版）

```bash
# 1. upstreamブランチに新バージョンを取り込む
git checkout upstream
# 新ASP3タルボールを展開してファイルを上書きコピー後：
git add -A
git commit -m "Import TOPPERS/ASP3 3.8.0"

# 2. 上流の変更箇所を確認（= DIVERGENCE_MAPとの照合起点）
git diff upstream~1 upstream          # 上流での変更内容
git diff upstream main -- kernel/     # kernel/への影響
git diff upstream main -- syssvc/     # syssvc/への影響

# 3. AIにDIVERGENCE_MAP + 上流diffを渡して影響箇所を抽出
#    「このdiffでDIVERGENCE_MAPのどの行が影響を受けるか列挙せよ」

# 4. mainに必要な変更を手動で取り込む
git checkout main
# UPSTREAM_PRISTINEのファイルは git checkout upstream -- <file> で更新
# DIVERGENCEファイルは差分を確認しながら手動マージ
# （注意：DIVERGENCE_MAP「削除済みファイル」節のファイルは復活させない。
#   doc/*.txt のdiffは docs/spec/ へ、cfg.rb系のdiffは cfg/*.py へ手動反映）
git add -A
git commit -m "chore(upstream): merge ASP3 3.8.0"

# 5. UPSTREAM_VERSIONを更新
echo "ASP3_UPSTREAM_VERSION = 3.8.0" > UPSTREAM_VERSION
echo "ASP3_LAST_MERGED      = $(date +%Y-%m-%d)" >> UPSTREAM_VERSION
git add UPSTREAM_VERSION
git commit -m "chore: update UPSTREAM_VERSION to 3.8.0"

# 6. 回帰確認（push すればCIが全ターゲットを実行）
cmake --preset linux -B build/linux && cmake --build build/linux
ctest --preset linux
```

## 2. 既存3リポジトリの移行方針（Pico SDK統合等で使用）

移行後は元リポジトリが本カーネルをsubmoduleとして参照する構成に切り替える。

```
asp3_pico_sdk/                  ← 既存リポジトリ（アプリ層のみ残す）
├── asp3_core/                  ←   submodule: 本カーネル
├── src/                        ←   アプリケーション
└── CMakeLists.txt

asp3_fsp/ ・ asp3_stm32cube/  ← 同様
```

```bash
# 例：asp3_pico_sdkを移行後
cd asp3_pico_sdk
git submodule add https://github.com/toppers/asp3_core asp3_core
git submodule update --init
```

## 3. タグ・マイルストーン

主要マイルストーンに注釈付きタグを打つ：

```bash
git tag -a v0.1.0 -m "Initial: TECS-less + Python cfg + CMake + M33/A35"
```
