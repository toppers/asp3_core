#
#		ターゲット依存部のCMake定義（ESP32-C6用）
#
#  変数を積み上げ，チップ依存部の chip.cmake をincludeする
#  （AGENTS.md §11）．ESP-IDFは使用しない（ベアメタル移植・Direct Boot．
#  経緯は docs/dev/esp-idf-integration.md）．
#

set(TARGETDIR ${ASP3_ROOT_DIR}/target/esp32c6_gcc)

#
#  コンフィギュレーション関連
#
list(APPEND ASP3_CFG_FILES
    ${TARGETDIR}/target_kernel.cfg
)

list(APPEND ASP3_KERNEL_CFG_TRB_FILES
    ${TARGETDIR}/target_kernel.py
)

list(APPEND ASP3_CHECK_TRB_FILES
    ${TARGETDIR}/target_check.py
)

#
#  インクルードディレクトリ
#
list(APPEND ASP3_INCLUDE_DIRS
    ${TARGETDIR}
)

#
#  QEMU／実機の切り替え（既定：QEMU）
#
#  OFF（実機ESP32-C6-DevKit）にすると TOPPERS_USE_QEMU を定義しない：
#    - target_exit のセミホスティング終了を行わない
#  実機のロード手段（esptool書込み手順）は実機対応時に整備する．
#
option(ESP32C6_QEMU "Build for QEMU esp32c6 (OFF: real ESP32-C6 board)" ON)

#
#  コンソールの選択（chip.cmake参照）．既定はQEMU=UART0・実機=USB
#  Serial/JTAG（UARTブリッジを持たないネイティブUSBボードを想定．
#  UART配線のあるボードでは -DESP32C6_CONSOLE=uart0 を指定する）．
#
if(ESP32C6_QEMU)
    set(_esp32c6_console_default uart0)
else()
    set(_esp32c6_console_default usbjtag)
endif()
set(ESP32C6_CONSOLE ${_esp32c6_console_default}
    CACHE STRING "Console device: uart0 or usbjtag")

#
#  コンパイル定義
#
#  USE_TIM_AS_HRT：高分解能タイマにSYSTIMERを使用（Machine Timer不使用）
#
list(APPEND ASP3_COMPILE_DEFS
    USE_TIM_AS_HRT
)

if(ESP32C6_QEMU)
    list(APPEND ASP3_COMPILE_DEFS TOPPERS_USE_QEMU)
endif()

#
#  リンクオプション
#
list(APPEND ASP3_LINK_OPTIONS
    -Wl,--print-memory-usage
    -Wl,--gc-sections
    -Wl,--build-id=none
)

set(ASP3_LDSCRIPT ${TARGETDIR}/esp32c6.ld)

#
#  ターゲット依存部のソース
#
list(APPEND ASP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
    ${TARGETDIR}/target_timer.c
    ${TARGETDIR}/flash_header.S
)

#
#  チップ依存部のインクルード
#
include(${ASP3_ROOT_DIR}/arch/riscv_gcc/esp32c6/chip.cmake)

#
#  QEMUによる実行（cmake --build <dir> --target run）
#
#  Espressif版QEMU（esp32c6マシンを持つフォーク）が必要．PATHにない
#  場合は -DQEMU_SYSTEM_RISCV32_ESP=/path/to/qemu-system-riscv32 で
#  指定する．QEMUはELFではなくフラッシュイメージ（asp_flash.bin＝
#  ポストビルドで生成．run.cmake参照）から起動する．
#
if(ESP32C6_QEMU)
    set(QEMU_SYSTEM_RISCV32_ESP qemu-system-riscv32
        CACHE STRING "Path to Espressif qemu-system-riscv32 (esp32c6 machine)")
    set(ASP3_RUN_COMMAND
        ${QEMU_SYSTEM_RISCV32_ESP} -M esp32c6 -nographic
        -drive file=${CMAKE_BINARY_DIR}/asp_flash.bin,if=mtd,format=raw
        -semihosting
    )
else()
    #
    #  実機への書込み（cmake --build <dir> --target run）．
    #  同じasp_flash.bin（Direct Boot形式）をesptoolでフラッシュ先頭へ
    #  書き込む．コンソールは書込みと同じUSBポート（USB Serial/JTAG＝
    #  /dev/ttyACM*）に出る（ESP32C6_CONSOLE=usbjtag時）．
    #
    set(ESP32C6_ESPTOOL esptool
        CACHE STRING "Path to esptool")
    set(ESP32C6_PORT /dev/ttyACM0
        CACHE STRING "Serial port of the ESP32-C6 board")
    set(ASP3_RUN_COMMAND
        ${ESP32C6_ESPTOOL} --chip esp32c6 --port ${ESP32C6_PORT}
        write-flash 0x0 ${CMAKE_BINARY_DIR}/asp_flash.bin
    )
endif()

#
#  フラッシュイメージ生成等（aspターゲット定義後に取込み）
#
set(ASP3_TARGET_RUN_CMAKE ${TARGETDIR}/run.cmake)
