// file: functions.S
.text
.global double_value
.type double_value, %function
double_value:
    add x0, x0, x0
    mov x1, #0                 // permitted caller-clobber
    ret
.size double_value, .-double_value

.global double_plus
.type double_plus, %function
double_plus:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x19, [sp, #16]
    mov x19, x1
    bl double_value
    add x0, x0, x19
    ldr x19, [sp, #16]
    ldp x29, x30, [sp], #32
    ret
.size double_plus, .-double_plus
.section .note.GNU-stack,"",%progbits
