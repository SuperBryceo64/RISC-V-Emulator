#include "RV64I.h"

RV64I::RV64I(endian_t endian) : RISC_V<double_word>(32, endian) 
    { base = "RV64I"; (*constants)[0xFFFFFFFF] = Register<double_word>(0xFFFFFFFF, true); }

RV64I::RV64I(endian_t endian, ExtensionList<double_word> &extension_list) : RISC_V<double_word>(32, endian, extension_list)
    { base = "RV64I"; (*constants)[0xFFFFFFFF] = Register<double_word>(0xFFFFFFFF, true); }

RV64I::RV64I(ExtensionList<double_word> &extension_list) : RISC_V<double_word>(32, LITTLE, extension_list)
    { base = "RV64I"; (*constants)[0xFFFFFFFF] = Register<double_word>(0xFFFFFFFF, true); }

RV64I::~RV64I() {}

dec_instr_t RV64I::decode()
{
    word raw_instruction = ir->read();
    byte opcode = raw_instruction & 127;  // retrieve opcode from least significant 7 bits
    dec_instr_t decoded_instruction;  // decoded instruction should be invalid by default
    switch(opcode)
    {
        case ARITH_LOG_R_W:
            decoded_instruction = getDecodedInstructionFromFormat(raw_instruction, R);
            // If ARITH_LOG_R_W instruction doesn't exist in Base ISA (likely M instruction), check extensions
            if (decoded_instruction.funct7 != 0 && decoded_instruction.funct7 != 32)
            {
                decoded_instruction.valid = false;
                if(extensions != NULL) 
                {
                    byte idx = 0;
                    while(idx < extensions->size() && !decoded_instruction.valid)
                    {
                        decoded_instruction = (extensions->at(idx++))->decode();
                    }
                }
            }
            break;
        
        case ARITH_LOG_I_W:
            decoded_instruction = getDecodedInstructionFromFormat(raw_instruction, I);
            break;
        
        default:
            // if opcode doesn't exist in RV64, check RV32 and extensions
            decoded_instruction = RISC_V<double_word>::decode();
            break;
    }

    // If decoded_instruction is still invalid, an illegal instruction exception should be raised during execution
    return decoded_instruction;
}

