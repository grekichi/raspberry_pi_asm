MEMORY
{
    flash : ORIGIN = 0x10000000, LENGTH = 0x1000
}
SECTIONS
{
    .text   : { *(.text*)    } > flash
}
