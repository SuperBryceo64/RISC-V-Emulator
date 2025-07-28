#include "Counter.h"

template<typename reg_size>
Counter<reg_size>::Counter() : Register<reg_size>(0, false), count_value(1) {}

template<typename reg_size>
Counter<reg_size>::Counter(reg_size count_value) : Register<reg_size>(0, false), count_value(count_value) {}

template<typename reg_size>
void Counter<reg_size>::increment() { value++; }

template<typename reg_size>
void Counter<reg_size>::count() { value += count_value; }