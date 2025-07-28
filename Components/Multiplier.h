#ifndef MULTIPLIER_H
#define MULTIPLIER_H

#include <utility>
#include "ALU.h"

template <typename word_size = word>
class Multiplier : public ALU<word_size>
{
    using ALU<word_size>::operand1;
    using ALU<word_size>::result;

    public:
        Multiplier();
        void operate(operation_t operation, word_size operand2) override;
    
    private:
        // Performs unsigned multiplication and returns pair(upper word_size, lower word_size)
        std::pair<word_size, word_size> multiply_unsigned(word_size operand1, word_size operand2);
        // Performs unsigned division and returns pair(quotient, remainder)
        std::pair<word_size, word_size> divide_unsigned(word_size operand1, word_size operand2);
};

#endif