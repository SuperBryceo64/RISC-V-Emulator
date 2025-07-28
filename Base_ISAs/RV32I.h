#ifndef RV32I_H
#define RV32I_H

#include "../Components/RISC_V.h"

class RV32I : public RISC_V<word>
{
    using RISC_V<word>::base;
    
    public:
        RV32I(endian_t endian = LITTLE);
        RV32I(endian_t endian, ExtensionList<word> &extension_list);
        RV32I(ExtensionList<word> &extension_list);
        ~RV32I();
};

#endif