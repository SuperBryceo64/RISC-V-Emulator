start:
    # push some data to the stack
    li a2, 0x4
    li a3, 0xCAFEBABE
    ecall
    # allocate some bytes to heap
    la s0, num_bytes
    li a2, 0x6
1:  lw a3, s0
    ecall
    ebreak
    addi s0, s0, 4
    beqz a0, 1b
    j end

end:
    ebreak
    # terminate program
    li a2 0x1
    ecall

num_bytes: .word 32, 2, 1, -1