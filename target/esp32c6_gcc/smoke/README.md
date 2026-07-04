# ESP32-C6 boot smoke test（Phase A 第1マイルストーン）

ASP3カーネル／cfgには未統合の，スタンドアロンの最小検証プログラム。
Direct Boot・ウォッチドッグ無効化・polled USB Serial/JTAGコンソールが
実機（ESP32-C6FH4 rev v0.2）で動くことのみを確認する。
割込み（CLIC）が未実装のため，ASP3カーネル本体（cfg生成・
chip_kernel_impl.c等）へはまだ組み込めない（詳細はdocs/dev/
esp32c6-target.md）。

## ビルド（手動．CMake未統合）

```bash
cd target/esp32c6_gcc/smoke
riscv64-unknown-elf-gcc -march=rv32imac_zicsr_zifencei -mabi=ilp32 \
  -c ../flash_header.S -o flash_header.o -x assembler-with-cpp
riscv64-unknown-elf-gcc -march=rv32imac_zicsr_zifencei -mabi=ilp32 \
  -c start.S -o start.o -x assembler-with-cpp
riscv64-unknown-elf-gcc -march=rv32imac_zicsr_zifencei -mabi=ilp32 \
  -Os -ffreestanding -nostdlib -c main.c -o main.o
riscv64-unknown-elf-gcc -march=rv32imac_zicsr_zifencei -mabi=ilp32 \
  -nostdlib -nostartfiles -T link.ld \
  flash_header.o start.o main.o -o smoke.elf -Wl,--no-relax
riscv64-unknown-elf-objcopy -O binary \
  --only-section=.flash_header --only-section=.text smoke.elf smoke.bin
```

## 実機書込み・確認

```bash
esptool --chip esp32c6 --port /dev/ttyACM1 write-flash 0x0 smoke.bin
```

USB Serial/JTAG（`/dev/ttyACM1`）を115200bpsで開き，RTSでリセットする
と，以下が連続して出力される：

```
ASP3 ESP32-C6 smoke test: boot OK, polled USB-Serial/JTAG print
```

## 確認済み事項

- Direct Boot：C3と同じマジックナンバー（`0xaedb041d`×2＋flash+8への
  ジャンプ）で実機起動が成立する（公式のesp32c3-direct-boot-example
  リポジトリはC3/H2のみ対応と明記しているが，C6でも動作することを
  実機で確認した）。
- ウォッチドッグ：無効化しないと数秒以内に`TG0_WDT_HPSYS`リセットが
  掛かる（起動ログの`rst:0x7 (TG0_WDT_HPSYS)`で確認）。TIMG0/TIMG1・
  LP_WDT・SWDともC3と同じ解錠キー（`0x50D83AA1`／`0x8F1D312A`）で
  無効化に成功。
- コンソール：本ボードはUSB Serial/JTAGのみ（UART0は未配線）。
  レジスタレイアウトはC3と同一（オフセット0x00=EP1，0x04=EP1_CONF），
  ベースアドレスのみ異なる（`0x6000F000`）。

## 未確認・次の課題

- CPUクロックのPLL切替（PCR経由）：本テストはリセット既定クロックの
  まま（周波数未計測）。
- CLIC割込みコントローラ：本テストは割込み完全OFF（`mstatus.MIE`を
  一切有効化しない）。ASP3カーネル本体の統合にはこれが必須。
- SYSTIMER（HRT）：未検証。
- LPコアとの相互作用：未確認（本テストはHPコアのみ対象）。
