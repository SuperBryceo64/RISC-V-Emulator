#ifndef MEMORY_H
#define MEMORY_H

#include <unordered_map>
#include "../Utilities/DataTypes.h"

typedef enum
{
    LITTLE,
    BIG
} endian_t;

template <typename address_size = word>
class Memory
{
    public:
        Memory();
        Memory(endian_t endian);
        ~Memory();
        void clear();  // clears all data stored in memory
        byte getByte(address_size address);
        template <typename word_size = address_size>
            word_size getWord(address_size address);  // word as in word_size, not necessarily 32 bits
        void setByte(address_size address, byte data);
        template <typename word_size = address_size>
            void setWord(address_size address, word_size data);  // word as in word_size, not necessarily 32 bits
        void printByte(address_size address, bool endline = true, base_t base = HEX);
        template <typename word_size = address_size>
            void printWord(address_size address, bool endline = true, base_t base = HEX);  // word as in word_size, not necessarily 32 bits

    private:
        std::unordered_map<address_size, byte> *RAM;
        endian_t endian;
};

#endif