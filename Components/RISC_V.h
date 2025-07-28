#ifndef RISC_V_H
#define RISC_V_H

#include <vector>
#include <string>
#include <map>
#include "../Utilities/DataTypes.h"
#include "../Utilities/DecodeAndEncodeInstructionFromFormat.h"
#include "../Utilities/CombineFunct.h"
#include "Register.h"
#include "Counter.h"
#include "ALU.h"
#include "Memory.h"
#include "../Extensions/Extension.h"

template <typename word_size = word>
using ConstantList = std::map<word_size, Register<word_size>>;

template <typename word_size = word>
using ExtensionList = std::vector<Extension<word_size>*>;

template <typename word_size = word>
class RISC_V
{
    public:
        RISC_V(byte number_of_registers = 32, endian_t endian = LITTLE);
        RISC_V(byte number_of_registers, endian_t endian, ExtensionList<word_size> &extension_list);
        RISC_V(ExtensionList<word_size> &extension_list);
        virtual ~RISC_V() = 0;  // pure virtual destructor ensures class is abstract and can't be instantiated
        virtual void start();
    
    protected:
        Counter<word_size> *pc;  // program counter
        Register<word> *ir;  // instruction register
        ALU<word_size> *alu;  // arithmetic logic unit
        Register<word_size> *register_set;
        ConstantList<word_size> *constants;
        Memory<word_size> *memory;
        ExtensionList<word_size> *extensions;  // list of ISA extensions

        std::string base;
        byte num_registers;
        bool running;
        bool restarting;

        AddressRange<word_size> bootloader_address_range;
        AddressRange<word_size> program_address_range;
        AddressRange<word_size> global_data_address_range;
        AddressRange<word_size> interrupt_handler_address_range;
        
        virtual bool loadMemory();
        virtual void debugger();  // a special debugger routine that runs when EBREAK is called

        virtual void fetch();
        virtual dec_instr_t decode();  // returns valid instruction for a successful decoding
        virtual bool execute(dec_instr_t instruction);  // returns true for a successful execution
        virtual void handleInterrupts();  // handles any traps that are raised

        bool executeFromExtensions(dec_instr_t instruction);  // calls execute() from extensions

        enum menu_options
        {
            READ_REGISTERS    = 1,
            READ_MEMORY       = 2,
            CONTINUE_PROGRAM  = 3,
            TERMINATE_PROGRAM = 4,
            RESTART_PROGRAM   = 5
        };

        enum interrupt_flag
        {
            SAZ = 0b00000001,
            II  = 0b00000010,
            SF  = 0b00000100,
            MSP = 0b00001000,
            EB  = 0b00010000,
            EC  = 0b00100000,
            TP  = 0b01000000,
            RP  = 0b10000000
        };

        virtual void setInterruptFlag(interrupt_flag flag);
        virtual void clearInterruptFlag(interrupt_flag flag);

    private:
        RISC_V_Components<word_size> getComponents();
};

#endif