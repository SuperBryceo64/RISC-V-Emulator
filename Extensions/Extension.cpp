#include "Extension.h"

template <typename word_size>
Extension<word_size>::Extension() : name(""), cpu_pc(NULL), cpu_ir(NULL), cpu_alu(NULL), cpu_register_set(NULL),
    cpu_constants(NULL), cpu_memory(NULL) {}

template <typename word_size>
Extension<word_size>::Extension(RISC_V_Components<word_size> &cpu_components) : name(""), cpu_pc(cpu_components.pc),
    cpu_ir(cpu_components.ir), cpu_alu(cpu_components.alu), cpu_register_set(cpu_components.register_set),
    cpu_constants(cpu_components.constants), cpu_memory(cpu_components.memory) {}

template <typename word_size>
Extension<word_size>::~Extension() {}

template <typename word_size>
std::string Extension<word_size>::getName() { return name; }