#include "RISC_V.h"
#include "stdio.h"
#include "../Utilities/HexDump.h"

template <typename word_size>
RISC_V<word_size>::RISC_V(byte number_of_registers, endian_t endian) 
{ 
    pc = new Counter<word_size>(4);
    ir = new Register<word>;
    alu = new ALU<word_size>;
    register_set = new Register<word_size>[number_of_registers]{Register<word_size>(0, true)};  // x0 is always hardcoded to zero
    constants = new ConstantList<word_size>{{0x80, Register<word_size>(0x80, true)}, {0x800, Register<word_size>(0x800, true)},
                                            {0x1000, Register<word_size>(0x1000, true)}, {0x8000, Register<word_size>(0x8000, true)},
                                            {0x100000, Register<word_size>(0x100000, true)}, {0x80000000, Register<word_size>(0x80000000, true)}};
    memory = new Memory<word_size>(endian);
    extensions = NULL;
    base = "";
    num_registers = number_of_registers;
    running = false;
    restarting = false;
    bootloader_address_range = {0x4, 0x7FF};  // by default, bootloader program should start at address 0x4 and end at address 0x7FF
    program_address_range = {0x800, 0x400007FF};  // by default, main program should start at address 0x800 and end at address 0x400007FF
    global_data_address_range = {0x40000800, 0x800007FF};  // by default, global data should start at address 0x40000800 and end at address 0x800007FF
    interrupt_handler_address_range = {0xFFFFF800, 0xFFFFFFFF};  // by default, interrupt handler should start at address 0xFFFFF800 and end at address 0xFFFFFFFF
}

template <typename word_size>
RISC_V<word_size>::RISC_V(byte number_of_registers, endian_t endian, ExtensionList<word_size> &extension_list)
    : RISC_V(number_of_registers, endian)
{
    extensions = new ExtensionList<word_size>();
    RISC_V_Components components = getComponents();
    for(Extension<word_size> *extension_ptr : extension_list)
    {
        extensions->push_back(extension_ptr->create(components));
    }
}

template <typename word_size>
RISC_V<word_size>::RISC_V(ExtensionList<word_size> &extension_list) : RISC_V(32, LITTLE, extension_list) {}

template <typename word_size>
RISC_V<word_size>::~RISC_V() 
{
    delete pc;
    delete ir;
    delete alu;
    delete [] register_set;
    delete constants;
    delete memory;
    if(extensions != NULL) 
    {
        for(Extension<word_size> *extension : *extensions) { delete extension; }
        delete extensions;
    }
}

template <typename word_size>
bool RISC_V<word_size>::loadMemory() 
{
    // assemble programs
    // open binary files containing machine code
    FILE *bootloader_ptr, *program_ptr, *global_data_ptr, *interrupt_handler_ptr;
    bootloader_ptr = fopen("./Programs/bootloader", "rb");
    if (bootloader_ptr == NULL) 
    {
        perror("Error opening bootloader");
        return false;
    }

    program_ptr = fopen("./Programs/program", "rb");
    if (program_ptr == NULL)
    {
        perror("Error opening main program");
        return false;
    }

    global_data_ptr = fopen("./Programs/program_data", "rb");
    if (global_data_ptr == NULL)
    {
        perror("Error opening global data");
        return false;
    }

    interrupt_handler_ptr = fopen("./Programs/interrupt_handler", "rb");
    if (interrupt_handler_ptr == NULL)
    {
        perror("Error opening interrupt handler");
        return false;
    }

    // load machine code into respective memory addresses
    // Bootloader
    word_size curr_address = bootloader_address_range.start;
    word curr_instruction = 0;
    while((curr_address+3 <= bootloader_address_range.end)  // last byte of instruction must fit in range
       && (fread(&curr_instruction, sizeof(word), 1, bootloader_ptr) != 0))  // AND there must still be data left in the file to read
    {
        memory-> template setWord<word>(curr_address, curr_instruction);
        curr_address += 4;
    }

    if((curr_address+3 > bootloader_address_range.end)
       && (fread(&curr_instruction, sizeof(word), 1, bootloader_ptr) != 0))  // if bootloader code doesn't fit inside memory space
    {
        printf("Error: Bootloader cannot fit in allocated memory space\n");
        return false;
    }

    // Main Program
    curr_address = program_address_range.start;
    curr_instruction = 0;
    while((curr_address+3 <= program_address_range.end)  // last byte of instruction must fit in range
       && (fread(&curr_instruction, sizeof(word), 1, program_ptr) != 0))  // AND there must still be data left in the file to read
    {
        memory-> template setWord<word>(curr_address, curr_instruction);
        curr_address += 4;
    }

    if((curr_address+3 > program_address_range.end)
       && (fread(&curr_instruction, sizeof(word), 1, program_ptr) != 0))  // if main program code doesn't fit inside memory space
    {
        printf("Error: Main program cannot fit in allocated memory space\n");
        return false;
    }

    // Global Data
    curr_address = global_data_address_range.start;
    byte curr_data = 0;
    while((curr_address <= global_data_address_range.end)  // last byte of instruction must fit in range
       && (fread(&curr_data, sizeof(byte), 1, global_data_ptr) != 0))  // AND there must still be data left in the file to read
    {
        memory->setByte(curr_address, curr_data);
        curr_address++;
    }

    if((curr_address > global_data_address_range.end)
       && (fread(&curr_data, sizeof(byte), 1, global_data_ptr) != 0))  // if main program code doesn't fit inside memory space
    {
        printf("Error: Global data cannot fit in allocated memory space\n");
        return false;
    }

    // Interrupt Handler
    curr_address = interrupt_handler_address_range.start;
    curr_instruction = 0;
    while((curr_address+3 <= interrupt_handler_address_range.end)  // last byte of instruction must fit in range
       && (fread(&curr_instruction, sizeof(word), 1, interrupt_handler_ptr) != 0))  // AND there must still be data left in the file to read
    {
        memory-> template setWord<word>(curr_address, curr_instruction);
        curr_address += 4;
    }

    if((curr_address+3 > interrupt_handler_address_range.end)
       && (fread(&curr_instruction, sizeof(word), 1, interrupt_handler_ptr) != 0))  // if interrupt handler code doesn't fit inside memory space
    {
        printf("Error: Interrupt handler cannot fit in allocated memory space\n");
        return false;
    }

    return true;
}

