#include "M.h"
#include "../Utilities/CombineFunct.h"

template <typename word_size>
M<word_size>::M() : Extension<word_size>() { name = "M"; }

template <typename word_size>
M<word_size>::M(RISC_V_Components<word_size> &cpu_components) : Extension<word_size>(cpu_components)
    { name = "M"; cpu_alu = new Multiplier<word_size>(); }

template <typename word_size>
M<word_size>::~M() { if (cpu_alu != NULL) { delete cpu_alu; } }

template <typename word_size>
M<word_size>* M<word_size>::create(RISC_V_Components<word_size> &cpu_components)
{
    M<word_size> *copy = new M<word_size>(cpu_components);
    return copy;
}

template <typename word_size>
dec_instr_t M<word_size>::decode()
{ 
    word raw_instruction = cpu_ir->read();
    byte opcode = raw_instruction & 127;  // retrieve opcode from least significant 7 bits
    dec_instr_t decoded_instruction;  // decoded instruction should be invalid by default

    switch(opcode)
    {
        case ARITH_LOG_R:   // All M instructions are part of ARITH_LOG_R and ARITH_LOG_R_W opcode groups
        case ARITH_LOG_R_W:
            decoded_instruction = getDecodedInstructionFromFormat(raw_instruction, R);
            // Instruction must have funct7 = 1 to be part of M extension
            if (decoded_instruction.funct7 != 1) { decoded_instruction.valid = false; }
            // RV32M should not be able to decode ARITH_LOG_R_W instructions
            if (sizeof(word_size) <= 4 && opcode == ARITH_LOG_R_W) { decoded_instruction.valid = false; }
            break;

        default:
            break;
    }

    return decoded_instruction;
}

