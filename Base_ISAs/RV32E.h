#ifndef RV32E_H
#define RV32E_H

#include "../Components/RISC_V.h"

class RV32E : public RISC_V<word>
{
    using RISC_V<word>::base;
    
    public:
        RV32E(endian_t endian = LITTLE);
        RV32E(endian_t endian, ExtensionList<word> &extension_list);
        RV32E(ExtensionList<word> &extension_list);
        ~RV32E();

    private:
        dec_instr_t decode() override;
};

#endif