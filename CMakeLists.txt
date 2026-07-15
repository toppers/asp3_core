#
#		TOPPERS/ASP3 Core CMakeビルド
#
#  asp3_fsp / asp3_pico_sdk のCMake実装をベースに，SDK非依存の
#  スタンドアロンビルド（サンプルアプリ asp）として構成したもの．
#
#  使い方（例）：
#    cmake --preset mps2_an505-qemu -B build/mps2_an505-qemu
#    cmake --build build/mps2_an505-qemu
#    cmake --build build/mps2_an505-qemu --target run    # QEMU実行
#
cmake_minimum_required(VERSION 3.16)

project(asp3_core C ASM)

include(${CMAKE_CURRENT_LIST_DIR}/asp3_core.cmake)

#
#  ライブラリ専用モード（外部SDKアプリ向け）
#
#  ON のとき，asp 実行ファイル・サンプルアプリ・ctest登録を作らず，
#  カーネルライブラリ（asp3）・cfg生成・ヘルパ関数（asp3_add_syssvc /
#  asp3_cfg_check）のみを公開する．外部リポジトリ（各社SDK）が
#  add_subdirectory(asp3_core) して，自前の最終実行ファイルから asp3 を
#  リンクする構成で使う（Pico SDK統合・docs/dev/pico-sdk-integration.md）．
#  既定OFF＝従来どおり asp 実行ファイルまでビルド（既存プリセットに影響なし）．
#
option(ASP3_LIBRARY_ONLY
    "Build only the asp3 kernel library (no asp executable / sample)" OFF)

#
#  Pythonインタプリタ（cfg実行用）
#
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PYTHON_EXE ${Python3_EXECUTABLE} -B)

#
#  cfg（コンフィギュレータ）関連の定義
#
set(CFG_PY ${ASP3_ROOT_DIR}/cfg/cfg.py)

list(APPEND ASP3_SYMVAL_TABLES
    ${ASP3_ROOT_DIR}/kernel/kernel_sym.def
)

list(APPEND ASP3_API_TABLES
    ${ASP3_ROOT_DIR}/kernel/kernel_api.def
)

list(APPEND ASP3_INCLUDE_DIRS
    ${ASP3_ROOT_DIR}/include
)

list(APPEND ASP3_COMPILE_DEFS
    TOPPERS_OMIT_TECS
)

#
#  共通コンパイルオプション（sample/MakefileのCOPTS既定と同一）
#
list(APPEND ASP3_COMPILE_OPTIONS
    -O2 -Wall -g
)

#
#  追加のコンパイル定義（テストランナ等が -DASP3_EXTRA_COMPILE_DEFS=
#  "HRT_CONFIG1;SIMTIM_TEST" の形で指定する）
#
if(DEFINED ASP3_EXTRA_COMPILE_DEFS)
    list(APPEND ASP3_COMPILE_DEFS ${ASP3_EXTRA_COMPILE_DEFS})
endif()

#
#  生成されるCコードの出力先
#
set(CFG_GEN_DIR ${CMAKE_BINARY_DIR}/generated)
set(CFG1_OUT_FILE ${CFG_GEN_DIR}/cfg1_out.c)
set(CFG1_OUT cfg1_out)
set(OFFSET_H_FILE ${CFG_GEN_DIR}/offset.h)
set(CFG1_OUT_SYMS_FILE ${CFG_GEN_DIR}/cfg1_out.syms)
set(CFG1_OUT_SREC_FILE ${CFG_GEN_DIR}/cfg1_out.srec)
set(CFG1_OUT_DB_FILE ${CFG_GEN_DIR}/cfg1_out.db)
set(CFG2_OUT_DB_FILE ${CFG_GEN_DIR}/cfg2_out.db)
file(MAKE_DIRECTORY ${CFG_GEN_DIR})

