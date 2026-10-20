// file: addresses.S
.section .rodata
.p2align 3
local_value:
    .quad 42

.text
.global value_address
.type value_address, %function
value_address:
    adrp x0, local_value
    add x0, x0, :lo12:local_value
    ret
.size value_address, .-value_address

.global read_value
.type read_value, %function
read_value:
    adrp x0, local_value
    add x0, x0, :lo12:local_value
    ldr x0, [x0]
    ret
.size read_value, .-read_value

.global literal_value
.type literal_value, %function
literal_value:
    ldr x0, local_value
    ret
.size literal_value, .-literal_value
.section .note.GNU-stack,"",%progbits
