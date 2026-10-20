// file: interop.S
.text
.global add_u64
.type add_u64, %function
add_u64:
    add x0, x0, x1
    ret
.size add_u64, .-add_u64
.section .note.GNU-stack,"",%progbits
