#ifndef DECODE_AND_ENCODE_INSTRUCTION_FROM_FORMAT_H
#define DECODE_AND_ENCODE_INSTRUCTION_FROM_FORMAT_H

#include "DataTypes.h"

// returns decoded instruction based on its format (utility for decode())
dec_instr_t getDecodedInstructionFromFormat(word raw_instruction, instr_format_t format)
{
    dec_instr_t decoded_instruction;
    switch(format)
    {
        case R:
            decoded_instruction.valid = true;
            decoded_instruction.opcode = (byte) (raw_instruction & 127);
            decoded_instruction.imm = (word) 0;
            decoded_instruction.rd = (byte) ((raw_instruction >> 7) & 31);
            decoded_instruction.rs1 = (byte) ((raw_instruction >> 15) & 31);
            decoded_instruction.rs2 = (byte) ((raw_instruction >> 20) & 31);
            decoded_instruction.funct3 = (byte) ((raw_instruction >> 12) & 7);
            decoded_instruction.funct7 = (byte) ((raw_instruction >> 25) & 127);
            break;

        case I:
            decoded_instruction.valid = true;
            decoded_instruction.opcode = (byte) (raw_instruction & 127);
            decoded_instruction.imm = (word) ((raw_instruction >> 20) & 4095);
            decoded_instruction.rd = (byte) ((raw_instruction >> 7) & 31);
            decoded_instruction.rs1 = (byte) ((raw_instruction >> 15) & 31);
            decoded_instruction.rs2 = (byte) 0;
            decoded_instruction.funct3 = (byte) ((raw_instruction >> 12) & 7);
            decoded_instruction.funct7 = (byte) 0;
            break;

        case S:
            decoded_instruction.valid = true;
            decoded_instruction.opcode = (byte) (raw_instruction & 127);
            decoded_instruction.imm = (word) (((raw_instruction >> 20) & 4064) | ((raw_instruction >> 7) & 31));
            decoded_instruction.rd = (byte) 0;
            decoded_instruction.rs1 = (byte) ((raw_instruction >> 15) & 31);
            decoded_instruction.rs2 = (byte) ((raw_instruction >> 20) & 31);
            decoded_instruction.funct3 = (byte) ((raw_instruction >> 12) & 7);
            decoded_instruction.funct7 = (byte) 0;
            break;

        case B:
            decoded_instruction.valid = true;
            decoded_instruction.opcode = (byte) (raw_instruction & 127);
            decoded_instruction.imm = (word) (((raw_instruction >> 19) & 4096) | ((raw_instruction << 4) & 2048) |
                                              ((raw_instruction >> 20) & 2016) | ((raw_instruction >> 7) & 30));
            decoded_instruction.rd = (byte) 0;
            decoded_instruction.rs1 = (byte) ((raw_instruction >> 15) & 31);
            decoded_instruction.rs2 = (byte) ((raw_instruction >> 20) & 31);
            decoded_instruction.funct3 = (byte) ((raw_instruction >> 12) & 7);
            decoded_instruction.funct7 = (byte) 0;
            break;

        case U:
            decoded_instruction.valid = true;
            decoded_instruction.opcode = (byte) (raw_instruction & 127);
            decoded_instruction.imm = (word) (raw_instruction & 4294963200u);
            decoded_instruction.rd = (byte) ((raw_instruction >> 7) & 31);
            decoded_instruction.rs1 = (byte) 0;
            decoded_instruction.rs2 = (byte) 0;
            decoded_instruction.funct3 = (byte) 0;
            decoded_instruction.funct7 = (byte) 0;
            break;

        case J:
            decoded_instruction.valid = true;
            decoded_instruction.opcode = (byte) (raw_instruction & 127);
            decoded_instruction.imm = (word) (((raw_instruction >> 11) & 1048576) | (raw_instruction & 1044480) |
                                              ((raw_instruction >> 9) & 2048) | ((raw_instruction >> 20) & 2046));
            decoded_instruction.rd = (byte) ((raw_instruction >> 7) & 31);
            decoded_instruction.rs1 = (byte) 0;
            decoded_instruction.rs2 = (byte) 0;
            decoded_instruction.funct3 = (byte) 0;
            decoded_instruction.funct7 = (byte) 0;
            break;

        default:
            decoded_instruction.valid = false;
            break;
    }
    return decoded_instruction;
}

// returns encoded instruction based on its format (utility for assemble())
word getEncodedInstructionFromFormat(dec_instr_t decoded_instruction, instr_format_t format)
{
    word raw_instruction = 0;
    switch(format)
    {
        case R:
            raw_instruction |= (word)(decoded_instruction.opcode) & 127;
            raw_instruction |= ((word)(decoded_instruction.rd) & 31) << 7;
            raw_instruction |= ((word)(decoded_instruction.funct3) & 7) << 12;
            raw_instruction |= ((word)(decoded_instruction.rs1) & 31) << 15;
            raw_instruction |= ((word)(decoded_instruction.rs2) & 31) << 20;
            raw_instruction |= ((word)(decoded_instruction.funct7 & 127)) << 25;
            break;
        case I:
            raw_instruction |= (word)(decoded_instruction.opcode) & 127;
            raw_instruction |= ((word)(decoded_instruction.rd) & 31) << 7;
            raw_instruction |= ((word)(decoded_instruction.funct3) & 7) << 12;
            raw_instruction |= ((word)(decoded_instruction.rs1) & 31) << 15;
            raw_instruction |= ((word)(decoded_instruction.imm) & 4095) << 20;
            break;
        case S:
            raw_instruction |= (word)(decoded_instruction.opcode) & 127;
            raw_instruction |= ((word)(decoded_instruction.imm) & 31) << 7;
            raw_instruction |= ((word)(decoded_instruction.funct3) & 7) << 12;
            raw_instruction |= ((word)(decoded_instruction.rs1) & 31) << 15;
            raw_instruction |= ((word)(decoded_instruction.rs2) & 31) << 20;
            raw_instruction |= ((word)(decoded_instruction.imm) & 4064) << 20;
            break;
        case B:
            raw_instruction |= (word)(decoded_instruction.opcode) & 127;
            raw_instruction |= (((word)(decoded_instruction.imm) & 30) << 7) | 
                               (((word)(decoded_instruction.imm) & 2048) >> 4);
            raw_instruction |= ((word)(decoded_instruction.funct3) & 7) << 12;
            raw_instruction |= ((word)(decoded_instruction.rs1) & 31) << 15;
            raw_instruction |= ((word)(decoded_instruction.rs2) & 31) << 20;
            raw_instruction |= (((word)(decoded_instruction.imm) & 4096) << 19) |
                               (((word)(decoded_instruction.imm) & 2016) << 20);
            break;
        case U:
            raw_instruction |= (word)(decoded_instruction.opcode) & 127;
            raw_instruction |= ((word)(decoded_instruction.rd) & 31) << 7;
            raw_instruction |= (word)(decoded_instruction.imm) & 4294963200u;
            break;
        case J:
            raw_instruction |= (word)(decoded_instruction.opcode) & 127;
            raw_instruction |= ((word)(decoded_instruction.rd) & 31) << 7;
            raw_instruction |= (((word)(decoded_instruction.imm) & 1048576) << 11) |
                               (((word)(decoded_instruction.imm) & 2046) << 20) |
                               (((word)(decoded_instruction.imm) & 2048) << 9) |
                               ((word)(decoded_instruction.imm) & 1044480);
            break;
    }

    return raw_instruction;
}
#endif