// file: memory.S
.text
.global sum_i32
.type sum_i32, %function
sum_i32:
    mov x2, #0
    cbz x1, .Ldone
.Lloop:
    ldrsw x3, [x0], #4
    add x2, x2, x3
    subs x1, x1, #1
    b.ne .Lloop
.Ldone:
    mov x0, x2
    ret
.size sum_i32, .-sum_i32

.global zero_word
.type zero_word, %function
zero_word:
    ldr w0, [x0]
    ret
.size zero_word, .-zero_word
.section .note.GNU-stack,"",%progbits