#
#  tecsgen.cfgスタブの生成（非TECS環境向け）
#  test/のテストプログラムの.cfgが INCLUDE("tecsgen.cfg") するため，
#  非TECS版システムサービスのINCLUDEに展開するスタブを生成しておく
#  （非TECS版システムサービス ユーザーズマニュアル §2.7の方法）
#
if(NOT EXISTS ${CFG_GEN_DIR}/tecsgen.cfg)
    file(WRITE ${CFG_GEN_DIR}/tecsgen.cfg
        "INCLUDE(\"syssvc/syslog.cfg\");\n"
        "INCLUDE(\"syssvc/banner.cfg\");\n"
        "INCLUDE(\"syssvc/serial.cfg\");\n")
endif()

#
#  アプリケーション（サンプル）の定義
#
if(NOT DEFINED ASP3_APPLNAME)
    set(ASP3_APPLNAME sample1)
endif()
if(NOT DEFINED ASP3_APPLDIR)
    set(ASP3_APPLDIR ${ASP3_ROOT_DIR}/sample)
endif()
#  相対パス指定（例：-DASP3_APPLDIR=test/porting）はソースルート基準で解決する
if(NOT IS_ABSOLUTE ${ASP3_APPLDIR})
    set(ASP3_APPLDIR ${ASP3_ROOT_DIR}/${ASP3_APPLDIR})
endif()
#  追加アプリソース（ASP3_EXTRA_APP_C_FILES）の相対パスも同様に解決する
if(DEFINED ASP3_EXTRA_APP_C_FILES)
    set(_extra_app_c_files "")
    foreach(path IN LISTS ASP3_EXTRA_APP_C_FILES)
        if(NOT IS_ABSOLUTE ${path})
            set(path ${ASP3_ROOT_DIR}/${path})
        endif()
        list(APPEND _extra_app_c_files ${path})
    endforeach()
    set(ASP3_EXTRA_APP_C_FILES ${_extra_app_c_files})
endif()
#  .cfgファイル名がアプリ名と異なる場合は ASP3_APPCFGNAME で指定する
if(DEFINED ASP3_APPCFGNAME)
    set(ASP3_APP_CFG_FILE ${ASP3_APPLDIR}/${ASP3_APPCFGNAME}.cfg)
else()
    set(ASP3_APP_CFG_FILE ${ASP3_APPLDIR}/${ASP3_APPLNAME}.cfg)
endif()
list(APPEND ASP3_APP_INCLUDE_DIRS ${ASP3_APPLDIR})

#
#  ターゲット依存部の情報を読み込み
#
#  target.cmake は以下の変数を積み上げ，アーキ依存部の arch.cmake を
#  includeする（AGENTS.md §11）：
#    ASP3_CFG_FILES / ASP3_KERNEL_CFG_TRB_FILES / ASP3_CHECK_TRB_FILES /
#    ASP3_OFFSET_TRB_FILES / ASP3_SYMVAL_TABLES / ASP3_INCLUDE_DIRS /
#    ASP3_COMPILE_DEFS / ASP3_COMPILE_OPTIONS / ASP3_LINK_OPTIONS /
#    ASP3_LINK_LIBS / ASP3_LDSCRIPT / ASP3_ARCH_C_FILES /
#    ASP3_TARGET_C_FILES / ASP3_SYSSVC_TARGET_C_FILES / ASP3_RUN_COMMAND
#
#  ASP3_TARGET_DIR は asp3_core.cmake で解決済み（既定＝target/${ASP3_TARGET}，
#  外部SDKターゲットは -DASP3_TARGET_DIR=<外部の絶対パス> で供給）
include(${ASP3_TARGET_DIR}/target.cmake)

#
#  トレースログ機能（arch/tracelog・非TECS版）
#
#  ASP3_ENABLE_TRACE=ON で TOPPERS_ENABLE_TRACE を定義し，カーネル内の
#  LOG_*フックを有効化してトレースログ実装をリンクする（既定OFF）．
#  linuxプリセットは既定ONで，実行時の --slog で構造化ログ
#  （T=,EV=形式．AGENTS.md §8）を出力できる．
#
option(ASP3_ENABLE_TRACE "Enable kernel trace log (arch/tracelog)" OFF)
if(ASP3_ENABLE_TRACE)
    list(APPEND ASP3_COMPILE_DEFS TOPPERS_ENABLE_TRACE)
    list(APPEND ASP3_ARCH_C_FILES
        ${ASP3_ROOT_DIR}/arch/tracelog/trace_log.c
        ${ASP3_ROOT_DIR}/arch/tracelog/trace_dump.c
        ${ASP3_ROOT_DIR}/arch/tracelog/trace_slog.c
    )
