// file: registers.S
.text
.global width_demo
.type width_demo, %function
width_demo:
    mov x0, #-1
    mov w0, #0x1234
    ret
.size width_demo, .-width_demo

.global constant_demo
.type constant_demo, %function
constant_demo:
    movz x0, #0xdef0
    movk x0, #0x9abc, lsl #16
    movk x0, #0x5678, lsl #32
    movk x0, #0x1234, lsl #48
    ret
.size constant_demo, .-constant_demo

.global scaled_sum
.type scaled_sum, %function
scaled_sum:
    add x0, x0, x1
    mul x0, x0, x2
    ret
.size scaled_sum, .-scaled_sum
.section .note.GNU-stack,"",%progbits
