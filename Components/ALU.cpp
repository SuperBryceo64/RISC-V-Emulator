#include "ALU.h"

template<typename word_size>
ALU<word_size>::ALU() : operand1(), result() {}

template<typename word_size>
word_size ALU<word_size>::getOperand1() { return operand1.read(); }

template<typename word_size>
void ALU<word_size>::setOperand1(word_size new_operand1) { operand1.write(new_operand1); }

template<typename word_size>
void ALU<word_size>::operate(operation_t operation, word_size operand2) 
{
    switch(operation)
    {
        case ADD:    // Add operands and store in result register
            result.write(operand1.read() + operand2);
            break;

        case SUB:    // Subtract operands and store in result register
            result.write(operand1.read() - operand2);
            break;

        case AND:    // Perform bitwise AND on operands and store in result register
            result.write(operand1.read() & operand2);
            break;

        case OR:     // Perform bitwise OR on operands and store in result register
            result.write(operand1.read() | operand2);
            break;

        case NOT:    // Perform bitwise NOT on operand2 and store in result register
            result.write(~operand2);
            break;

        case XOR:    // Perform bitwise XOR on operands and store in result register
            result.write(operand1.read() ^ operand2);
            break;

        case EQ:     // If operands are equal, store a '1' in the result register; else store a '0'
            result.write(operand1.read() == operand2);
            break;

        case NEQ:    // If operands are not equal, store a '1' in the result register; else store a '0'
            result.write(operand1.read() != operand2);
            break;

        case LT:     // If operand1 is less than operand2 (assuming they are signed), store a '1' in the result register; else store a '0'
        {
            byte bit_size = sizeof(word_size) * 8;
            byte operand1_sign = (operand1.read() >> (bit_size-1)) & 1, operand2_sign = (operand2 >> (bit_size-1)) & 1;
            // if operand1 is negative and operand2 is non-negative, then comparison is true
            if(operand1_sign == 1 && operand2_sign == 0) { result.write(1); }
            // if operand1 is non-negative and operand2 is negative, then comparison is false
            else if(operand1_sign == 0 && operand2_sign == 1) { result.write(0); }
            // otherwise, we have to check bit-by-bit
            else
            {
                word_size operand1_temp = operand1.read() << 1, operand2_temp = operand2 << 1;
                byte operand1_bit, operand2_bit;
                while(operand1_temp != 0 || operand2_temp != 0)
                {
                    operand1_bit = (operand1_temp >> (bit_size-1)) & 1;
                    operand2_bit = (operand2_temp >> (bit_size-1)) & 1;
                    if(operand1_bit == 0 && operand2_bit == 1) 
                    {
                        result.write(1);
                        break;
                    }
                    else if(operand1_bit == 1 && operand2_bit == 0) {
                        result.write(0);
                        break;
                    }
                    else {
                        operand1_temp = operand1_temp << 1;
                        operand2_temp = operand2_temp << 1;
                    }
                }
                // If every bit has been parsed without a decision, then the operands are equal (so result should be '0')
                if(operand1_temp == 0 && operand2_temp == 0) { result.write(0); }
            }
            break;
        }

        case LTU:    // If operand1 is less than operand2 (assuming they are unsigned), store a '1' in the result register; else store a '0'
        {
            byte bit_size = sizeof(word_size) * 8;
            word_size operand1_temp = operand1.read(), operand2_temp = operand2;
            byte operand1_bit, operand2_bit;
            while(operand1_temp != 0 || operand2_temp != 0)
            {
                operand1_bit = (operand1_temp >> (bit_size-1)) & 1;
                operand2_bit = (operand2_temp >> (bit_size-1)) & 1;
                if(operand1_bit == 0 && operand2_bit == 1) 
                {
                    result.write(1);
                    break;
                }
                else if(operand1_bit == 1 && operand2_bit == 0)
                {
                    result.write(0);
                    break;
                }
                else
                {
                    operand1_temp = operand1_temp << 1;
                    operand2_temp = operand2_temp << 1;
                }
            }
            // If every bit has been parsed without a decision, then the operands are equal (so result should be '0')
            if(operand1_temp == 0 && operand2_temp == 0) { result.write(0); }
            break;
        }

        case GE:    // If operand1 is greater than or equal to operand2 (assuming they are signed), store a '1' in the result register; else store a '0'
        {
            byte bit_size = sizeof(word_size) * 8;
            byte operand1_sign = (operand1.read() >> (bit_size-1)) & 1, operand2_sign = (operand2 >> (bit_size-1)) & 1;
            // if operand1 is non-negative and operand2 is negative, then comparison is true
            if(operand1_sign == 0 && operand2_sign == 1) { result.write(1); }
            // if operand1 is negative and operand2 is non-negative, then comparison is false
            else if(operand1_sign == 1 && operand2_sign == 0) { result.write(0); }
            // otherwise, we have to check bit-by-bit
            else
            {
                word_size operand1_temp = operand1.read() << 1, operand2_temp = operand2 << 1;
                byte operand1_bit, operand2_bit;
                while(operand1_temp != 0 || operand2_temp != 0)
                {
                    operand1_bit = (operand1_temp >> (bit_size-1)) & 1;
                    operand2_bit = (operand2_temp >> (bit_size-1)) & 1;
                    if(operand1_bit == 1 && operand2_bit == 0) 
                    {
                        result.write(1);
                        break;
                    }
                    else if(operand1_bit == 0 && operand2_bit == 1) {
                        result.write(0);
                        break;
                    }
                    else {
                        operand1_temp = operand1_temp << 1;
                        operand2_temp = operand2_temp << 1;
                    }
                }
                // If every bit has been parsed without a decision, then the operands are equal (so result should be '1')
                if(operand1_temp == 0 && operand2_temp == 0) { result.write(1); }
            }
            break;
        }

        case GEU:   // If operand1 is greater than or equal to operand2 (assuming they are unsigned), store a '1' in the result register; else store a '0'
        {
            byte bit_size = sizeof(word_size) * 8;
            word_size operand1_temp = operand1.read(), operand2_temp = operand2;
            byte operand1_bit, operand2_bit;
            while(operand1_temp != 0 || operand2_temp != 0)
            {
                operand1_bit = (operand1_temp >> (bit_size-1)) & 1;
                operand2_bit = (operand2_temp >> (bit_size-1)) & 1;
                if(operand1_bit == 1 && operand2_bit == 0) 
                {
                    result.write(1);
                    break;
                }
                else if(operand1_bit == 0 && operand2_bit == 1) {
                    result.write(0);
                    break;
                }
                else {
                    operand1_temp = operand1_temp << 1;
                    operand2_temp = operand2_temp << 1;
                }
            }
            // If every bit has been parsed without a decision, then the operands are equal (so result should be '1')
            if(operand1_temp == 0 && operand2_temp == 0) { result.write(1); }
            break;
        }

        case SLL:    // Perform a logical left shift on operands and store in result register
        {   
            byte bit_size = sizeof(word_size) * 8;
            word_size operand2_temp = operand2;
            while(bit_size > 1) 
            { 
                operand2_temp = operand2_temp >> 1;
                bit_size /= 2;
            }
            if(operand2_temp != 0) { result.write(0); }
            else { result.write(operand1.read() << operand2); }
            break;
        }

        case SRL:    // Perform a logical right shift on operands and store in result register
        {
            byte bit_size = sizeof(word_size) * 8;
            byte bit_size_temp = bit_size;
            word_size operand2_temp = operand2;
            word_size bitmask = 0 - 1;
            while(bit_size_temp > 1) 
            { 
                operand2_temp = operand2_temp >> 1;
                bit_size_temp /= 2;
            }
            if(operand2_temp != 0) { result.write(0); }
            else 
            {   bitmask = ~((~(bitmask << operand2)) << (bit_size - operand2)); 
                result.write((operand1.read() >> operand2) & bitmask); 
            }
            break;
        }

        case SRA:    // Perform an arithmetic right shift on operands and store in result register
        {
            byte bit_size = sizeof(word_size) * 8;
            byte bit_size_temp = bit_size;
            byte operand1_sign = (operand1.read() >> (bit_size-1)) & 1;
            word_size operand2_temp = operand2;
            word_size bitmask = 0 - 1;
            word_size result_temp;
            while(bit_size_temp > 1) 
            { 
                operand2_temp = operand2_temp >> 1;
                bit_size_temp /= 2;
            }
            if(operand2_temp != 0) { result.write(0-operand1_sign); }
            else 
            {   bitmask = (~(bitmask << operand2)) << (bit_size - operand2);
                bitmask = operand1_sign ? bitmask : ~bitmask;
                result_temp = operand1.read() >> operand2;
                result_temp = operand1_sign ? (result_temp | bitmask) : (result_temp & bitmask);
                result.write(result_temp); 
            }
            break;
        }

        case SXT:    // Perform a sign extension on operand1 where the sign bit is the MSB that is '1' on operand2 and store in result register
            // An operand2 of 0 indicates that the result should equal operand1
            if(operand2 == 0) { result.write(operand1.read()); }
            else
            {
                byte bit_size = sizeof(word_size) * 8;
                word_size bitmask = 0 - 1;
                byte num_shifts = 0;
                word_size operand1_temp = operand1.read();
                byte operand1_sign;
                word_size operand2_temp = operand2;
                byte operand2_bit;
                word_size result_temp = operand1.read();
                do
                {
                    operand1_sign = (operand1_temp >> (bit_size-1)) & 1;
                    operand2_bit = (operand2_temp >> (bit_size-1)) & 1;
                    bitmask = bitmask << 1;
                    num_shifts++;
                    operand1_temp = operand1_temp << 1;
                    operand2_temp = operand2_temp << 1;
                } while (operand2_bit == 0);
                bitmask = ~bitmask << (bit_size - num_shifts);
                bitmask = operand1_sign ? bitmask : ~bitmask;
                result_temp = operand1_sign ? (result_temp | bitmask) : (result_temp & bitmask);
                result.write(result_temp);
            }
            break;

        default:
            break;
    }
}

template<typename word_size>
word_size ALU<word_size>::getResult() { return result.read(); }