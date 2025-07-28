#ifndef COUNTER_H
#define COUNTER_H

#include "../Utilities/DataTypes.h"
#include "Register.h"

template <typename reg_size = word>
class Counter : public Register<reg_size>
{
    using Register<reg_size>::value;
    
    public:
        Counter();
        Counter(reg_size count_value);
        void increment();
        void count();
    
    private:
        reg_size count_value;
};

#endif