/* Nuts & Bolts 64 mod image, loaded at the start of Expansion Pak RAM. */
OUTPUT_ARCH(mips)
ENTRY(nbHeader)

SECTIONS
{
    . = 0x80400000;
    .text : {
        KEEP(*(.nbheader))
        *(.text .text.*)
    }
    .rodata : ALIGN(8) {
        *(.rodata .rodata.* .rodata1)
    }
    .data : ALIGN(8) {
        *(.data .data.* .sdata .sdata.*)
        . = ALIGN(8);
    }
    __load_end = .;
    __load_size = __load_end - 0x80400000;

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.* .sbss .sbss.* COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }
    ASSERT(__bss_end < 0x80800000, "mod does not fit in Expansion Pak RAM")

    /DISCARD/ : {
        *(.MIPS.*) *(.comment) *(.mdebug*) *(.pdr) *(.reginfo) *(.eh_frame) *(.gnu.attributes) *(.note*)
    }
}