template <typename word_size>
void RISC_V<word_size>::debugger()
{
    for(;;)
    {
        int response;

        // Print title card and menu options
        printf("\n*******************************");
        printf("\n      RISC-V CPU Debugger");
        printf("\n*******************************");
        printf("\nBase: %s\n", base.c_str());
        printf("Extensions: ");
        if(extensions != NULL)
        {
            for(Extension<word_size> *extension_ptr : ExtensionList<word_size> (*extensions))
            printf("%s ", extension_ptr->getName().c_str());
        }
        else { printf("None"); }
        printf("\n\nPC = 0x%llX\n\n", (double_word) pc->read());
        printf("Menu:\n");
        printf("1.) Read Registers\n");
        printf("2.) Read Memory\n");
        printf("3.) Continue Program\n");
        printf("4.) Terminate Program\n");
        printf("5.) Restart Program\n");
        
        // Receive user's response
        bool valid;
        do
        {
            printf("\nResponse: ");
            scanf("%d", &response);
            scanf("%*[^\n]");
            switch(response)
            {
                case READ_REGISTERS:
                case READ_MEMORY:
                case CONTINUE_PROGRAM:
                case TERMINATE_PROGRAM:
                case RESTART_PROGRAM:
                    valid = true;
                    break;
                default:
                    valid = false;
                    printf("\n**Invalid Response!**\n");
                    break;
            }
        } while (!valid);
        printf("\n");

        // Execute option
        switch(response)
        {
            case READ_REGISTERS:
                for(byte i = 0; i < num_registers; i++)
                {
                    if(sizeof(word_size) <= 4) { printf("x%-2u = %08X%s", i, (word) register_set[i].read(), (i%8 == 7) ? "\n" : " | "); }
                    else { printf("x%-2u = %016llX%s", i, (double_word) register_set[i].read(), (i%4 == 3) ? "\n" : " | "); }
                }
                printf("\n");
                break;
            case READ_MEMORY:
            {
                word_size start = 0, end = 0;
                do
                {         
                    printf("Select the address in hex: 0x");
                    valid = scanf(sizeof(word_size) <= 4 ? "%x%*[^0-9,a-f,A-F\n]%x" : "%llx%*[^0-9,a-f,A-F\n]%llx", &start, &end) >= 1;
                    scanf("%*[^\n]");
                    if (!valid) { printf("\n**Invalid Response!**\n\n"); }
                } while (!valid);
                if(start >= end)  // print a single byte
                { 
                    printf(sizeof(word_size) <= 4 ? "%08X: " : "%016llX: ", start);
                    memory->printByte(start, false, HEX);
                    byte data = memory->getByte(start);
                    printf("[%c]\n", (data >= ' ' && data <= '~') ? data : '.');
                }
                else { hexDump<word_size>(*memory, start, end); }  // print line(s) of bytes
                printf("\n");
                break;
            }
            case CONTINUE_PROGRAM:
                return;
            case TERMINATE_PROGRAM:
                printf("Are you sure you want to terminate program? (y/n): ");
                scanf("\n%c", (char*) &response);
                scanf("%*[^\n]");
                if (response == 'Y' || response == 'y') { setInterruptFlag(TP); printf("\n"); return; }
                else { continue; }
                break;
            case RESTART_PROGRAM:
                printf("Are you sure you want to restart program? (y/n): ");
                scanf("\n%c", (char*) &response);
                scanf("%*[^\n]");
                if (response == 'Y' || response == 'y') { setInterruptFlag(RP); printf("\n"); return; }
                else { continue; }
                break;
        }

        printf("Return to Menu? (y/n): ");
        scanf("\n%c", (char*) &response);
        scanf("%*[^\n]");
        if ((char) response == 'N' || (char) response == 'n') { return; }
        else { continue; }
    }
}

