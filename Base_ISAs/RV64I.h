#ifndef RV64I_H
#define RV64I_H

#include "../Components/RISC_V.h"

class RV64I : public RISC_V<double_word>
{
    using RISC_V<double_word>::base;
    
    public:
        RV64I(endian_t endian = LITTLE);
        RV64I(endian_t endian, ExtensionList<double_word> &extension_list);
        RV64I(ExtensionList<double_word> &extension_list);
        ~RV64I();
    
    private:
        dec_instr_t decode() override;
        bool execute(dec_instr_t instruction) override;
};

#endif