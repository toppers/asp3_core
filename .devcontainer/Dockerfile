#
#		TOPPERS/ASP3 Core 開発コンテナ（devcontainer / CI 共用）
#
#  設計・経緯は docs/dev/devcontainer.md を参照．
#  - ubuntu:26.04 ベース・シングルステージ（QEMU はソースビルドせず apt 版
#    10.2 を使用．icicle-kit 直接ブート対応＝CI polarfire ジョブで実績あり）
#  - ツールチェーンは apt 3系統＋aarch64-none-elf（ARM公式tarball）．
#    arm64 は aarch64-none-elf に一本化し，gcc-aarch64-linux-gnu は
#    入れない（2系統混在の回避．toolchain-a35.cmake の既定プレフィックスが
#    aarch64-none-elf のため，コンテナ内では A35_TOOLCHAIN_PREFIX 不要）
#
#  検証済みバージョン（2026-06-06 時点の ubuntu:26.04 apt）：
#    host gcc 15.2 / arm-none-eabi 14.2 / riscv64-unknown-elf 14.2 /
#    aarch64-none-elf 13.3.rel1（tarball固定）/ CMake 4.2 / Ninja /
#    Python 3.14 / QEMU 10.2.1 / cppcheck 2.19
#  ※ apt パッケージは `=version` でピンしない（Ubuntuアーカイブは旧版を
#    保持しないため再ビルドが壊れる）．バージョン固定はイメージの日付タグ
#    （ghcr.io/toppers/asp3_core-dev:YYYYMMDD）で担保する．
#
FROM ubuntu:26.04

ARG DEBIAN_FRONTEND=noninteractive

#  apt 一式（26.04では riscv の QEMU が qemu-system-misc から
#  qemu-system-riscv に分離している点に注意）
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils \
        git gh sudo \
        build-essential cmake ninja-build \
        python3 \
        gcc-arm-none-eabi libnewlib-arm-none-eabi \
        gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf \
        qemu-system-arm qemu-system-riscv \
        cppcheck clang-tidy clangd jq \
        device-tree-compiler \
        gdb-multiarch \
    && rm -rf /var/lib/apt/lists/*

#  aarch64-none-elf（ベアメタル用．aptには26.04でも存在しないため
#  ARM公式tarballを使用．バージョンはCIのstm32ジョブと同一の13.3.rel1）
ARG ARM_TOOLCHAIN_NAME=arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-elf
ARG ARM_TOOLCHAIN_SHA256=7fedf894040580b1db747d06ac5d4263c46e591ffe7695656d1da5accb00a159
RUN curl -fsSL -o /tmp/toolchain.tar.xz \
        "https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/${ARM_TOOLCHAIN_NAME}.tar.xz" \
    && echo "${ARM_TOOLCHAIN_SHA256}  /tmp/toolchain.tar.xz" | sha256sum -c - \
    && tar -xJf /tmp/toolchain.tar.xz -C /opt \
    && rm /tmp/toolchain.tar.xz \
    #  ドキュメント類を削除してサイズ節減
    && rm -rf "/opt/${ARM_TOOLCHAIN_NAME}/share/doc" \
              "/opt/${ARM_TOOLCHAIN_NAME}/share/info" \
              "/opt/${ARM_TOOLCHAIN_NAME}/share/man"
ENV PATH="/opt/${ARM_TOOLCHAIN_NAME}/bin:${PATH}"

#  非rootユーザ（devcontainer用）．ubuntu:26.04既定の ubuntu(UID 1000) を
#  devcontainers 慣行の vscode に置き換える．末尾の USER vscode が既定の
#  ため、CIの container: 実行では options: --user root で上書きする
#  （Actionsのランナーファイルへの書込みにrootが必要）
RUN userdel -r ubuntu \
    && useradd -m -s /bin/bash -u 1000 vscode \
    && echo 'vscode ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/vscode \
    && chmod 0440 /etc/sudoers.d/vscode

#  ツールチェーンのバージョン一覧（イメージ検証・記録用）
RUN { gcc --version | head -1; \
      arm-none-eabi-gcc --version | head -1; \
      aarch64-none-elf-gcc --version | head -1; \
      riscv64-unknown-elf-gcc --version | head -1; \
      cmake --version | head -1; \
      ninja --version; \
      python3 --version; \
      qemu-system-arm --version | head -1; \
      qemu-system-riscv64 --version | head -1; \
      cppcheck --version; \
    } | tee /etc/asp3_core-toolchain-versions.txt

USER vscode
WORKDIR /workspaces
