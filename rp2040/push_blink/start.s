.cpu cortex-m0
.thumb

    ldr r0, =0x20001000
    mov sp, r0
    bl push_blink
    b .

.thumb_func
.global PUT32
PUT32:
    str r1, [r0]
    bx lr

.thumb_func
.global GET32
GET32:
    ldr r0, [r0]
    bx lr

.global DELAY
.thumb_func
DELAY:
    sub r0, #1
    bne DELAY
    bx lr