template <typename word_size>
void RISC_V<word_size>::fetch()
{
    word_size address = pc->read();
    word instruction = memory-> template getWord<word>(address);
    ir->write(instruction);
}

template <typename word_size>
dec_instr_t RISC_V<word_size>::decode()
{
    word raw_instruction = ir->read();
    byte opcode = raw_instruction & 127;  // retrieve opcode from least significant 7 bits
    dec_instr_t decoded_instruction;  // decoded instruction should be invalid by default
    switch(opcode)
    {
        case ARITH_LOG_R:
            decoded_instruction = getDecodedInstructionFromFormat(raw_instruction, R);
            // If ARITH_LOG_R instruction doesn't exist in Base ISA (likely M instruction), check extensions
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
        
        case ARITH_LOG_I:
        case LOAD:
        case JALR:
        case ENVIRONMENT:
            decoded_instruction = getDecodedInstructionFromFormat(raw_instruction, I);
            break;
        
        case STORE:
            decoded_instruction = getDecodedInstructionFromFormat(raw_instruction, S);
            break;
        
        case BRANCH:
            decoded_instruction = getDecodedInstructionFromFormat(raw_instruction, B);
            break;
        
        case LUI:
        case AUIPC:
            decoded_instruction = getDecodedInstructionFromFormat(raw_instruction, U);
            break;

        case JAL:
            decoded_instruction = getDecodedInstructionFromFormat(raw_instruction, J);
            break;
        
        default:
            // opcode doesn't exist in base ISA; have extensions decode instruction instead
            if(extensions != NULL) 
            {
                byte idx = 0;
                while(idx < extensions->size() && !decoded_instruction.valid)
                {
                    decoded_instruction = (extensions->at(idx++))->decode();
                }
            }
            break;
    }

    // If decoded_instruction is still invalid, an illegal instruction exception should be raised during execution
    return decoded_instruction;
}

