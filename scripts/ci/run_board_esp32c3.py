#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#		testexec.py のTARGET_RUN用ボードランナ（ESP32-C3用）
#
#  ESP32-C3実機にesptoolでフラッシュイメージ（asp_flash.bin＝Direct
#  Boot形式）を書き込み，USB Serial/JTAGコンソール（/dev/ttyACM*）の
#  出力を完走マーカ（またはタイムアウト）まで取得して標準出力へ流す．
#  カレントディレクトリ＝OBJディレクトリ（asp_flash.binがある）で実行
#  される．
#
#  書込み後にRTS（USB Serial/JTAG経由のチップリセット）で再起動して
#  から取得する（esptool終了時のリセットでは出力の先頭を取りこぼす
#  ため）．ポートを開くのは1プロセスのみ（cat併用不可）．
#
#  使い方: run_board_esp32c3.py [タイムアウト秒]
#  環境変数: ESP32C3_TTY（既定 /dev/ttyACM0）
#            ESPTOOL（既定 esptool＝PATHから）
#
import os
import re
import subprocess
import sys
import time

import serial

#  完走マーカ（scripts/ci/run_board_pico2.sh と同じ判定）
FINISH_RE = re.compile(
    rb"All check points passed\.|^## |"
    rb"high resolution timer count test finishes\.|"
    rb"-- for checking boundary conditions --|"
    rb"This test program is not necessary\.|Unregistered|"
    rb"# \d+/\d+ passed",
    re.M)


def main():
    tmo = float(sys.argv[1]) if len(sys.argv) > 1 else 120.0
    port = os.environ.get("ESP32C3_TTY", "/dev/ttyACM0")
    esptool = os.environ.get("ESPTOOL", "esptool")

    #  フラッシュ書込み（esptoolが終了時にハードリセットする）
    ret = subprocess.run(
        [esptool, "--chip", "esp32c3", "--port", port,
         "write-flash", "0x0", "asp_flash.bin"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if ret.returncode != 0:
        sys.stdout.buffer.write(ret.stdout)
        print("## esptool write-flash failed")
        sys.exit(1)

    #  リセットして出力を取得する
    p = serial.Serial(port, 115200, timeout=0.5)
    try:
        p.reset_input_buffer()
        p.dtr = False
        p.rts = True       # チップリセット（アサート）
        time.sleep(0.2)
        p.rts = False      # リセット解除→ブート
        buf = b""
        end = time.time() + tmo
        while time.time() < end:
            d = p.read(4096)
            if d:
                buf += d
            if FINISH_RE.search(buf):
                #  残りの出力（dlynseの境界測定など）のため少し待つ
                time.sleep(3.0)
                buf += p.read(65536)
                break
    finally:
        p.close()
    sys.stdout.buffer.write(buf.replace(b"\r\n", b"\n"))
    sys.stdout.flush()


if __name__ == "__main__":
    main()
