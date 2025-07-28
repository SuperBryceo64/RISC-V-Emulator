#include "Multiplier.h"

template<typename word_size>
Multiplier<word_size>::Multiplier() : ALU<word_size>() {}

template<typename word_size>
void Multiplier<word_size>::operate(operation_t operation, word_size operand2)
{
    switch (operation)
    {
        case MUL:  // Multiplies the operands and stores the lower word size in result register
            result.write(multiply_unsigned(operand1.read(), operand2).second);
            break;

        case MULH:  // Multiplies the operands as signed numbers and stores the upper word size in result register
        {
            std::pair<word_size, word_size> product;
            word_size operand1_temp = operand1.read();
            byte bit_size = sizeof(word_size) * 8;
            bool negate = false;

            // check if either operand is negative
            if (((operand1_temp >> bit_size-1) & 1) == 1) { operand1_temp = -operand1_temp; negate = !negate; }
            if (((operand2 >> bit_size-1) & 1) == 1) { operand2 = -operand2; negate = !negate; }

            // calculate unsigned product
            product = multiply_unsigned(operand1_temp, operand2);

            // negate product if needed
            if (negate)
            {
                product.first = ~product.first;
                product.second = ~product.second;
                product.first += (product.second == (word_size)(-1)) ? 1 : 0;
            }

            result.write(product.first);
            break;
        }    

        case MULHU:  // Multiplies the operands as unsigned numbers and stores the upper word size in result register
            result.write(multiply_unsigned(operand1.read(), operand2).first);
            break;

        case MULHSU:  // Multiplies operand1 as signed and operand2 as unsigned and stores the upper word size in result register
        {
            std::pair<word_size, word_size> product;
            word_size operand1_temp = operand1.read();
            byte bit_size = sizeof(word_size) * 8;
            bool negate = false;

            // check if operand1 is negative
            if (((operand1_temp >> bit_size-1) & 1) == 1) { operand1_temp = -operand1_temp; negate = !negate; }

            // calculate unsigned product
            product = multiply_unsigned(operand1_temp, operand2);

            // negate product if needed
            if (negate)
            {
                product.first = ~product.first;
                product.second = ~product.second;
                product.first += (product.second == (word_size)(-1)) ? 1 : 0;
            }

            result.write(product.first);
            break;
        }

        case DIV:  // Divides the operands as signed numbers and stores the quotient (rounded to zero) in result register
        {    
            word_size quotient;
            word_size operand1_temp = operand1.read();
            byte bit_size = sizeof(word_size) * 8;
            bool negate = false;

            // check if either operand is negative
            if (((operand1_temp >> bit_size-1) & 1) == 1) { operand1_temp = -operand1_temp; negate = !negate; }
            if (((operand2 >> bit_size-1) & 1) == 1) { operand2 = -operand2; negate = !negate; }

            // calculate unsigned quotient
            quotient = divide_unsigned(operand1_temp, operand2).first;

            // negate quotient if needed
            quotient = (negate && quotient != (word_size)(-1)) ? -quotient : quotient;

            result.write(quotient);
            break;
        }

        case DIVU:  // Divides the operands as unsigned numbers and stores the quotient (rounded to zero) in result register
            result.write(divide_unsigned(operand1.read(), operand2).first);
            break;

        case REM:  // Takes the remainder of operand1 / operand2 as signed numbers and stores in result register
        {    
            word_size remainder;
            word_size operand1_temp = operand1.read();
            byte bit_size = sizeof(word_size) * 8;
            bool negate = false;

            // check if either operand is negative (only dividend negates the remainder)
            if (((operand1_temp >> bit_size-1) & 1) == 1) { operand1_temp = -operand1_temp; negate = true; }
            if (((operand2 >> bit_size-1) & 1) == 1) { operand2 = -operand2; }

            // calculate unsigned remainder
            remainder = divide_unsigned(operand1_temp, operand2).second;

            // negate remainder if needed
            remainder = negate ? -remainder : remainder;

            result.write(remainder);
            break;
        }

        case REMU:  // Takes the remainder of operand1 / operand2 as unsigned numbers and stores in result register
            result.write(divide_unsigned(operand1.read(), operand2).second);
            break;

        case SXT:
            ALU<word_size>::operate(SXT, operand2);
            break;

        case AND:
            ALU<word_size>::operate(AND, operand2);
            break;

        default:
            break;
    }
}

template<typename word_size>
std::pair<word_size, word_size> Multiplier<word_size>::multiply_unsigned(word_size multiplicand, word_size multiplier)
{
    word_size product_high = 0, product_low = 0;
    const byte bit_size = sizeof(word_size) * 8;
    byte bit_pos = 0;
    bool carry;
    while (multiplier != 0)
    {
        carry = ((multiplier & 1) == 1) && 
                (((word_size)(product_low + (multiplicand << bit_pos)) < product_low) || 
                 ((word_size)(product_low + (multiplicand << bit_pos)) < (word_size)(multiplicand << bit_pos)));
        product_low += ((multiplier & 1) == 1) ? (word_size)(multiplicand << bit_pos) : 0;
        product_high += ((multiplier & 1) == 1 && bit_pos > 0) ? ((multiplicand >> (bit_size - bit_pos)) + carry) : 0;
        bit_pos++;
        multiplier >>= 1;
    }

    return std::pair<word_size, word_size>(product_high, product_low);
}

template<typename word_size>
std::pair<word_size, word_size> Multiplier<word_size>::divide_unsigned(word_size dividend, word_size divisor)
{
    word_size quotient = 0, remainder = 0;
    word_size partial_dividend = 0;
    const byte bit_size = sizeof(word_size) * 8;
    s_byte bit_pos = bit_size - 1;

    // a divisor of zero should set the quotient to -1 (FFFF...FF) and remainder to dividend 
    if (divisor == 0) { return std::pair<word_size, word_size>((word_size)(-1), dividend); }
    // if divisor is larger than dividend, quotient is 0 and remainder is dividend
    if (divisor > dividend) { return std::pair<word_size, word_size>(0, dividend); }
    // if divisor and dividend are equal, quotient is 1 and remainder is 0
    if (divisor == dividend) { return std::pair<word_size, word_size>(1, 0); }

    while (divisor > (dividend >> bit_pos)) { bit_pos--; }

    partial_dividend = dividend >> bit_pos;
    while (bit_pos >= 0)
    {
        if (partial_dividend >= divisor)
        {
            quotient |= (1 << bit_pos);
            partial_dividend -= divisor;
        }

        if (bit_pos > 0)
        {
            partial_dividend <<= 1;
            partial_dividend |= ((dividend >> --bit_pos) & 1);
        }
        else { bit_pos--; }
    }
    remainder = partial_dividend;

    return std::pair<word_size, word_size>(quotient, remainder);
}