template <typename word_size>
bool RISC_V<word_size>::execute(dec_instr_t instruction)
{
    // Invalid instructions are obviously not allowed to continue 
    if (!instruction.valid) {
        setInterruptFlag(II);
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
    // count determines if pc should count after instruction is executed (pc should not count for jumps and successful branches)
    bool count = true;
    
    switch (instruction.opcode)
    {
        case ARITH_LOG_R:
            switch(combineFunct(instruction.funct3, instruction.funct7))
            {
                case 0b0000000000:  // ADD
                    alu->setOperand1(register_set[instruction.rs1].read());     // load rs1's value into ALU
                    alu->operate(ADD, register_set[instruction.rs2].read());    // perform addition with rs2's value
                    register_set[instruction.rd].write(alu->getResult());       // store result into rd
                    break;

                case 0b0000100000:  // SUB
                    alu->setOperand1(register_set[instruction.rs1].read());     // load rs1's value into ALU
                    alu->operate(SUB, register_set[instruction.rs2].read());    // perform subtraction with rs2's value
                    register_set[instruction.rd].write(alu->getResult());       // store result into rd
                    break;

                case 0b0010000000:  // SLL
                {
                    // Only shift with the [log2(word_size)] least significant bits of rs2
                    word_size shamt = register_set[instruction.rs2].read() & (sizeof(word_size)*8 - 1);  // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());                              // load rs1's value into ALU
                    alu->operate(SLL, shamt);                                                            // perform logical left shift with rs2's value
                    register_set[instruction.rd].write(alu->getResult());                                // store result into rd
                    break;
                }

                case 0b0100000000:  // SLT
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(LT, register_set[instruction.rs2].read());  // perform less than rs2's value
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;

                case 0b0110000000:  // SLTU
                    alu->setOperand1(register_set[instruction.rs1].read());   // load rs1's value into ALU
                    alu->operate(LTU, register_set[instruction.rs2].read());  // perform unsigned less than with rs2's value
                    register_set[instruction.rd].write(alu->getResult());     // store result into rd
                    break;

                case 0b1000000000:  // XOR
                    alu->setOperand1(register_set[instruction.rs1].read());   // load rs1's value into ALU
                    alu->operate(XOR, register_set[instruction.rs2].read());  // perform bitwise XOR with rs2's value
                    register_set[instruction.rd].write(alu->getResult());     // store result into rd
                    break;

                case 0b1010000000:  // SRL
                {
                    // Only shift with the [log2(word_size)] least significant bits of rs2
                    word_size shamt = register_set[instruction.rs2].read() & (sizeof(word_size)*8 - 1);  // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());                              // load rs1's value into ALU
                    alu->operate(SRL, shamt);                                                            // perform logical right shift with rs2's value
                    register_set[instruction.rd].write(alu->getResult());                                // store result into rd
                    break;
                }
                
                case 0b1010100000:  // SRA
                {
                    // Only shift with the [log2(word_size)] least significant bits of rs2
                    word_size shamt = register_set[instruction.rs2].read() & (sizeof(word_size)*8 - 1);  // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());                              // load rs1's value into ALU
                    alu->operate(SRA, shamt);                                                            // perform arithmetic right shift with rs2's value
                    register_set[instruction.rd].write(alu->getResult());                                // store result into rd
                    break;
                }

                case 0b1100000000:  // OR
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(OR, register_set[instruction.rs2].read());  // perform bitwise OR with rs2's value
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;

                case 0b1110000000:  // AND
                    alu->setOperand1(register_set[instruction.rs1].read());   // load rs1's value into ALU
                    alu->operate(AND, register_set[instruction.rs2].read());  // perform bitwise AND with rs2's value
                    register_set[instruction.rd].write(alu->getResult());     // store result into rd
                    break;

                default:
                    // funct doesn't exist in base ISA; have extensions execute instruction instead
                    success = executeFromExtensions(instruction);
                    break;
            }
            break;

        case ARITH_LOG_I:
            switch(instruction.funct3)
            {
                case 0b000:  // ADDI
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());          // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                     // perform addition with sign ext'd imm
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;

                case 0b001:  // SLLI
                {
                    // Only shift with the [log2(word_size)] least significant bits of imm
                    word_size shamt = instruction.imm & (sizeof(word_size)*8 - 1);  // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());         // load rs1's value into ALU
                    alu->operate(SLL, shamt);                                       // perform logical left shift with imm
                    register_set[instruction.rd].write(alu->getResult());           // store result into rd
                    break;
                }

                case 0b010:  // SLTI
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());          // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(LT, alu->getResult());                      // perform less than with sign ext'd imm
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;

                case 0b011:  // SLTIU
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());          // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(LTU, alu->getResult());                      // perform unsigned less than with sign ext'd imm
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;

                case 0b100:  // XORI
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());          // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(XOR, alu->getResult());                     // perform bitwise XOR with sign ext'd imm
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;

                case 0b101:
                {
                    word_size shamt = instruction.imm & (sizeof(word_size)*8 - 1);  // calculate shift amount
                    alu->setOperand1(register_set[instruction.rs1].read());         // load rs1's value into ALU
                    if(((instruction.imm >> 10) & 1) == 0)  // SRLI
                    {
                        // Only shift with the [log2(word_size)] least significant bits of imm
                        alu->operate(SRL, shamt);                                   // perform logical right shift with imm
                    }
                    else  // SRAI
                    {
                        // Only shift with the [log2(word_size)] least significant bits of imm
                        alu->operate(SRA, shamt);                                   // perform arithmetic right shift with imm
                    }
                    register_set[instruction.rd].write(alu->getResult());           // store result into rd
                    break;
                }

                case 0b110:  // ORI
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());          // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(OR, alu->getResult());                      // perform bitwise OR with sign ext'd imm
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;

                case 0b111:  // ANDI
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());          // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(AND, alu->getResult());                     // perform bitwise AND with sign ext'd imm
                    register_set[instruction.rd].write(alu->getResult());    // store result into rd
                    break;
            }
            break;

        case LOAD:
            switch(instruction.funct3)
            {
                case 0b000:  // LB
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
                        alu->setOperand1(memory->getByte(alu->getResult()));   // load byte from memory into ALU
                        alu->operate(SXT, constants->at(0x80).read());         // sign extend byte
                        register_set[instruction.rd].write(alu->getResult());  // store sign ext'd byte into rd
                    }
                    break;

                case 0b001:  // LH
                    alu->setOperand1(instruction.imm);                           // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());              // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());      // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                         // add rs1's value with sign ext'd imm
                    // check if user program is attempting to access restricted memory
                    if ((pc->read() >= program_address_range.start && pc->read() <= program_address_range.end) &&
                        (alu->getResult() < program_address_range.start || alu->getResult() > program_address_range.end) &&
                        (alu->getResult() < global_data_address_range.start || alu->getResult() > global_data_address_range.end))
                    {
                        setInterruptFlag(SF);
                    }
                    else
                    {
                        alu->setOperand1(memory->                                // load half word from memory into ALU
                            template getWord<half_word>(alu->getResult()));  
                        alu->operate(SXT, constants->at(0x8000).read());         // sign extend half word
                        register_set[instruction.rd].write(alu->getResult());    // store sign ext'd half word into rd
                    }
                    break;

                case 0b010:  // LW
                    alu->setOperand1(instruction.imm);                           // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());              // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());      // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                         // add rs1's value with sign ext'd imm
                    // check if user program is attempting to access restricted memory
                    if ((pc->read() >= program_address_range.start && pc->read() <= program_address_range.end) &&
                        (alu->getResult() < program_address_range.start || alu->getResult() > program_address_range.end) &&
                        (alu->getResult() < global_data_address_range.start || alu->getResult() > global_data_address_range.end))
                    {
                        setInterruptFlag(SF);
                    }
                    else
                    {
                        alu->setOperand1(memory->                                // load word from memory into ALU
                            template getWord<word>(alu->getResult()));  
                        alu->operate(SXT, constants->at(0x80000000).read());     // sign extend word (useful for RV64 and RV128)
                        register_set[instruction.rd].write(alu->getResult());    // store sign ext'd word into rd
                    }
                    break;

                case 0b100:  // LBU
                    alu->setOperand1(instruction.imm);                                              // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());                                 // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());                         // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                                            // add rs1's value with sign ext'd imm
                    // check if user program is attempting to access restricted memory
                    if ((pc->read() >= program_address_range.start && pc->read() <= program_address_range.end) &&
                        (alu->getResult() < program_address_range.start || alu->getResult() > program_address_range.end) &&
                        (alu->getResult() < global_data_address_range.start || alu->getResult() > global_data_address_range.end))
                    {
                        setInterruptFlag(SF);
                    }
                    else
                        { register_set[instruction.rd].write(memory->getByte(alu->getResult())); }  // load zero ext'd byte from memory into rd
                    break;

                case 0b101:  // LHU
                    alu->setOperand1(instruction.imm);                                          // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());                             // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());                     // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                                        // add rs1's value with sign ext'd imm
                    // check if user program is attempting to access restricted memory
                    if ((pc->read() >= program_address_range.start && pc->read() <= program_address_range.end) &&
                        (alu->getResult() < program_address_range.start || alu->getResult() > program_address_range.end) &&
                        (alu->getResult() < global_data_address_range.start || alu->getResult() > global_data_address_range.end))
                    {
                        setInterruptFlag(SF);
                    }
                    else
                    { 
                        register_set[instruction.rd].write(memory->                             // load zero ext'd half word from memory into rd
                            template getWord<half_word>(alu->getResult()));
                    }
                    break;

                default:
                    // funct doesn't exist in base ISA; have extensions execute instruction instead
                    success = executeFromExtensions(instruction);
                    break;
            }
            break;

        case STORE:
            switch(instruction.funct3)
            {
                case 0b000:  // SB
                    alu->setOperand1(instruction.imm);                                                // load immediate into ALU
                    alu->operate(SXT, constants->at(0x800).read());                                   // sign extend imm by bit 11
                    alu->setOperand1(register_set[instruction.rs1].read());                           // load rs1's value into ALU
                    alu->operate(ADD, alu->getResult());                                              // add rs1's value with sign ext'd imm
                    // check if user program is attempting to access restricted memory
                    if ((pc->read() >= program_address_range.start && pc->read() <= program_address_range.end) &&
                        (alu->getResult() < program_address_range.start || alu->getResult() > program_address_range.end) &&
                        (alu->getResult() < global_data_address_range.start || alu->getResult() > global_data_address_range.end))
                    {
                        setInterruptFlag(alu->getResult() == 0 ? SAZ : SF);
                    }
                    else
                        { memory->setByte(alu->getResult(), register_set[instruction.rs2].read()); }  // store LS byte of rs2's value into memory
                    break;

                case 0b001:  // SH
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
                        memory-> template setWord<half_word>                           // store LS half word of rs2's value into memory
                            (alu->getResult(), register_set[instruction.rs2].read());  
                    }
                    break;

                case 0b010:  // SW
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
                        memory-> template setWord<word>                                // store LS word of rs2's value into memory
                            (alu->getResult(), register_set[instruction.rs2].read());  
                    }
                    break;

                default:
                    // funct doesn't exist in base ISA; have extensions execute instruction instead
                    success = executeFromExtensions(instruction);
                    break;
            }
            break;

        case BRANCH:
            switch (instruction.funct3)
            {
                case 0b000:  // BEQ
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(EQ, register_set[instruction.rs2].read());  // check if equal to rs2's value
                    count = !alu->getResult();                               // pc should not count if branch is successful
                    alu->setOperand1(alu->getResult());                      // load comparison result into ALU
                    alu->operate(SXT, alu->getResult());                     // produces either all zeros or all ones
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(AND, alu->getResult());                     // perform bitwise AND with bitmask to determine branch
                    alu->setOperand1(alu->getResult());                      // load branch amount into ALU
                    alu->operate(SXT, constants->at(0x1000).read());         // sign extend branch amount by bit 12
                    alu->setOperand1(alu->getResult());                      // load sign ext'd branch about into ALU
                    alu->operate(ADD, pc->read());                           // add branch amount with address in pc
                    pc->write(alu->getResult());                             // write branch address into pc
                    break;

                case 0b001:  // BNE
                    alu->setOperand1(register_set[instruction.rs1].read());   // load rs1's value into ALU
                    alu->operate(NEQ, register_set[instruction.rs2].read());  // check if not equal to rs2's value
                    count = !alu->getResult();                                // pc should not count if branch is successful
                    alu->setOperand1(alu->getResult());                       // load comparison result into ALU
                    alu->operate(SXT, alu->getResult());                      // produces either all zeros or all ones
                    alu->setOperand1(instruction.imm);                        // load immediate into ALU
                    alu->operate(AND, alu->getResult());                      // perform bitwise AND with bitmask to determine branch
                    alu->setOperand1(alu->getResult());                       // load branch amount into ALU
                    alu->operate(SXT, constants->at(0x1000).read());          // sign extend branch amount by bit 12
                    alu->setOperand1(alu->getResult());                       // load sign ext'd branch about into ALU
                    alu->operate(ADD, pc->read());                            // add branch amount with address in pc
                    pc->write(alu->getResult());                              // write branch address into pc
                    break;

                case 0b100:  // BLT
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(LT, register_set[instruction.rs2].read());  // check if less than rs2's value
                    count = !alu->getResult();                               // pc should not count if branch is successful
                    alu->setOperand1(alu->getResult());                      // load comparison result into ALU
                    alu->operate(SXT, alu->getResult());                     // produces either all zeros or all ones
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(AND, alu->getResult());                     // perform bitwise AND with bitmask to determine branch
                    alu->setOperand1(alu->getResult());                      // load branch amount into ALU
                    alu->operate(SXT, constants->at(0x1000).read());         // sign extend branch amount by bit 12
                    alu->setOperand1(alu->getResult());                      // load sign ext'd branch about into ALU
                    alu->operate(ADD, pc->read());                           // add branch amount with address in pc
                    pc->write(alu->getResult());                             // write branch address into pc
                    break;

                case 0b101:  // BGE
                    alu->setOperand1(register_set[instruction.rs1].read());  // load rs1's value into ALU
                    alu->operate(GE, register_set[instruction.rs2].read());  // check if greater than or equal to rs2's value
                    count = !alu->getResult();                               // pc should not count if branch is successful
                    alu->setOperand1(alu->getResult());                      // load comparison result into ALU
                    alu->operate(SXT, alu->getResult());                     // produces either all zeros or all ones
                    alu->setOperand1(instruction.imm);                       // load immediate into ALU
                    alu->operate(AND, alu->getResult());                     // perform bitwise AND with bitmask to determine branch
                    alu->setOperand1(alu->getResult());                      // load branch amount into ALU
                    alu->operate(SXT, constants->at(0x1000).read());         // sign extend branch amount by bit 12
                    alu->setOperand1(alu->getResult());                      // load sign ext'd branch about into ALU
                    alu->operate(ADD, pc->read());                           // add branch amount with address in pc
                    pc->write(alu->getResult());                             // write branch address into pc
                    break;

                case 0b110:  // BLTU
                    alu->setOperand1(register_set[instruction.rs1].read());   // load rs1's value into ALU
                    alu->operate(LTU, register_set[instruction.rs2].read());  // check if unsigned less than rs2's value
                    count = !alu->getResult();                                // pc should not count if branch is successful
                    alu->setOperand1(alu->getResult());                       // load comparison result into ALU
                    alu->operate(SXT, alu->getResult());                      // produces either all zeros or all ones
                    alu->setOperand1(instruction.imm);                        // load immediate into ALU
                    alu->operate(AND, alu->getResult());                      // perform bitwise AND with bitmask to determine branch
                    alu->setOperand1(alu->getResult());                       // load branch amount into ALU
                    alu->operate(SXT, constants->at(0x1000).read());          // sign extend branch amount by bit 12
                    alu->setOperand1(alu->getResult());                       // load sign ext'd branch about into ALU
                    alu->operate(ADD, pc->read());                            // add branch amount with address in pc
                    pc->write(alu->getResult());                              // write branch address into pc
                    break;

                case 0b111:  // BGEU
                    alu->setOperand1(register_set[instruction.rs1].read());   // load rs1's value into ALU
                    alu->operate(GEU, register_set[instruction.rs2].read());  // check if unsigned greater than or equal to rs2's value
                    count = !alu->getResult();                                // pc should not count if branch is successful
                    alu->setOperand1(alu->getResult());                       // load comparison result into ALU
                    alu->operate(SXT, alu->getResult());                      // produces either all zeros or all ones
                    alu->setOperand1(instruction.imm);                        // load immediate into ALU
                    alu->operate(AND, alu->getResult());                      // perform bitwise AND with bitmask to determine branch
                    alu->setOperand1(alu->getResult());                       // load branch amount into ALU
                    alu->operate(SXT, constants->at(0x1000).read());          // sign extend branch amount by bit 12
                    alu->setOperand1(alu->getResult());                       // load sign ext'd branch about into ALU
                    alu->operate(ADD, pc->read());                            // add branch amount with address in pc
                    pc->write(alu->getResult());                              // write branch address into pc
                    break;
                
                default:
                    // funct doesn't exist in base ISA; have extensions execute instruction instead
                    success = executeFromExtensions(instruction);
                    break;
            }
            break;

        case JAL:  // JAL
            alu->setOperand1(instruction.imm);                  // load immediate into ALU
            alu->operate(SXT, constants->at(0x100000).read());  // sign extend imm by bit 20
            alu->setOperand1(alu->getResult());                 // load sign ext'd imm into ALU
            alu->operate(ADD, pc->read());                      // add offset to pc
            pc->count();                                        // count to next instruction
            register_set[instruction.rd].write(pc->read());     // store address after jump instruction into rd
            pc->write(alu->getResult());                        // jump to new address
            count = register_set[0].read();                     // pc should not count after instruction is executed
            break;

        case JALR:
            switch (instruction.funct3)
            {
                case 0b000:  // JALR
                    alu->operate(GEU, register_set[0].read());                // generate a 1 (every unsigned number is >= 0)
                    alu->operate(NOT, alu->getResult());                      // generate ...111110
                    alu->setOperand1(alu->getResult());                       // load bitmask into ALU
                    alu->operate(AND, instruction.imm);                       // set bit 0 of immediate to 0
                    alu->setOperand1(alu->getResult());                       // load aligned imm into ALU
                    alu->operate(SXT, constants->at(0x800).read());           // sign extend aligned imm by bit 11
                    alu->setOperand1(alu->getResult());                       // load sign ext'd aligned imm into ALU
                    alu->operate(ADD, register_set[instruction.rs1].read());  // add offset to rs1's value
                    pc->count();                                              // count to next instruction
                    register_set[instruction.rd].write(pc->read());           // store address after jump instruction into rd
                    pc->write(alu->getResult());                              // jump to new address
                    count = register_set[0].read();                           // pc should not count after instruction is executed
                    break;
                
                default:
                    // funct doesn't exist in base ISA; have extensions execute instruction instead
                    success = executeFromExtensions(instruction);
                    break;
            }
            break;

        case LUI:  // LUI
            alu->setOperand1(instruction.imm);                     // load immediate into ALU
            alu->operate(SXT, constants->at(0x80000000).read());   // sign extend imm by bit 31 (useful for RV64 and RV128)
            register_set[instruction.rd].write(alu->getResult());  // store sign ext'd imm into rd
            break;

        case AUIPC:  // AUIPC
            alu->setOperand1(instruction.imm);                     // load immediate into ALU
            alu->operate(SXT, constants->at(0x80000000).read());   // sign extend imm by bit 31 (useful for RV64 and RV128)
            alu->setOperand1(alu->getResult());                    // load sign ext'd imm into ALU
            alu->operate(ADD, pc->read());                         // add offset to pc's value
            register_set[instruction.rd].write(alu->getResult());  // store new address in rd
            break;

        case ENVIRONMENT:
            switch (instruction.funct3)
            {
            case 0b000:
                if((instruction.imm & 1) == 0)  // ECALL
                {
                    setInterruptFlag(EC);
                }
                else  // EBREAK
                {
                    setInterruptFlag(EB);
                    count = false;  // pc will count after interrupt handler handles debugger
                }
                break;
            
            default:
                // funct doesn't exist in base ISA; have extensions execute instruction instead
                success = executeFromExtensions(instruction);
                break;
            }
            break;
        
        default:
            // opcode doesn't exist in base ISA; have extensions execute instruction instead
            success = executeFromExtensions(instruction);
            break;
    }

    if(success && count) { pc->count(); }

    if (!success) { setInterruptFlag(II); }

    return success;
}

