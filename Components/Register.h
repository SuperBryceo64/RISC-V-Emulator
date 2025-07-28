#ifndef REGISTER_H
#define REGISTER_H

#include "../Utilities/DataTypes.h"

template <typename reg_size = word>
class Register
{
    public:
        Register();
        Register(reg_size value, bool is_const_reg);
        reg_size read();
        void write(reg_size data);
    
    protected:
        reg_size value;

    private:
        bool is_const_reg;
};

#endif