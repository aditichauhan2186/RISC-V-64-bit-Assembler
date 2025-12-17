.text
    lui x5, 0x10001     # U-Format: x5 = 0x10001000
    auipc x6, 0x0       # U-Format: x6 = pc (0x4)
    
    # x5 now holds 0x10001000
    addi x7, x5, 12     # I-Format (Arith): x7 = 0x10001000 + 12
    lw x8, 12(x5)       # I-Format (Load): x8 = mem[0x10001000 + 12]
    sw x8, 16(x5)       # S-Format (Store): mem[0x10001000 + 16] = x8

    # Branch test
    blt x0, x0, loop    # SB-Format: should not branch

loop:
    addw x9, x9, x0     # R-Format (addw)
    jal x0, loop        # UJ-Format