template <typename word_size>
bool RISC_V<word_size>::executeFromExtensions(dec_instr_t instruction)
{
    if(extensions != NULL) 
    {
        byte idx = 0;
        bool success = false;
        while(idx < extensions->size() && !success)
        {
            success = (extensions->at(idx++))->execute(instruction);
        }
        return success;
    }
    else { return false; }  // there are no extensions to execute from
}

template <typename word_size>
void RISC_V<word_size>::handleInterrupts()
{
    byte interruptFlags = memory->getByte(1);  // interrupt flags is a byte stored in address 1

    if ((interruptFlags & SAZ) != 0)  // there was an attempt to store data in address 0 (reserved for NULL pointers)
    {
        printf("EXCEPTION: Attempted to write to NULL\n");
        printf(sizeof(word_size) <= 4 ? "PC = %X\n" : "PC = %llX\n", pc->read());
        printf("Terminating program...\n");
        running = false;
    }
    else if ((interruptFlags & II) != 0)  // CPU encountered an illegal instruction
    {
        printf("EXCEPTION: Illegal Instruction: %08X\n", ir->read());
        printf(sizeof(word_size) <= 4 ? "PC = %X\n" : "PC = %llX\n", pc->read());
        printf("Terminating program...\n");
        running = false;
    }
    else if ((interruptFlags & SF) != 0)  // user program is attempting to access restricted memory space
    {
        printf("EXCEPTION: Segmentation Fault\n");
        printf(sizeof(word_size) <= 4 ? "PC = %X\n" : "PC = %llX\n", pc->read());
        printf("Terminating program...\n");
        running = false;
    }
    else if ((interruptFlags & MSP) != 0)  // user program is attempting to modify stack pointer
    {
        printf("EXCEPTION: Attempted to modify Stack Pointer\n");
        printf(sizeof(word_size) <= 4 ? "PC = %X\n" : "PC = %llX\n", pc->read());
        printf("Terminating program...\n");
        running = false;
    }
    else if ((interruptFlags & EB) != 0)  // user program called EBREAK
    {
        clearInterruptFlag(EB);
        debugger();
        if ((interruptFlags & TP) == 0) { pc->count(); }
    }
    else if ((interruptFlags & EC) != 0)  // user program or interrupt handler program called ECALL
    {
        clearInterruptFlag(EC);
        static word_size return_address = 0;
        // a call to ECALL from the user program jumps the pc to the interrupt handler program
        if (pc->read() >= program_address_range.start && pc->read() <= program_address_range.end)
        {
            return_address = pc->read();
            // jump to interrupt handler program
            pc->write(interrupt_handler_address_range.start);
        }
        // a call to ECALL from the interrupt handler program jumps the pc back to the user program
        else if (pc->read() >= interrupt_handler_address_range.start && pc->read() <= interrupt_handler_address_range.end)
        {
            pc->write(return_address);
        }
        // a call to ECALL from any other memory location is not allowed and will terminate the program
        else
        {
            printf("EXCEPTION: Illegal Use of ECALL\n");
            printf(sizeof(word_size) <= 4 ? "PC = %X\n" : "PC = %llX\n", pc->read());
            printf("Terminating program...\n");
            running = false;
        }
    }
    
    if ((interruptFlags & TP) != 0)
    {
        printf("Exit command called\n");
        printf(sizeof(word_size) <= 4 ? "PC = %X\n" : "PC = %llX\n", pc->read());
        printf("Terminating program...\n");
        running = false;
    }
    else if ((interruptFlags & RP) != 0)
    {
        printf("Restart command called\n");
        printf(sizeof(word_size) <= 4 ? "PC = %X\n" : "PC = %llX\n", pc->read());
        printf("Restarting program...\n");
        running = false;
        restarting = true;
    }
}

