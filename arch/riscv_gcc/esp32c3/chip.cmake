#
#		チップ依存部のCMake定義（ESP32-C3用）
#
#  target.cmake からincludeされる（Makefile.chipのCMake版に相当）．
#
#  RP2350 RISC-V（arch/riscv_gcc/rp2350）を雛形に，割込みコントローラを
#  Xh3irq→割込みマトリクス（INTMTX），UARTをPL011系→ESP32-C3 UARTに
#  置き換えたもの．ISAはRV32IMC（A拡張なし＝Hazard3との差分）．
#

set(CHIPDIR ${ASP3_ROOT_DIR}/arch/riscv_gcc/esp32c3)

list(APPEND ASP3_INCLUDE_DIRS
    ${CHIPDIR}
)

#
#  ESP32-C3コア（RV32IMC＋Zicsr/Zifencei．A拡張・FPU無し）
#
#  ツールチェーンのマルチリブはrv32imcを持たないためrv32im/ilp32が
#  リンクされる（ABI互換・C拡張の有無はコードサイズのみの差）．
#
list(APPEND ASP3_COMPILE_OPTIONS
    -march=rv32imc_zicsr_zifencei
    -mabi=ilp32
    -mcmodel=medany
    -msmall-data-limit=8
    -mstrict-align
    -mno-save-restore
    -fsigned-char
    -ffunction-sections
)

list(APPEND ASP3_LINK_OPTIONS
    -march=rv32imc_zicsr_zifencei
    -mabi=ilp32
)

list(APPEND ASP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
    ${CHIPDIR}/chip_support.S
)

#
#  非TECS版SIOドライバ（UART0）
#
list(APPEND ASP3_SYSSVC_TARGET_C_FILES
    ${CHIPDIR}/chip_serial.c
    ${CHIPDIR}/esp32c3_uart.c
)

#
#  PLIC・Machine Timerは使用しない（割込みコントローラは割込み
#  マトリクス，高分解能タイマはSYSTIMER（ターゲット依存部）を使用）
#
set(ASP3_RISCV_OMIT_PLIC_MTIMER ON)

#
#  コア依存部のインクルード
#
include(${ASP3_ROOT_DIR}/arch/riscv_gcc/common/arch.cmake)

#
#  ベアメタルリンクのため libc は使用しない（PolarFire・RP2350と同様）．
#  コンパイラが生成する memcpy/memset 等は libc_stub.c（ISA非依存・
#  PolarFire依存部からパス参照）が提供する．
#
list(REMOVE_ITEM ASP3_LINK_LIBS c)
list(APPEND ASP3_ARCH_C_FILES
    ${ASP3_ROOT_DIR}/arch/riscv_gcc/polarfire_soc/libc_stub.c
)