bool RV64I::execute(dec_instr_t instruction)
{
    // Invalid instructions are obviously not allowed to continue 
    if (!instruction.valid) {
        RISC_V<double_word>::setInterruptFlag(II);
        return false; 
    }

    // The user program is not allowed to modify the stack pointer
    if (pc->read() >= program_address_range.start && pc->read() <= program_address_range.end && instruction.rd == 2)
    {
        setInterruptFlag(MSP);
        return false;
    }

    // Execution was successful if instruction was found and sucessfully executed
    bool success = true;
    // count determines if pc should count after instruction is executed (pc should not count if RV32 instruction is executed)
    bool count = true;

    switch (instruction.opcode)
    {
        case ARITH_LOG_R_W:
            switch(combineFunct(instruction.funct3, instruction.funct7))
            {
                case 0b0000000000:  // ADDW
                    alu->setOperand1(register_set[instruction.rs1].read());     // load rs1's value into ALU
                    alu->operate(ADD, register_set[instruction.rs2].read());    // perform addition with rs2's value
                    alu->setOperand1(alu->getResult());                         // load sum into ALU
                    alu->operate(SXT, constants->at(0x80000000).read());        // sign extend sum by bit 31
                    register_set[instruction.rd].write(alu->getResult());       // store sign ext'd result into rd
                    break;
                
                case 0b0000100000:  // SUBW
                    alu->setOperand1(register_set[instruction.rs1].read());     // load rs1's value into ALU
                    alu->operate(SUB, register_set[instruction.rs2].read());    // perform subtraction with rs2's value
                    alu->setOperand1(alu->getResult());                         // load difference into ALU
                    alu->operate(SXT, constants->at(0x80000000).read());        // sign extend sum by bit 31
                    register_set[instruction.rd].write(alu->getResult());       // store sign ext'd result into rd
                    break;

                case 0b0010000000:  // SLLW
                {
                    // Only shift with the 5 least significant bits of rs2
                    double_word shamt = register_set[instruction.rs2].read() & 31;  // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());         // load rs1's value into ALU
                    alu->operate(SLL, shamt);                                       // perform logical left shift with rs2's value
                    alu->setOperand1(alu->getResult());                             // load result into ALU
                    alu->operate(SXT, constants->at(0x80000000).read());            // sign extend result by bit 31
                    register_set[instruction.rd].write(alu->getResult());           // store result into rd
                    break;
                }

                case 0b1010000000:  // SRLW
                {
                    // Only shift with the 5 least significant bits of rs2
                    double_word shamt = register_set[instruction.rs2].read() & 31;  // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());         // load rs1's value into ALU
                    alu->operate(AND, constants->at(0xFFFFFFFF).read());            // zero extend upper word of rs1's value
                    alu->setOperand1(alu->getResult());                             // load zero ext'd rs1 into ALU
                    alu->operate(SRL, shamt);                                       // perform logical right shift with rs2's value
                    alu->setOperand1(alu->getResult());                             // load result into ALU
                    alu->operate(SXT, constants->at(0x80000000).read());            // sign extend result by bit 31
                    register_set[instruction.rd].write(alu->getResult());           // store result into rd
                    break;
                }

                case 0b1010100000:  // SRAW
                {
                    // Only shift with the 5 least significant bits of rs2
                    double_word shamt = register_set[instruction.rs2].read() & 31;  // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());         // load rs1's value into ALU
                    alu->operate(SXT, constants->at(0x80000000).read());            // sign extend rs1's value by bit 31
                    alu->setOperand1(alu->getResult());                             // load sign ext'd rs1 into ALU
                    alu->operate(SRA, shamt);                                       // perform arithmetic right shift with rs2's value
                    register_set[instruction.rd].write(alu->getResult());           // store result into rd
                    break;
                }

                default:
                    // funct doesn't exist in base ISA; have extensions execute instruction instead
                    success = executeFromExtensions(instruction);
                    break;
            }
            break;
        
        case ARITH_LOG_I_W:
            switch(instruction.funct3)
            {
                case 0b000:  // ADDIW
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());          // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                     // perform addition with sign ext'd imm
                    alu->setOperand1(alu->getResult());                      // load result into ALU
                    alu->operate(SXT, constants->at(0x80000000).read());     // sign extend result by bit 31
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;
                
                case 0b001:  // SLLIW
                {
                    // Only shift with the 5 least significant bits of imm
                    double_word shamt = instruction.imm & 31;                // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(SLL, shamt);                                // perform logical left shift with imm
                    alu->setOperand1(alu->getResult());                      // load result into ALU
                    alu->operate(SXT, constants->at(0x80000000).read());     // sign extend result by bit 31
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;
                }

                case 0b101:
                {
                    double_word shamt = instruction.imm & 31;                 // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());   // load rs1's value into ALU
                    if(((instruction.imm >> 10) & 1) == 0)  // SRLIW
                    {
                        // Only shift with the 5 least significant bits of imm
                        alu->operate(AND, constants->at(0xFFFFFFFF).read());  // zero extend upper word of rs1's value
                        alu->setOperand1(alu->getResult());                   // load zero ext'd rs1 into ALU
                        alu->operate(SRL, shamt);                             // perform logical right shift with imm
                        alu->setOperand1(alu->getResult());                   // load result into ALU
                        alu->operate(SXT, constants->at(0x80000000).read());  // sign extend result by bit 31
                    }
                    else  // SRAIW
                    {
                        // Only shift with the 5 least significant bits of imm
                        alu->operate(SXT, constants->at(0x80000000).read());  // sign extend rs1's value by bit 31
                        alu->setOperand1(alu->getResult());                   // load sign ext'd rs1 into ALU
                        alu->operate(SRA, shamt);                             // perform arithmetic right shift with imm
                    }         
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;
                }

                default:
                    // funct doesn't exist in base ISA; have extensions execute instruction instead
                    success = executeFromExtensions(instruction);
                    break;
            }
            break;

        case LOAD:
            switch(instruction.funct3)
            {
                case 0b011:  // LD
                    alu->setOperand1(instruction.imm);                         // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());            // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());    // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                       // add rs1's value with sign ext'd imm
                    // check if user program is attempting to access restricted memory
                    if ((pc->read() >= program_address_range.start && pc->read() <= program_address_range.end) &&
                        (alu->getResult() < program_address_range.start || alu->getResult() > program_address_range.end) &&
                        (alu->getResult() < global_data_address_range.start || alu->getResult() > global_data_address_range.end))
                    {
                        setInterruptFlag(SF);
                    }
                    else
                    {
                        register_set[instruction.rd].write(memory->            // load double word from memory into rd
                            template getWord<double_word>(alu->getResult()));  // (The RISC-V standard does not require sign extension of the double word)
                    }
                    break;

                case 0b110:  // LWU
                    alu->setOperand1(instruction.imm);                         // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());            // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());    // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                       // add rs1's value with sign ext'd imm
                    // check if user program is attempting to access restricted memory
                    if ((pc->read() >= program_address_range.start && pc->read() <= program_address_range.end) &&
                        (alu->getResult() < program_address_range.start || alu->getResult() > program_address_range.end) &&
                        (alu->getResult() < global_data_address_range.start || alu->getResult() > global_data_address_range.end))
                    {
                        setInterruptFlag(SF);
                    }
                    else
                    {
                        register_set[instruction.rd].write(memory->            // load zero ext'd word from memory into rd
                            template getWord<word>(alu->getResult()));
                    }
                    break;

                default:
                    // if load instruction doesn't exist in RV64, execute from RV32
                    count = false;
                    success = RISC_V<double_word>::execute(instruction);
                    break;
            }
            break;

        case STORE:
            switch(instruction.funct3)
            {
                case 0b011:  // SD
                    alu->setOperand1(instruction.imm);                                 // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());                    // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());            // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                               // add rs1's value with sign ext'd imm
                    // check if user program is attempting to access restricted memory
                    if ((pc->read() >= program_address_range.start && pc->read() <= program_address_range.end) &&
                        (alu->getResult() < program_address_range.start || alu->getResult() > program_address_range.end) &&
                        (alu->getResult() < global_data_address_range.start || alu->getResult() > global_data_address_range.end))
                    {
                        setInterruptFlag(alu->getResult() == 0 ? SAZ : SF);
                    }
                    else
                    {
                        memory-> template setWord<double_word>                         // store LS double word of rs2's value into memory
                            (alu->getResult(), register_set[instruction.rs2].read());  
                    }
                    break;
                
                default:
                    // if store instruction doesn't exist in RV64, execute from RV32
                    count = false;
                    success = RISC_V<double_word>::execute(instruction);
                    break;
            }
            break;

        default:
            // if opcode instruction doesn't exist in RV64, execute from RV32
            count = false;
            success = RISC_V<double_word>::execute(instruction);
            break;
    }

    if(success && count) { pc->count(); }

    if (!success) { setInterruptFlag(II); }

    return success;
}