// file: neon.S
.text
.global add_one_u32
.type add_one_u32, %function
add_one_u32:
    movi v1.4s, #1
    cmp x2, #4
    b.lo .Ltail
.Lvector:
    ld1 {v0.4s}, [x0], #16
    add v0.4s, v0.4s, v1.4s
    st1 {v0.4s}, [x1], #16
    sub x2, x2, #4
    cmp x2, #4
    b.hs .Lvector
.Ltail:
    cbz x2, .Ldone
.Lscalar:
    ldr w3, [x0], #4
    add w3, w3, #1
    str w3, [x1], #4
    subs x2, x2, #1
    b.ne .Lscalar
.Ldone:
    ret
.size add_one_u32, .-add_one_u32
.section .note.GNU-stack,"",%progbits
