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
    ldr r2, =0x400              ;@ size of code = 1024 bytes

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

.thumb_func
reset:
    bl segled
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

//-------------------------------------------------------------------------
//--- Original program ---
// 
// Copyright (c) 2021 David Welch dwelch@dwelch.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------
//
// Modified program
// Copyright (c) 2025 grekichi
//
