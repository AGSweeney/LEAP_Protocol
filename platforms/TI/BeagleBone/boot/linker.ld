ENTRY(_start)

MEMORY
{
    DDR (rwx) : ORIGIN = 0x80000000, LENGTH = 16M
}

SECTIONS
{
    .text : {
        KEEP(*(.vectors))
        KEEP(*(.text.start))
        *(.text*)
        *(.rodata*)
    } > DDR

    .data : {
        *(.data*)
    } > DDR

    .bss : {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
    } > DDR

    . = ALIGN(8);
    . = ORIGIN(DDR) + LENGTH(DDR);
    _stack_top = .;
}
