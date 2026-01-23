.cpu cortex-m0plus
.thumb

XIP_SSI_BASE        = 0x18000000
XIP_SSI_CTRLR0      = XIP_SSI_BASE + 0x00
XIP_SSI_CTRLR1      = XIP_SSI_BASE + 0x04
XIP_SSI_SSIENR      = XIP_SSI_BASE + 0x08
XIP_SSI_BAUDR       = XIP_SSI_BASE + 0x14
XIP_SSI_SPI_CTRLR0  = XIP_SSI_BASE + 0xF4


    ldr r0, =XIP_SSI_SSIENR     ;@ SSI Enable
    ldr r1, =0x00000000         ;@ reset
    str r1, [r0]

    ldr r0, =XIP_SSI_BAUDR      ;@ Baud rate
    ldr r1, =0x00000008         ;@ set 1000
    str r1, [r0]

    ldr r0, =XIP_SSI_CTRLR0     ;@ Control register 0
    ldr r1, =0x001F0300         ;@ SPI frame format -> Dual == 2 bits per SCK
    str r1, [r0]

    ldr r0, =XIP_SSI_SPI_CTRLR0 ;@ SPI control
    ldr r1, =0x03000218         ;@ reset, 8-bit instruction, address lenght -> 6 x 4 = 12bits
    str r1, [r0]

    ldr r0, =XIP_SSI_CTRLR1     ;@ Master control register
    ldr r1, =0x00000000         ;@ Number of data frames -> reset
    str r1, [r0]

    ldr r0, =XIP_SSI_SSIENR     ;@ SSI enable
    ldr r1, =0x00000001         ;@ set
    str r1, [r0]

    ldr r0, =0x10000000         ;@ Source address (FLASH)
    ldr r1, =0x20000000         ;@ Destination (SRAM)
    ldr r2, =0x2000              ;@ size of code = 4096 bytes

copy_loop:
    ldr r3, [r0]
    str r3, [r1]
    add r0, #0x4
    add r1, #0x4
    sub r2, #1
    bne copy_loop

    ldr r0,=0x20000101
    bx r0

pool0:
.align
.ltorg 

;@ ------------------------------------------
.balign 0x100

.global vector_table
vector_table:
.thumb_func
.global centry
centry:
    b reset
    .balign 4
    .word reset  ;@ has to be offset 4
    .word loop
    .word loop

    .word loop
    .word loop
    .word loop
    .word loop

    .word loop
    .word loop
    .word loop
    .word loop

    .word loop
    .word loop
    .word loop
    .word Systick_Handler

.thumb_func
reset:
    ldr r1, =0xE000ED08  ;@ VTOR
    ldr r0, =vector_table
    str r0, [r1]

    ldr r0, =0x20002000
    mov sp, r0
    bl temp_humid_m
    b loop

.thumb_func
loop:
    b loop

;@ ------------------------------------------
.thumb_func
.globl PUT32
PUT32:
    str r1,[r0]
    bx lr

.thumb_func
.globl GET32
GET32:
    ldr r0,[r0]
    bx lr

.globl DELAY
.thumb_func
DELAY:
    sub r0,#1
    bne DELAY
    bx lr

.thumb_func
.global WFI
WFI:
    wfi     ;@ wait for interrupt : 割り込みが発生するまで、低電力モードを維持できる指示
    bx lr

