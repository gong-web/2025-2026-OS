/* Simple linker script for the ucore kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

BASE_ADDRESS = 0xFFFFFFFFC0200000;

SECTIONS
{
    /* Load the kernel at this address: "." means the current address */
    . = 0x80200000;

    .text.kern_entry : {
        *(.text.kern_entry)
    }

    . = BASE_ADDRESS + SIZEOF(.text.kern_entry);

    .text : AT(ADDR(.text) - 0xFFFFFFFF40000000) {
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }

    PROVIDE(etext = .); /* Define the 'etext' symbol to this value */

    .rodata : AT(ADDR(.rodata) - 0xFFFFFFFF40000000) {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* Adjust the address for the data segment to the next page */
    . = ALIGN(0x1000);

    /* The data segment */
    .data : AT(ADDR(.data) - 0xFFFFFFFF40000000) {
        *(.data)
        *(.data.*)
    }

    .sdata : AT(ADDR(.sdata) - 0xFFFFFFFF40000000) {
        *(.sdata)
        *(.sdata.*)
    }

    PROVIDE(edata = .);

    .bss : AT(ADDR(.bss) - 0xFFFFFFFF40000000) {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

    PROVIDE(end = .);

    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
}
