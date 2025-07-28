#include "RV32I.h"

RV32I::RV32I(endian_t endian) : RISC_V<word>(32, endian) { base = "RV32I"; }

RV32I::RV32I(endian_t endian, ExtensionList<word> &extension_list) : RISC_V<word>(32, endian, extension_list)
    { base = "RV32I"; }

RV32I::RV32I(ExtensionList<word> &extension_list) : RISC_V<word>(32, LITTLE, extension_list)
    { base = "RV32I"; }

RV32I::~RV32I() {}