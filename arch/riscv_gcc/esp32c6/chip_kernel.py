# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのチップ依存部（ESP32-C6用）
#
#  pico2_riscv/rp2350版からの流用。INTNO_VALIDをESP32-C6のCPU割込み線
#  番号（1〜31）に変更。
#

#
#  使用できる割込み番号とそれに対応する割込みハンドラ番号
#  （ASP3の割込み番号INTNO = ESP32-C6のCPU割込み線番号（1〜31）そのまま．
#  +1ずらしなし）
#
INTNO_VALID = list(range(1, 32))
INHNO_VALID = INTNO_VALID

#
#  生成スクリプトのコア依存部
#
IncludeTrb("core_kernel.py")
