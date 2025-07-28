#include "RV32E.h"

RV32E::RV32E(endian_t endian) : RISC_V<word>(16, endian) { base = "RV32E"; }

RV32E::RV32E(endian_t endian, ExtensionList<word> &extension_list) : RISC_V<word>(16, endian, extension_list)
    { base = "RV32E"; }

RV32E::RV32E(ExtensionList<word> &extension_list) : RISC_V<word>(16, LITTLE, extension_list)
    { base = "RV32E"; }

RV32E::~RV32E() {}

dec_instr_t RV32E::decode()
{
    dec_instr_t decoded_instruction = RISC_V<word>::decode();

    // clear all but the first 4 bits of rd, rs1, and rs2 to ensure x16-x31 are never accessed
    decoded_instruction.rd &= 15;
    decoded_instruction.rs1 &= 15;
    decoded_instruction.rs2 &= 15;

    return decoded_instruction;
}