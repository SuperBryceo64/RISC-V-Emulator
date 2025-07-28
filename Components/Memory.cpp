#include "Memory.h"
#include <stdio.h>

template<typename address_size>
Memory<address_size>::Memory() : endian(LITTLE) { RAM = new std::unordered_map<address_size, byte>(); }

template<typename address_size>
Memory<address_size>::Memory(endian_t endian) : endian(endian) { RAM = new std::unordered_map<address_size, byte>(); }

template<typename address_size>
Memory<address_size>::~Memory(){ delete RAM; }

template<typename address_size>
void Memory<address_size>::clear()
{
    delete RAM;
    RAM = new std::unordered_map<address_size, byte>();
}

template<typename address_size>
byte Memory<address_size>::getByte(address_size address)
{
    if(RAM->count(address) == 0) { (*RAM)[address] = 0; }
    return RAM->at(address);
}

template<typename address_size>
template<typename word_size>
word_size Memory<address_size>::getWord(address_size address)
{
    word_size data = 0;
    switch(endian)
    {
        case LITTLE:
            for(byte i = 0; i < sizeof(word_size); i++)
            {
                data += (((word_size) getByte(address++)) << (i * 8));
            }
            break;
        
        case BIG:
            for(byte i = 0; i < sizeof(word_size); i++)
            {
                data += getByte(address++);
                if(i < (byte) sizeof(word_size) - 1) { data = data << 8; }
            }
            break;
    }
    return data;
}

template<typename address_size>
void Memory<address_size>::setByte(address_size address, byte data)
{
    if (address != 0) { (*RAM)[address] = data; }
    else { (*RAM)[1] |= 1; }  // attempting to store to address 0 will set the SAZ flag in the interrupt flags
}

template<typename address_size>
template <typename word_size>
void Memory<address_size>::setWord(address_size address, word_size data)
{
     // attempting to store to address 0 will set the SAZ flag in the interrupt flags
    if (address == 0)
    {
        (*RAM)[1] |= 1;
        return;
    }

    switch(endian)
    {
        case LITTLE:
            for(byte i = 0; i < sizeof(word_size); i++)
            {
                setByte(address++, (byte) data);  // data is truncated to least significant byte
                data = data >> 8;
            }
            break;

        case BIG:
            for(s_byte i = sizeof(word_size) - 1; i >= 0; i--) { setByte(address++, (byte) (data >> (8 * i))); }
            break;
    }
}

template<typename address_size>
void Memory<address_size>::printByte(address_size address, bool endline, base_t base)
{
    byte data = getByte(address);
    switch(base)
    {
        case BIN:
            for(s_byte i = 7; i >= 0; i--) { printf("%u", (data >> i) & 1); }
            printf(" ");
            break;
        
        case OCT:
            printf("%o ", data);
            break;
        
        case DEC:
            printf("%u ", data);
            break;
        
        case HEX:
            printf("%02X ", data);
            break;
    }
    if(endline) { printf("\n"); }
}

template<typename address_size>
template <typename word_size>
void Memory<address_size>::printWord(address_size address, bool endline, base_t base)
{
    switch(endian)
    {
        case LITTLE:
            for(s_byte i = sizeof(word_size) - 1; i >= 0; i--) { printByte(address + i, false, base); }
            break;
        
        case BIG:
            for(byte i = 0; i < sizeof(word_size); i++) { printByte(address + i, false, base); }
            break;
    }
    if(endline) { printf("\n"); }
}