endif()

list(APPEND ASP3_CFG_FILES
    ${ASP3_APP_CFG_FILE}
)

list(APPEND ASP3_INCLUDE_DIRS
    ${ASP3_ROOT_DIR}
    ${CFG_GEN_DIR}
)

#
#  リンカスクリプトの指定
#
if(DEFINED ASP3_LDSCRIPT)
    list(APPEND ASP3_LINK_OPTIONS -Wl,-T,${ASP3_LDSCRIPT})
endif()

#
#  cfgコマンドライン引数の組み立て
#
foreach(path IN LISTS ASP3_SYMVAL_TABLES)
    list(APPEND CFG_SYMVAL_TABLES "--symval-table" ${path})
endforeach()

foreach(path IN LISTS ASP3_API_TABLES)
    list(APPEND CFG_API_TABLES "--api-table" "${path}")
endforeach()

foreach(path IN LISTS ASP3_OFFSET_TRB_FILES)
    list(APPEND CFG_OFFSET_TRB_FILES "-T" ${path})
endforeach()

foreach(path IN LISTS ASP3_KERNEL_CFG_TRB_FILES)
    list(APPEND CFG_KERNEL_CFG_TRB_FILES "-T" "${path}")
endforeach()

foreach(path IN LISTS ASP3_CHECK_TRB_FILES)
    list(APPEND CFG_CHECK_TRB_FILES "-T" ${path})
endforeach()

foreach(path IN LISTS ASP3_INCLUDE_DIRS ASP3_APP_INCLUDE_DIRS)
    list(APPEND CFG_INCLUDE_DIRS "-I${path}")
endforeach()

set(CFG_GEN_H_FILES ${CFG_GEN_DIR}/kernel_cfg.h)
set(CFG_GEN_C_FILES ${CFG_GEN_DIR}/kernel_cfg.c)

#
#  カーネルのコンフィギュレーションファイルの生成
#
#  パス1：.cfg＋api-table → cfg1_out.c
#
add_custom_command(
    OUTPUT ${CFG1_OUT_FILE} ${CFG1_OUT_DB_FILE}
    WORKING_DIRECTORY ${CFG_GEN_DIR}
    COMMAND ${PYTHON_EXE} ${CFG_PY} --pass 1 --kernel asp
            ${CFG_INCLUDE_DIRS} ${CFG_API_TABLES} ${CFG_SYMVAL_TABLES}
            -M ${CFG_GEN_DIR}/cfg1_out_c.d ${ASP3_CFG_FILES}
    DEPENDS ${CFG_PY} ${ASP3_SYMVAL_TABLES} ${ASP3_API_TABLES} ${ASP3_CFG_FILES}
    COMMENT "Running cfg pass 1 to generate cfg1_out.c"
)

#
#  cfg1_outのビルド（静的APIパラメータの値の取り出し用）
#
#  ASP3_START_FILES（start.S等）はライブラリ外で先頭にリンクする
#  （Makefile版のSTART_OBJSに相当．arm_gcc系で使用）
add_executable(${CFG1_OUT} ${ASP3_START_FILES} ${CFG1_OUT_FILE})
target_include_directories(${CFG1_OUT}
    PRIVATE ${ASP3_INCLUDE_DIRS} ${ASP3_APP_INCLUDE_DIRS}
    PRIVATE ${ASP3_ROOT_DIR}/kernel    # ASP3_START_FILES（start.S）用
)
target_compile_definitions(${CFG1_OUT} PRIVATE ${ASP3_COMPILE_DEFS})
target_compile_options(${CFG1_OUT} PRIVATE ${ASP3_COMPILE_OPTIONS})
#  cfg1_outは全シンボルの値をシンボルテーブルから取り出すため，
#  セクションガベージコレクションは行わない（Makefileビルドと同じ）
set(CFG1_LINK_OPTIONS ${ASP3_LINK_OPTIONS})
list(REMOVE_ITEM CFG1_LINK_OPTIONS "-Wl,--gc-sections")
target_link_options(${CFG1_OUT} PRIVATE ${CFG1_LINK_OPTIONS})
target_link_libraries(${CFG1_OUT} PRIVATE ${ASP3_LINK_LIBS})

