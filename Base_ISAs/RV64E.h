#ifndef RV64E_H
#define RV64E_H

#include "../Components/RISC_V.h"

class RV64E : public RISC_V<double_word>
{
    using RISC_V<double_word>::base;
    
    public:
        RV64E(endian_t endian = LITTLE);
        RV64E(endian_t endian, ExtensionList<double_word> &extension_list);
        RV64E(ExtensionList<double_word> &extension_list);
        ~RV64E();

    private:
        dec_instr_t decode() override;
        bool execute(dec_instr_t instruction) override;
};

#endif