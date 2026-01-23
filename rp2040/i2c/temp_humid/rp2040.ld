MEMORY
{
    ram : ORIGIN = 0x20000000, LENGTH = 0x2000
}
SECTIONS
{
    .text   : { *(.text*)    } > ram
}
