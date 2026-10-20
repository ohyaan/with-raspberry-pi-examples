// file: report.S
.text
.global summarize_i32
.type summarize_i32, %function
summarize_i32:
    mov x3, #0                 // sum
    mov x4, #0                 // negative count
    cbz x1, .Ldone
.Lloop:
    ldrsw x5, [x0], #4
    add x3, x3, x5
    cmp x5, #0
    cset x6, lt
    add x4, x4, x6
    subs x1, x1, #1
    b.ne .Lloop
.Ldone:
    str x4, [x2]
    mov x0, x3
    ret
.size summarize_i32, .-summarize_i32
.section .note.GNU-stack,"",%progbits
