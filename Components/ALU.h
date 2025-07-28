#ifndef ALU_H
#define ALU_H

#include "../Utilities/DataTypes.h"
#include "Register.h"

template <typename word_size = word>
class ALU
{
    public:
        ALU();
        word_size getOperand1();
        void setOperand1(word_size new_operand1);
        virtual void operate(operation_t operation, word_size operand2);
        word_size getResult();

    protected:
        Register<word_size> operand1;
        Register<word_size> result;
};

#endif