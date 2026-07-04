MEMORY
{
    FLASH (rx) : ORIGIN = 0x42000000, LENGTH = 0x100000
    DRAM  (rwx): ORIGIN = 0x40800000, LENGTH = 0x80000
}

ENTRY(_flash_entry)

SECTIONS
{
    .flash_header : { *(.flash_header) } > FLASH

    .text : {
        *(.text.entry)
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > FLASH

    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    } > DRAM

    . = ORIGIN(DRAM) + LENGTH(DRAM);
    _stack_top = .;
}
