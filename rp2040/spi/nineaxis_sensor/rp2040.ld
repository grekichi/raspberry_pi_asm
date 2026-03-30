MEMORY
{
    ram : ORIGIN = 0x20000000, LENGTH = 0x3000
}
SECTIONS
{
    .text   : { *(.text*)    } > ram
}
