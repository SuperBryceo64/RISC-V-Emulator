#ifndef COMBINE_FUNCT_H
#define COMBINE_FUNCT_H

#include "DataTypes.h"

// Combines funct3 and funct7 into a 10-bit value: funct3[2:0] | funct7[6:0]
half_word combineFunct(byte funct3, byte funct7)
{
    return (((half_word) funct3) << 7) | (funct7 & 127);
}

#endif