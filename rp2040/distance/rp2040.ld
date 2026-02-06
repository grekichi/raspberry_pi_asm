MEMORY
{
    ram : ORIGIN = 0x20000000, LENGTH = 0x2500
}
SECTIONS
{
    .text   : { *(.text*)    } > ram
}
