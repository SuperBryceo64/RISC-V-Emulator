#ifndef EXTENSION_H
#define EXTENSION_H

#include <string>
#include <map>
#include "../Utilities/DataTypes.h"
#include "../Utilities/DecodeAndEncodeInstructionFromFormat.h"
#include "../Utilities/CombineFunct.h"
#include "../Components/Register.h"
#include "../Components/Counter.h"
#include "../Components/ALU.h"
#include "../Components/Memory.h"

template <typename word_size = word>
struct RISC_V_Components
{
    Counter<word_size> *pc;
    Register<word> *ir;
    ALU<word_size> *alu;
    Register<word_size> *register_set;
    std::map<word_size, Register<word_size>> *constants;
    Memory<word_size> *memory;
};

template <typename word_size = word>
class Extension
{
    public:
        Extension();
        Extension(RISC_V_Components<word_size> &cpu_components);
        virtual ~Extension();
        // allows derived classes to create new objects and assign it the cpu's components
        virtual Extension<word_size>* create(RISC_V_Components<word_size> &cpu_components) = 0;
        virtual dec_instr_t decode() = 0;  // returns valid instruction for a successful decoding
        virtual bool execute(dec_instr_t instruction) = 0;  // returns true for a successful execution
        virtual std::string getName();
    
    protected:
        std::string name;
        Counter<word_size> *cpu_pc;  // CPU's program counter
        Register<word> *cpu_ir;  // CPU's instruction register
        ALU<word_size> *cpu_alu;  // CPU's arithmetic logic unit
        Register<word_size> *cpu_register_set;
        std::map<word_size, Register<word_size>> *cpu_constants;
        Memory<word_size> *cpu_memory;
};

#endif