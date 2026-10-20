// file: hello.S
.section .rodata
message:
    .ascii "Hello, AArch64!\n"
.equ message_length, . - message

.text
.global _start
.type _start, %function
_start:
    mov x0, #1                  // stdout file descriptor
    adr x1, message             // address, not the message bytes
    mov x2, #message_length     // byte count
    mov x8, #64                 // Linux AArch64 write
    svc #0
    mov x9, #message_length
    cmp x0, x9
    b.ne .Lfailed               // short/failed write is a failure here
    mov x0, #0
    b .Lexit
.Lfailed:
    mov x0, #1
.Lexit:
    mov x8, #93                 // Linux AArch64 exit
    svc #0
.size _start, .-_start
.section .note.GNU-stack,"",%progbits
