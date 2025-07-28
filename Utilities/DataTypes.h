#ifndef DATA_TYPES_H
#define DATA_TYPES_H

typedef unsigned char byte;
typedef signed char s_byte;

typedef unsigned short half_word;
typedef signed short s_half_word;

typedef unsigned int word;
typedef signed int s_word;

typedef unsigned long long double_word;
typedef signed long long s_double_word;

typedef __uint128_t quad_word;
typedef __int128_t s_quad_word;

typedef enum
{
    BIN = 2,
    OCT = 8,
    DEC = 10,
    HEX = 16
} base_t;

typedef enum
{
    // Base Integer Operations
    ADD,  // Add
    SUB,  // Subtract
    AND,  // Bitwise AND
    OR,   // Bitwise OR
    NOT,  // Bitwise NOT
    XOR,  // Bitwise XOR
    EQ,   // Equal to
    NEQ,  // Not Equal to
    LT,   // Less than
    LTU,  // Unsigned Less than
    GE,   // Greater than or equal to
    GEU,  // Unsigned Greater than or equal to 
    SLL,  // Logical Left Shift
    SRL,  // Logical Right Shift
    SRA,  // Arithmetic Right Shift
    SXT,   // Sign Extension

    // M Extension Operations
    MUL,     // Multiplication (Lower)
    MULH,    // Signed Multiplication (Higher)
    MULHU,   // Unsigned Multiplication (Higher)
    MULHSU,  // Signed x Unsigned Multiplication (Higher)
    DIV,     // Signed Division
    DIVU,    // Unsigned Division
    REM,     // Signed Remainder
    REMU     // Unsigned Remainder
} operation_t;

enum base_opcode_group
{
    ARITH_LOG_R   = 0b0110011u,  // Arithmetic/Logical Register
    ARITH_LOG_I   = 0b0010011u,  // Arithmetic/Logical Immediate
    LOAD          = 0b0000011u,  // Load
    STORE         = 0b0100011u,  // Store
    BRANCH        = 0b1100011u,  // Branch
    JAL           = 0b1101111u,  // Jump and Link
    JALR          = 0b1100111u,  // Jump and Link Register
    LUI           = 0b0110111u,  // Load Upper Immediate
    AUIPC         = 0b0010111u,  // Add Upper Immediate to PC
    ENVIRONMENT   = 0b1110011u,  // Environment Call/Break
    // RV64-Exclusive Opcodes
    ARITH_LOG_R_W = 0b0111011u,  // Arithmetic/Logical Register (Word)
    ARITH_LOG_I_W = 0b0011011u   // Arithmetic/Logical Immediate (Word)
};

typedef enum
{
    R,
    I,
    S,
    B,
    U,
    J
} instr_format_t;

typedef struct
{
    bool valid = false;
    byte opcode = 0;
    word imm = 0;
    byte rd = 0;
    byte rs1 = 0;
    byte rs2 = 0;
    byte funct3 = 0;
    byte funct7 = 0;
} dec_instr_t;  // represents a decoded instruction

template <typename word_size = word>
struct AddressRange  // represents the accessible range of addresses on memory map
{
    word_size start = 0;  // first accessible address in range
    word_size end = 0;    // last accessible address in range
};

#endif