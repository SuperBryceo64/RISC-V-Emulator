#include <stdio.h>
#include "RISC-V_Emulator.h"

int main()
{
    M<word> M_ext32;
    M<double_word> M_ext64;
    ExtensionList<word> extensions32 = {&M_ext32};
    ExtensionList<double_word> extensions64 = {&M_ext64};
    endian_t endian32 = BIG, endian64 = LITTLE;
    RV32I cpu32I(endian32, extensions32);
    RV32E cpu32E;
    RV64I cpu64I(endian64, extensions64);
    RV64E cpu64E;

    // assemble();
    // hexDump("./Programs/interrupt_handler");
    // assemble("./Programs/bootloader.s", LITTLE);
    if(assemble(endian32)) { cpu32I.start(); }
    // if(assemble()) { cpu32E.start(); }
    // if(assemble<double_word>(endian64)) { cpu64I.start(); }
    // if(assemble<double_word>()) { cpu64E.start(); }
    return 0;
}