// file: branches.S
.text
.global sum_to_n
.type sum_to_n, %function
sum_to_n:
    mov x1, #0
    cbz x0, .Ldone
.Lloop:
    add x1, x1, x0
    subs x0, x0, #1
    b.ne .Lloop
.Ldone:
    mov x0, x1
    ret
.size sum_to_n, .-sum_to_n

.global signed_less
.type signed_less, %function
signed_less:
    cmp x0, x1
    cset w0, lt
    ret
.size signed_less, .-signed_less

.global unsigned_less
.type unsigned_less, %function
unsigned_less:
    cmp x0, x1
    cset w0, lo
    ret
.size unsigned_less, .-unsigned_less
.section .note.GNU-stack,"",%progbits
