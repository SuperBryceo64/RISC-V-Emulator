# The requested service is given by register a2
# Any other parameters ECALL may need are given by registers a3, a4, etc.
# ECALL returns its success state in register a0; 0 indicates success and a non-zero
#   number indicates an error code
handle_interrupt:
    beqz a2, halt
    li a0, 0x1
    beq a2, a0, terminate_program
    li a0, 0x2
    beq a2, a0, restart_program
    li a0, 0x3
    beq a2, a0, print_cpu_info
    li a0, 0x4
    beq a2, a0, push_stack
    li a0, 0x5
    beq a2, a0, pop_stack
    li a0, 0x6
    beq a2, a0, allocate_heap
    li a0, 0x7
    beq a2, a0, free_heap
    li a0, 0x8
    beq a2, a0, read_char
    li a0, 0x9
    beq a2, a0, write_char
    li a0, -0x1
    j return

# 0
halt:
    j return

# 1
terminate_program:
    lb a1, 1(zero)
    ori a1, a1, 0b01000000
    mv a0, zero
    sb a1, 1(zero)  # program will end once this line is executed

# 2
restart_program:
    lb a1, 1(zero)
    ori a1, a1, 0b10000000
    mv a0, zero
    sb a1, 1(zero)  # program will restart once this line is executed

# 3
print_cpu_info:
    j return

# 4
push_stack:
    # check if cpu is 32-bit or 64-bit
    li a0, 0x400
    slli a0, a0, 22
    bnez a0, 1f
    # 32-bit
    addi sp, sp, -4
    sw a3, 0(sp)
    j 2f
    # 64-bit
1:  addi sp, sp, -8
    sd a3, 0(sp)
    mv a0, zero
2:  j return

# 5
pop_stack:
    # check for an empty stack
    li a0, 0xE0000800
    sub a0, a0, sp
    seqz a0, a0
    bnez a0, 2f
    # check if cpu is 32-bit or 64-bit
    li a0, 0x400
    slli a0, a0, 22
    bnez a0, 1f
    # 32-bit
    lw a1, 0(sp)
    addi sp, sp, 4
    j 2f
    # 64-bit
1:  ld a1, 0(sp)
    addi sp, sp, 8
    mv a0, zero
2:  j return

# 6
allocate_heap:
    # ** the first 178,956,970 bytes of the heap will be reserved for keeping track of which byte is allocated
    # ** this leaves a maximum heap size of 1,431,655,760 bytes = 1.33 GB (assuming stack doesn't use that space)
    # Set t4 to XLEN
    li a0, 0x400
    slli a0, a0, 22
    bnez a0, 1f
    li t4, 32
    j 2f
1:  li t4, 64
    j 2f
    # Check if requesting zero bytes
2:  seqz a0, a3
    bnez a0, 10f
    # Check if requesting negative # of bytes
    sltz a0, a3
    bnez a0, 10f
    # Check if requesting more than XLEN bytes
    sub a0, a3, t4
    sgtz a0, a0
    bnez a0, 10f
    # Initialize variables
    li a0, 0x80000800  # starting address of allocation table
    li a1, 0x8AAAB2A8  # starting address of heap (0x80000800 + 178,956,968)
    mv t1, zero        # initialize consecutive zeros counter
    mv t2, zero        # initialize bit pos / offset variable
    sltiu t5, t4, 64   # t5 stores a flag to indicate whether XLEN is 32 (1) or 64 (0)
    # Count number of consecutive zeros
3:  lw t0, a0          # load current word of allocation table
    bnez t5, 4f        # skip loading double word if XLEN is 32
    ld t0, a0          # load current double word of allocation table if XLEN is 64
4:  andi t3, t0, 1     # get bit value at the current bit pos
    bnez t3, 5f        # finish count if we reach an allocated byte
    addi t1, t1, 1     # increment counter
    srli t0, t0, 1     # shift to next bit
    addi t2, t2, 1     # increment bit pos
    bltu t2, t4, 4b    # loop if bit pos is < XLEN
    # Check if requested # of bytes fits in block
5:  bgeu t1, a3, 6f    # check if requested # of bytes fits in block
    # otherwise, check next block
    addi t2, t2, 1     # increment bit pos
    srli t0, t0, 1     # shift to next bit
    mv t1, zero        # clear consecutive zeros counter
    bltu t2, t4, 4b    # continue checking rest of allocation table
    # Search through the next allocation table
    srli t5, t4, 3     # t5 will temporarily store XLEN / 8 to increment allocation table pointer
    add a0, a0, t5     # go to next block of allocation table
    add a1, a1, t4     # go to next potential base heap address
    mv t1, zero        # clear consecutive zeros counter
    mv t2, zero        # reset bit pos / offset variable
    sltiu t5, t4, 64   # t5 is back to storing XLEN flag
    j 3b
    # Return heap address and set allocation table once block is found
6:  sub t2, t2, t1     # offset should be bit pos - count
    add a1, a1, t2     # heap address should be the base heap address + offset
    add t5, a1, a3     # t5 will temporarily store the first address after the allocated block
    mv t0, a0          # t0 will temporarily store the allocation table pointer stored by a0
    sltu t5, sp, t5    # t5 will indicate if heap address resides in stack space
    slli a0, t5, 1     # an error code of 2 is returned if heap address resides in stack space
    bnez a0, 10f       # jump to fail condition if heap address resides in stack space
    sltiu t5, t4, 64   # t5 is back to storing XLEN flag
    mv a0, t0          # a0 is back to storing allocation table pointer
    mv t1, a3          # counter is now set to # of bytes to allocate
    li t3, 1           # t3 now stores bitmask
    addi t1, t1, -1    # decrement counter
    lw t0, a0          # reload allocation table (as a word)
    bnez t5, 7f        # skip loading double word if XLEN is 32
    ld t0, a0          # reload allocation table as a double word if XLEN is 64
7:  beqz t1, 8f        # stop adding 1's when counter is zero
    slli t3, t3, 1     # generate 1's while counter is positive
    ori t3, t3, 1
    addi t1, t1, -1    # decrement counter
    j 7b
8:  sll t3, t3, t2     # shift bitmask by offset to generate final bitmask
    or t0, t0, t3      # apply bitmask to allocation table to set bits
    sw t0, a0          # store new allocation table (as a word) in memory
    bnez t5, 9f        # skip storing double word if XLEN is 32
    sd t0, a0          # store new allocation table as a double word in memory
    # Return success code
9:  mv a0, zero  # a returned status code of 0 indicates a success
    j 11f
10: mv a1, zero  # the returned heap address should be zero if a fail occurs
    # Clean up temporaries
11: mv t0, zero
    mv t1, zero
    mv t2, zero
    mv t3, zero
    mv t4, zero
    mv t5, zero
    j return

# 7
free_heap:
    j return

# 8
read_char:
    j return

# 9
write_char:
    j return

# interrupt handler must always end in a call to ECALL to return back to user program
return:
    ecall