template <typename word_size>
bool M<word_size>::execute(dec_instr_t instruction)
{ 
    // Invalid instructions are obviously not allowed to continue 
    if(!instruction.valid) { return false; }

    // Execution was successful if instruction was found and sucessfully executed
    bool success = true;
    
    switch (instruction.opcode)
    {
        case ARITH_LOG_R:
            switch (combineFunct(instruction.funct3, instruction.funct7))
            {
                case 0b0000000001:  // MUL
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());   // load rs1's value into ALU
                    cpu_alu->operate(MUL, cpu_register_set[instruction.rs2].read());  // perform MUL with rs2's value
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // store result into rd
                    break;
                
                case 0b0010000001:  // MULH
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());    // load rs1's value into ALU
                    cpu_alu->operate(MULH, cpu_register_set[instruction.rs2].read());  // perform MULH with rs2's value
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());      // store result into rd
                    break;
                
                case 0b0100000001:  // MULHSU
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());      // load rs1's value into ALU
                    cpu_alu->operate(MULHSU, cpu_register_set[instruction.rs2].read());  // perform MULHSU with rs2's value
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());        // store result into rd
                    break;

                case 0b0110000001:  // MULHU
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());     // load rs1's value into ALU
                    cpu_alu->operate(MULHU, cpu_register_set[instruction.rs2].read());  // perform MULHU with rs2's value
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());       // store result into rd
                    break;

                case 0b1000000001:  // DIV
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());   // load rs1's value into ALU
                    cpu_alu->operate(DIV, cpu_register_set[instruction.rs2].read());  // perform DIV with rs2's value
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // store result into rd
                    break;

                case 0b1010000001:  // DIVU
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());    // load rs1's value into ALU
                    cpu_alu->operate(DIVU, cpu_register_set[instruction.rs2].read());  // perform DIVU with rs2's value
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());      // store result into rd
                    break;

                case 0b1100000001:  // REM
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());   // load rs1's value into ALU
                    cpu_alu->operate(REM, cpu_register_set[instruction.rs2].read());  // perform REM with rs2's value
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // store result into rd
                    break;

                case 0b1110000001:  // REMU
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());    // load rs1's value into ALU
                    cpu_alu->operate(REMU, cpu_register_set[instruction.rs2].read());  // perform REMU with rs2's value
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());      // store result into rd
                    break;

                default:
                    success = false;
                    break;
            }
            break;
        
        case ARITH_LOG_R_W:
            switch (combineFunct(instruction.funct3, instruction.funct7))
            {
                case 0b0000000001:  // MULW
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());   // load rs1's value into ALU
                    cpu_alu->operate(MUL, cpu_register_set[instruction.rs2].read());  // perform MUL with rs2's value
                    cpu_alu->setOperand1(cpu_alu->getResult());                       // load result into ALU
                    cpu_alu->operate(SXT, (*cpu_constants).at(0x80000000).read());    // sign extend result by bit 31
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // store sign ext'd result into rd
                    break;

                case 0b1000000001:  // DIVW
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());   // load rs1's value into ALU
                    cpu_alu->operate(SXT, (*cpu_constants).at(0x80000000).read());    // sign extend rs1's value by bit 31
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // temporarily store sign ext'd rs1 into rd
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs2].read());   // load rs2's value into ALU
                    cpu_alu->operate(SXT, (*cpu_constants).at(0x80000000).read());    // sign extend rs2's value by bit 31
                    cpu_alu->setOperand1(cpu_register_set[instruction.rd].read());    // load sign ext'd rd1 into ALU
                    cpu_alu->operate(DIV, cpu_alu->getResult());                      // perform DIV with sign ext'd rs2
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // store result into rd
                    break;

                case 0b1010000001:  // DIVUW
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());   // load rs1's value into ALU
                    cpu_alu->operate(AND, (*cpu_constants).at(0xFFFFFFFF).read());    // zero extend rs1's value by bit 31
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // temporarily store zero ext'd rs1 into rd
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs2].read());   // load rs2's value into ALU
                    cpu_alu->operate(AND, (*cpu_constants).at(0xFFFFFFFF).read());    // zero extend rs2's value by bit 31
                    cpu_alu->setOperand1(cpu_register_set[instruction.rd].read());    // load zero ext'd rd1 into ALU
                    cpu_alu->operate(DIVU, cpu_alu->getResult());                     // perform DIVU with zero ext'd rs2
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // store result into rd
                    break;

                case 0b1100000001:  // REMW
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());   // load rs1's value into ALU
                    cpu_alu->operate(SXT, (*cpu_constants).at(0x80000000).read());    // sign extend rs1's value by bit 31
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // temporarily store sign ext'd rs1 into rd
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs2].read());   // load rs2's value into ALU
                    cpu_alu->operate(SXT, (*cpu_constants).at(0x80000000).read());    // sign extend rs2's value by bit 31
                    cpu_alu->setOperand1(cpu_register_set[instruction.rd].read());    // load sign ext'd rd1 into ALU
                    cpu_alu->operate(REM, cpu_alu->getResult());                      // perform REM with sign ext'd rs2
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // store result into rd
                    break;

                case 0b1110000001:  // REMUW
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs1].read());   // load rs1's value into ALU
                    cpu_alu->operate(AND, (*cpu_constants).at(0xFFFFFFFF).read());    // zero extend rs1's value by bit 31
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // temporarily store zero ext'd rs1 into rd
                    cpu_alu->setOperand1(cpu_register_set[instruction.rs2].read());   // load rs2's value into ALU
                    cpu_alu->operate(AND, (*cpu_constants).at(0xFFFFFFFF).read());    // zero extend rs2's value by bit 31
                    cpu_alu->setOperand1(cpu_register_set[instruction.rd].read());    // load zero ext'd rd1 into ALU
                    cpu_alu->operate(REMU, cpu_alu->getResult());                     // perform REMU with zero ext'd rs2
                    cpu_register_set[instruction.rd].write(cpu_alu->getResult());     // store result into rd
                    break;
            }
            break;

        default:
            success = false;
            break;
    }

    return success;
}