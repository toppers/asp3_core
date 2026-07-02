# Chip (ESP32-C3 RISC-V) awareness helpers for gdb OS-awareness (ASP3).
#
# pico2_riscv/rp2350版（Xh3irq・窓方式CSR）からの流用。ESP32-C3の割込み
# マトリクス（INTMTX）はメモリマップトレジスタで制御するため，窓方式CSR
# の exec_progbuf 読出しを単純なメモリ読出しに置き換えた。
#
# 役割: チップ（SoC）依存の知識。ESP32-C3の割込みはINTMTX（Interrupt
#       Matrix）で管理される。「指定 INTNO の割込み許可/禁止・ペンディング
#       状態」をINTMTXレジスタから読んで返す。
#
# ■ INTNO の対応
#   ASP3 の INTNO = ESP32-C3 の CPU割込み線番号そのまま（+1ずらしなし。
#   intmtx_kernel_impl.h参照）。
#
# ■ レジスタ（intmtx_kernel_impl.h と一致させること）
#   CPU_INT_ENABLE = 0x600C2104（bit n = CPU割込み線nの許可）
#   CPU_INT_EIP    = 0x600C2110（bit n = CPUへ到達している割込み線．
#                    許可・閾値通過後のため，禁止中の要求は観測できない）
#
# core_os_awareness.py は arch/riscv_gcc/common にあるため，本ファイルからの
# 相対パスで sys.path に追加してから import する。

import os
import sys

# 下位層(core)の import で .pyc を生成させない（ソースツリーに __pycache__ を残さない）。
sys.dont_write_bytecode = True

sys.path.insert(0, os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "../common")))

import core_os_awareness

import gdb

_INTMTX_CPU_INT_ENABLE = 0x600C2104
_INTMTX_CPU_INT_EIP = 0x600C2110


def _read_reg32(addr):
    """32ビットMMIOレジスタの読出し。"""
    return int(gdb.parse_and_eval("*(unsigned int *)0x%08x" % addr)) & 0xFFFFFFFF


def int_enabled(intno):
    """指定 INTNO の割込み許可状態（True=許可 / False=禁止）。"""
    return bool(_read_reg32(_INTMTX_CPU_INT_ENABLE) & (1 << int(intno)))


def int_pending(intno):
    """指定 INTNO のペンディング状態（True=ペンディング）。"""
    return bool(_read_reg32(_INTMTX_CPU_INT_EIP) & (1 << int(intno)))


# 割込みハンドラ番地の取得（_kernel_inh_table[INTNO]．INTNOで直接添字付け）と
# レディキュービットマップのビット方向は riscv 共通部の知識なので core 層の
# 実装をそのまま公開する。
inh_handler = core_os_awareness.inh_handler
primap_bit = core_os_awareness.primap_bit