template <typename word_size>
void RISC_V<word_size>::start()
{
    // initialize registers and memory
    pc->write(0);
    ir->write(0);
    for(byte i = 0; i < num_registers; i++) { register_set[i].write(0); }
    memory->clear();

    if (!loadMemory())  // load programs and data into memory
    {
        printf("EXCEPTION: Unable to load programs into memory\nTerminating program...\n");
        return;
    }

    pc->write(bootloader_address_range.start);  // PC should start execution from the bootloader's address for initialization
    running = true;
    restarting = false;
    dec_instr_t decoded_instruction;
    while(running)  // continously fetch, decode, and execute until an exception or interrupt occurs
    {
        fetch();
        decoded_instruction = decode();
        execute(decoded_instruction);
        handleInterrupts();
    }

    if (restarting) { start(); }
    
    return;
}

template <typename word_size>
RISC_V_Components<word_size> RISC_V<word_size>::getComponents()
{
    RISC_V_Components<word_size> components;
    components.pc = pc;
    components.ir = ir;
    components.alu = alu;
    components.register_set = register_set;
    components.constants = constants;
    components.memory = memory;

    return components;
}

template <typename word_size>
void RISC_V<word_size>::setInterruptFlag(interrupt_flag flag)
{
    memory->setByte(1, memory->getByte(1) | flag);
}

template <typename word_size>
void RISC_V<word_size>::clearInterruptFlag(interrupt_flag flag)
{
    memory->setByte(1, memory->getByte(1) & ~flag);
}