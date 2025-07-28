#include "Register.h"

template<typename reg_size>
Register<reg_size>::Register() : value(0), is_const_reg(false) {}

template<typename reg_size>
Register<reg_size>::Register(reg_size value, bool is_const_reg) : value(value), is_const_reg(is_const_reg) {}

template<typename reg_size>
reg_size Register<reg_size>::read() { return value; }

template<typename reg_size>
void Register<reg_size>::write(reg_size data) { value = (is_const_reg ? value : data); }