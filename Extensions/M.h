#ifndef M_H
#define M_H

#include "../Components/Multiplier.h"
#include "Extension.h"

template <typename word_size = word>
class M : public Extension<word_size>
{
    using Extension<word_size>::name;
    using Extension<word_size>::cpu_pc;
    using Extension<word_size>::cpu_ir;
    using Extension<word_size>::cpu_alu;
    using Extension<word_size>::cpu_register_set;
    using Extension<word_size>::cpu_constants;
    using Extension<word_size>::cpu_memory;
    
    public:
        M();
        M(RISC_V_Components<word_size> &cpu_components);
        ~M();
        M<word_size>* create(RISC_V_Components<word_size> &cpu_components) override;
        dec_instr_t decode() override;
        bool execute(dec_instr_t instruction) override;
};

#endif