/*
 *  ESP32-C6用リンカスクリプト（Direct Boot・XIP実行）
 *
 *  esp32c3.ld（VMA==LMAトリック＝IROM/DROM分離への対処）からの移植だが，
 *  C6はSOC_DROM_LOW＝SOC_IROM_LOW（共に0x42000000）＝IROM/DROMが分離
 *  していない（esp-hal-3rdparty soc/esp32c6/include/soc/soc.hで確認）．
 *  そのためC3のような複数セクション・複数ORIGINへの分割は不要で，
 *  .text／.rodataとも単一のFLASH領域に素直に配置できる（cfg pass1の
 *  シンボルVMAによるsrec/LMA参照も，単一領域のため自然に整合する）．
 *
 *  RAMはROMブートローダの作業領域を避けて512KB中の一部に制限する
 *  （esp32c3.ldの320KB制限と同じ考え方．具体的な安全マージンは実機
 *  test_porting/testexecの動作実績で検証する）．
 */

OUTPUT_ARCH(riscv)

MEMORY
{
    FLASH(rx) : ORIGIN = 0x42000000, LENGTH = 4096k
    RAM(rwx)  : ORIGIN = 0x40800000, LENGTH = 448k
}

ENTRY(_flash_entry)

SECTIONS
{
    /*
     *  Direct Bootマジック（+0）・エントリ（+8）・コード・rodataを
     *  単一のFLASH領域にまとめる（IROM/DROM分離が無いため区別不要）．
     */
    .text : {
        KEEP(*(.flash_header))     /* +0：マジックナンバー（8バイト） */
        KEEP(*(.text.entry))       /* +8：エントリ（j start） */
        . = ALIGN(256);            /* trap_vector_tableのアライン */
        *(.text*)
        . = ALIGN(16);
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
        . = ALIGN(16);
    } > FLASH

    /*
     *  Main DATA section (RAM)
     *
     *  LMAは.textの直後（FLASH基準）を明示する（.dataが空のときに
     *  LMAがVMA（RAM）へ落ちてフラッシュイメージが巨大化するのを
     *  防ぐ．esp32c3.ldと同じ対策）．
     */
    .data : AT(LOADADDR(.text) + SIZEOF(.text)) ALIGN(8) {
        __data_start = .;
        LONG(0)     /* 番兵（.dataが空だとldがAT指定を無視しLMAがRAMに落ちるため） */
        *(.data*)
        . = ALIGN(8);
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.*)
        . = ALIGN(4);
    } > RAM
    /*
     *  ROM化コピー元はFLASH側のアドレスで取る（start.Sはデータバス
     *  経由で読み出す．IROM/DROMが同一アドレス空間のためそのまま読める）
     */
    __idata_start = LOADADDR(.data);
    __idata_end = __idata_start + SIZEOF(.data);

    /* MAIN BSS SECTION */
    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM
}