add_custom_command(
    OUTPUT ${CFG1_OUT_SYMS_FILE}
    COMMAND ${CMAKE_NM} -n $<TARGET_FILE:${CFG1_OUT}> > ${CFG1_OUT_SYMS_FILE}
    DEPENDS ${CFG1_OUT}
    COMMENT "Generating cfg1_out.syms"
)

add_custom_command(
    OUTPUT ${CFG1_OUT_SREC_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:${CFG1_OUT}> ${CFG1_OUT_SREC_FILE}
    DEPENDS ${CFG1_OUT}
    COMMENT "Generating cfg1_out.srec"
)

#
#  パス2（-O）：offset.h の生成
#  （offset.hを使わないターゲット（POSIX等）ではスキップ）
#
if(ASP3_OFFSET_TRB_FILES)
    add_custom_command(
        OUTPUT ${OFFSET_H_FILE}
        WORKING_DIRECTORY ${CFG_GEN_DIR}
        COMMAND ${PYTHON_EXE} ${CFG_PY} --pass 2 -O --kernel asp
                ${CFG_INCLUDE_DIRS} ${CFG_OFFSET_TRB_FILES}
                --rom-symbol ${CFG1_OUT_SYMS_FILE} --rom-image ${CFG1_OUT_SREC_FILE}
        DEPENDS ${CFG_PY} ${ASP3_OFFSET_TRB_FILES}
                ${CFG1_OUT_SYMS_FILE} ${CFG1_OUT_SREC_FILE} ${CFG1_OUT_DB_FILE}
        COMMENT "Running cfg pass 2 to generate offset.h"
    )
else()
    set(OFFSET_H_FILE "")
endif()

#
#  パス2：kernel_cfg.c/h の生成
#
add_custom_command(
    OUTPUT ${CFG_GEN_H_FILES} ${CFG_GEN_C_FILES} ${CFG2_OUT_DB_FILE}
    WORKING_DIRECTORY ${CFG_GEN_DIR}
    COMMAND ${PYTHON_EXE} ${CFG_PY} --pass 2 --kernel asp
            ${CFG_INCLUDE_DIRS} ${CFG_KERNEL_CFG_TRB_FILES}
            --rom-symbol ${CFG1_OUT_SYMS_FILE} --rom-image ${CFG1_OUT_SREC_FILE}
    DEPENDS ${CFG_PY} ${ASP3_KERNEL_CFG_TRB_FILES}
            ${CFG1_OUT_SYMS_FILE} ${CFG1_OUT_SREC_FILE} ${CFG1_OUT_DB_FILE}
    COMMENT "Running cfg pass 2 to generate kernel_cfg.c/h"
)

add_custom_target(generate_cfg_gen_files
    DEPENDS ${OFFSET_H_FILE} ${CFG_GEN_H_FILES} ${CFG_GEN_C_FILES}
)

#
#  カーネルライブラリ（libasp3.a）
#
add_library(asp3 STATIC
    ${ASP3_ARCH_C_FILES}
    ${ASP3_TARGET_C_FILES}
    ${ASP3_ROOT_DIR}/kernel/alarm.c
    ${ASP3_ROOT_DIR}/kernel/cyclic.c
    ${ASP3_ROOT_DIR}/kernel/dataqueue.c
    ${ASP3_ROOT_DIR}/kernel/eventflag.c
    ${ASP3_ROOT_DIR}/kernel/exception.c
    ${ASP3_ROOT_DIR}/kernel/interrupt.c
    ${ASP3_ROOT_DIR}/kernel/mempfix.c
    ${ASP3_ROOT_DIR}/kernel/mutex.c
    ${ASP3_ROOT_DIR}/kernel/pridataq.c
    ${ASP3_ROOT_DIR}/kernel/semaphore.c
    ${ASP3_ROOT_DIR}/kernel/startup.c
    ${ASP3_ROOT_DIR}/kernel/sys_manage.c
    ${ASP3_ROOT_DIR}/kernel/task.c
    ${ASP3_ROOT_DIR}/kernel/task_manage.c
    ${ASP3_ROOT_DIR}/kernel/task_refer.c
    ${ASP3_ROOT_DIR}/kernel/task_sync.c
    ${ASP3_ROOT_DIR}/kernel/task_term.c
    ${ASP3_ROOT_DIR}/kernel/taskhook.c
    ${ASP3_ROOT_DIR}/kernel/time_event.c
    ${ASP3_ROOT_DIR}/kernel/time_manage.c
    ${ASP3_ROOT_DIR}/kernel/wait.c
    ${CFG_GEN_C_FILES}
)
add_dependencies(asp3 generate_cfg_gen_files)

target_include_directories(asp3
    PUBLIC ${ASP3_INCLUDE_DIRS} ${ASP3_APP_INCLUDE_DIRS}
    PRIVATE ${ASP3_ROOT_DIR}/kernel
)
target_compile_definitions(asp3
    PRIVATE ALLFUNC
    PUBLIC ${ASP3_COMPILE_DEFS}
)
target_compile_options(asp3 PUBLIC ${ASP3_COMPILE_OPTIONS})

#
#  パス3：構成チェック（POST_BUILD）
#
list(APPEND PASS3_ARGS ${PYTHON_EXE} ${CFG_PY} --pass 3 --kernel asp -O)
list(APPEND PASS3_ARGS ${CFG_INCLUDE_DIRS} ${CFG_CHECK_TRB_FILES})

function(asp3_cfg_check TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        WORKING_DIRECTORY ${CFG_GEN_DIR}
        COMMAND ${CMAKE_NM} -n $<TARGET_FILE:${TARGET}> > ${TARGET}.syms
        COMMAND ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:${TARGET}> ${TARGET}.srec
        COMMAND ${PASS3_ARGS} --rom-symbol ${TARGET}.syms --rom-image ${TARGET}.srec
        COMMENT "Running cfg pass 3 to check configuration"
    )
endfunction()

#
#  以下，asp 実行ファイル・実行/デバッグターゲット・ctest登録は
#  ライブラリ専用モード（ASP3_LIBRARY_ONLY=ON）では作らない．外部SDK側が
#  asp3 ライブラリをリンクして自前の実行ファイルを作るため（カーネル
#  ライブラリ asp3・cfg生成・ヘルパ関数は上で定義済みで，こちらで公開する）．
#
if(NOT ASP3_LIBRARY_ONLY)

#
#  サンプルアプリケーション（asp）
#
#  アプリのCソースは存在する場合のみ加える（cfg段階でエラーになる
#  ことを期待するテスト（.cfgのみ）に対応．Makefile版ではcfgエラーで
#  停止するためソース不在に到達しないのと等価）
set(ASP3_APP_MAIN_SRC ${ASP3_APPLDIR}/${ASP3_APPLNAME}.c)
if(NOT EXISTS ${ASP3_APP_MAIN_SRC})
    set(ASP3_APP_MAIN_SRC "")
endif()
add_executable(asp
    ${ASP3_START_FILES}
    ${ASP3_APP_MAIN_SRC}
    ${ASP3_EXTRA_APP_C_FILES}
)
#  ASP3_OMIT_DEFAULT_SYSSVC=ON でシステムサービスを組み込まない
#  （cfgテスト等の素のカーネルビルド用）
if(NOT ASP3_OMIT_DEFAULT_SYSSVC)
    asp3_add_syssvc(asp)
endif()
target_include_directories(asp
    PRIVATE ${ASP3_ROOT_DIR}/kernel    # ASP3_START_FILES（start.S）用
)
target_link_libraries(asp PRIVATE asp3 ${ASP3_LINK_LIBS})
target_link_options(asp PRIVATE ${ASP3_LINK_OPTIONS})
#  【SAFEG】CMSE Secure import library(secure_nsclib.o)を最終ELFリンク時に生成．
#  Non-secure 側ビルドが Secure gate veneer を参照するために必要．
#  cfg1_out(補助ELF)には付けない（cfg1_outは gate veneer を持たずリンク失敗するため）．
#  implib は Secure gate(cmse_nonsecure_entry)を持つアプリ(test 等)でのみ生成可
#  （gate が無いと ld が "no symbols" で失敗する）。gate を持つアプリのビルドで
#  -DENABLE_SAFEG_IMPLIB=ON を指定する。素のサンプル(gate 無)では OFF のまま。
option(ENABLE_SAFEG_IMPLIB "Generate CMSE import lib (app must have cmse_nonsecure_entry)" OFF)
if(ENABLE_SAFEG_M AND ENABLE_SAFEG_IMPLIB)
    set(SAFEG_NSCLIB ${CMAKE_BINARY_DIR}/secure_nsclib.o)
    target_link_options(asp PRIVATE -Wl,--cmse-implib,--out-implib=${SAFEG_NSCLIB})
    message(STATUS "[SAFEG] CMSE import lib -> ${SAFEG_NSCLIB}")
endif()
#  リンカスクリプトの変更で再リンクされるようにする（-Wl,-Tは依存に
#  ならないため．忘れると.ld編集が反映されない）
if(DEFINED ASP3_LDSCRIPT)
    set_property(TARGET asp APPEND PROPERTY LINK_DEPENDS ${ASP3_LDSCRIPT})
    set_property(TARGET ${CFG1_OUT} APPEND PROPERTY LINK_DEPENDS ${ASP3_LDSCRIPT})
endif()
asp3_cfg_check(asp)

#
#  実行ターゲット（QEMU等．target.cmakeがASP3_RUN_COMMANDを定義した場合）
#
if(DEFINED ASP3_RUN_COMMAND)
    add_custom_target(run
        COMMAND ${ASP3_RUN_COMMAND}
        DEPENDS asp
        USES_TERMINAL
        COMMENT "Running ${ASP3_TARGET}"
    )

    #
    #  OS-awareness付きQEMUデバッグ（cmake --build <dir> --target osdebug）
    #
    #  実行コマンドがQEMUのターゲットでのみ定義する（linux=ネイティブ実行や
    #  pico2=picotool書込みは対象外）．QEMUを -s -S（gdbserver :1234・起動時
    #  停止）で起動し，gdb-multiarchで接続してOS-awarenessエンジン
    #  （scripts/gdb_os_aware/os_awareness.py）を読み込む．エンジンはELF横の
    #  CMakeCache.txtからターゲット依存部（target_os_awareness.py）を自動発見
    #  する．gdb内: continue →（実行）→ Ctrl-C でhalt → atask / stask /
    #  sem / intr 等．QEMUの出力はビルドディレクトリの qemu-osdebug.log へ．
    #  実機SWD用のosdebug（stm32）はtarget/<name>/run.cmake側で定義される．
    #
    string(JOIN " " _ASP3_RUN_COMMAND_STR ${ASP3_RUN_COMMAND})
    if(_ASP3_RUN_COMMAND_STR MATCHES "qemu-system")
        set(OSGDB gdb-multiarch CACHE STRING "gdb for OS-awareness (osdebug)")
        #  gdbの起動引数（ELF指定＋接続コマンド）．マルチクラスタ構成のマシン
        #  （polarfire=icicle-kit等）はtarget.cmakeがASP3_OSDEBUG_GDB_CONNECT
        #  を上書きして接続手順を差し替える．
        if(NOT DEFINED ASP3_OSDEBUG_GDB_CONNECT)
            set(ASP3_OSDEBUG_GDB_CONNECT
                "$<TARGET_FILE:asp> -ex 'target remote :1234'")
        endif()
        add_custom_target(osdebug
            COMMAND bash -c "command -v ${OSGDB} >/dev/null 2>&1 || { echo '${OSGDB} not found (sudo apt-get install -y gdb-multiarch)'; exit 1; }; echo '[osdebug] QEMU output: qemu-osdebug.log'; echo '[osdebug] in gdb: continue -> (run) -> Ctrl-C -> atask / stask'; ${_ASP3_RUN_COMMAND_STR} -s -S < /dev/null > qemu-osdebug.log 2>&1 & QEMU_PID=$!; trap 'kill $QEMU_PID 2>/dev/null' EXIT INT TERM; sleep 1; ${OSGDB} ${ASP3_OSDEBUG_GDB_CONNECT} -ex 'source ${ASP3_ROOT_DIR}/scripts/gdb_os_aware/os_awareness.py'"
            DEPENDS asp
            USES_TERMINAL
            VERBATIM
            COMMENT "QEMU (-s -S) + ${OSGDB} with OS-awareness"
        )
    endif()
endif()

#
#  ctest（POSIX CLIターゲットのスモークテスト．経緯はdocs/dev/cli-target.md）
#
#  ホスト実行可能なlinux_gccかつ既定アプリ（sample1）のビルドでのみ登録
#  する．`ctest --preset linux` で実行できる．機能テスト群（TAP出力）の
#  一括実行は test/testexec.py を使用する．
#
if(ASP3_TARGET STREQUAL "linux_gcc" AND ASP3_APPLNAME STREQUAL "sample1")
    enable_testing()

    #  sample1が起動してタスクが回ることの確認（出力一致で合否判定）
    add_test(NAME sample1.run
        COMMAND bash -c "timeout 5 $<TARGET_FILE:asp> 2>&1 | head -40"
    )
    set_tests_properties(sample1.run PROPERTIES
        PASS_REGULAR_EXPRESSION "task1 is running"
        TIMEOUT 30
    )

    #  構造化ログ（--slog）の期待イベント列照合（AGENTS.md §8のフロー）
    if(ASP3_ENABLE_TRACE)
        add_test(NAME sample1.slog_check
            COMMAND bash -c "timeout 5 $<TARGET_FILE:asp> --slog 2>&1 \
                | ${Python3_EXECUTABLE} ${ASP3_ROOT_DIR}/scripts/parse_slog.py --json \
                > sample1_actual.jsonl; \
                ${Python3_EXECUTABLE} ${ASP3_ROOT_DIR}/scripts/check_events.py \
                ${ASP3_ROOT_DIR}/test/porting/expected/sample1.json sample1_actual.jsonl"
        )
        set_tests_properties(sample1.slog_check PROPERTIES TIMEOUT 30)
    endif()
endif()

#
#  ctest（移植検証テスト test/porting．経緯はdocs/dev/porting-test.md）
#
#  ホスト実行可能なlinux_gccでのみ登録する．合否はTAPサマリ行
#  （"# 8/8 passed"）の出力照合で判定する（全ターゲット共通の判定方式を
#  ctestに直結したもの）．
#
if(ASP3_TARGET STREQUAL "linux_gcc" AND ASP3_APPLNAME STREQUAL "test_porting")
    enable_testing()

    add_test(NAME test_porting.run
        COMMAND bash -c "timeout 10 $<TARGET_FILE:asp> 2>&1"
    )
    set_tests_properties(test_porting.run PROPERTIES
        PASS_REGULAR_EXPRESSION "# 8/8 passed"
        FAIL_REGULAR_EXPRESSION "not ok"
        TIMEOUT 30
    )
endif()

#
#  ターゲット固有の実行・デバッグターゲット（openocd/gdb/swd-run等）
#  target.cmake が ASP3_TARGET_RUN_CMAKE を定義した場合に取り込む
#  （aspターゲット定義後にincludeする必要があるためここで行う）
#
if(DEFINED ASP3_TARGET_RUN_CMAKE)
    include(${ASP3_TARGET_RUN_CMAKE})
endif()

endif()  # NOT ASP3_LIBRARY_ONLY
