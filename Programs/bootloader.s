bootloader:    
    sb zero, 1(zero)  # initialize interrupt flags to zero
    li sp, 0xE0000800  # initialize stack pointer
    li ra, 0x800  # initialize return address to start of main program
    jr ra  # jump to main program