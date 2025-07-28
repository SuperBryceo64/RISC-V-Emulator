#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <string.h>
#include <vector>
#include <unordered_map>
#include "DataTypes.h"

// Assemble a specific file
template <typename word_size = word>
bool assemble(std::string asm_filename, endian_t endian = LITTLE, word_size data_rel_address = -1)
{
    // Open files
    std::string file_ext;
    file_ext = asm_filename.substr(asm_filename.rfind(".")+1);

    if (file_ext.compare("s") != 0 && file_ext.compare("S") != 0)  // check for RISC-V assembly file extension
    {
        printf("Error: Assembly file must have correct file extension\n");
        return false;
    }

    FILE *asm_ptr;
    asm_ptr = fopen(asm_filename.c_str(), "rb");
    
    if (asm_ptr == NULL) 
    {
        printf("Error opening %s", asm_filename.c_str());
        perror("");
        return false;
    }

    std::string bin_filename;
    FILE *bin_ptr;
    bin_filename = asm_filename.substr(0, asm_filename.rfind("."));
    bin_ptr = fopen(bin_filename.c_str(), "wb");

    if (bin_ptr == NULL) 
    {
        printf("Error opening %s executable", bin_filename.c_str());
        perror("");
        fclose(asm_ptr);
        return false;
    }

    std::string data_filename;
    FILE *data_ptr = NULL;
    if (data_rel_address != -1)
    {
        printf("Creating data file for %s...\ndata_rel_address = %d\n", asm_filename.c_str(), data_rel_address);
        data_filename = asm_filename.substr(0, asm_filename.rfind(".")) + "_data";
        data_ptr = fopen(data_filename.c_str(), "wb");

        if (data_ptr == NULL) 
        {
            printf("Error opening %s binary", data_filename.c_str());
            perror("");
            fclose(asm_ptr);
            fclose(bin_ptr);
            return false;
        }
    }

    std::vector<std::string> instruction;  // [0] = instruction name or label, [1] ... = operands 1 ...
    char curr_char, sel;  // sel selects which string stores the next char
    word_size operands[3];  // stores the numerical values of each operand
    std::vector<word> machine_code;  // low level machine code that will be stored in binary

    const std::unordered_map<std::string, byte> reg_names  // aliases for each register as defined by the RISC-V ABI
        = { {"zero", 0}, {"ra", 1},   {"sp", 2},  {"gp", 3},  {"tp", 4},  {"t0", 5},  {"t1", 6},  {"t2", 7},  {"s0", 8},
            {"fp", 8},   {"s1", 9},   {"a0", 10}, {"a1", 11}, {"a2", 12}, {"a3", 13}, {"a4", 14}, {"a5", 15}, {"a6", 16},
            {"a7", 17},  {"s2", 18},  {"s3", 19}, {"s4", 20}, {"s5", 21}, {"s6", 22}, {"s7", 23}, {"s8", 24}, {"s9", 25},
            {"s10", 26}, {"s11", 27}, {"t3", 28}, {"t4", 29}, {"t5", 30}, {"t6", 31} };
    std::unordered_map<std::string, word_size> sym_table;  // symbol table for keeping track of symbol and label addresses
    
    unsigned long long line_num = 0;
    bool open_parenthesis = false;
    const std::string scanlist = "%*[_0-9,a-z,A-Z]%c";
    bool assembling = true;  // flag to determine whether assembling code or just parsing through it
    std::string target_sym;  // keeps track of symbol that was referenced but wasn't yet instantiated (forwared refs)
    unsigned long long return_line_num = 0;  // keeps track of line number of instruction making forward reference
    long return_pos = 0;
    bool forward_ref_found = false;
    
    word_size curr_program_address = 0;  // keeps track of the relative address of the current instruction
    word_size curr_data_address = data_rel_address;  // keeps track of the relative address of the current data variable
    word_size program_return_address = 0;  // keeps track of relative address of instruction making forward reference 
    word_size data_return_address = 0;  // keeps track of relative address of data section at the time of forward reference

    while((curr_char = fgetc(asm_ptr)) != EOF)  // read every line
    {
        line_num++;  // increment to next line
        sel = 0;  // reset select
        fseek(asm_ptr, -1, SEEK_CUR);  // move back cursor so character can be re-read
        while((curr_char = fgetc(asm_ptr)) != '\n' && curr_char != EOF)  // parse current line
        {
            if (instruction.size() <= sel) { instruction.push_back(""); }

            switch (curr_char)
            {
                case '\'':  // read rest of the characters in between the single quotes
                {
                    bool escape_char = false;
                    byte num_chars = 0;
                    instruction[sel] += curr_char;
                    while(((curr_char = fgetc(asm_ptr)) != '\'' || escape_char) && curr_char != '\n' && curr_char != EOF && num_chars < 1)
                    {
                        if (!escape_char)
                        {
                            escape_char = curr_char == '\\'; 
                            num_chars = curr_char == '\\' ? num_chars : num_chars+1;
                            instruction[sel] += curr_char;
                        }
                        else
                        {
                            escape_char = false;
                            if (curr_char >= '0' && curr_char <= '7')  // octal or null character escape sequence
                            {
                                fseek(asm_ptr, -1, SEEK_CUR);

                                byte i = 0;
                                while (i < 3 && (curr_char = fgetc(asm_ptr)) >= '0' && curr_char <= '7') { instruction[sel] += curr_char; i++; }
                                if (i < 3)
                                {
                                    if (curr_char != '\'' && curr_char != '\n' && curr_char != EOF)
                                    {
                                        printf("Assembler error in %s, line %llu: Invalid escape sequence\n", 
                                            asm_filename.c_str(), line_num);
                                        fclose(asm_ptr);
                                        fclose(bin_ptr);
                                        if (data_ptr != NULL) { fclose(data_ptr); }
                                        return false;
                                    }

                                    if (curr_char == '\'' || curr_char == '\n') { fseek(asm_ptr, -1, SEEK_CUR); }
                                }
                            }
                            else if (curr_char == 'x')  // hex character escape sequence
                            {
                                instruction[sel] += curr_char;

                                byte i = 0;
                                while (i < 2 && (isdigit(curr_char = fgetc(asm_ptr)) || (tolower(curr_char) >= 'a' && tolower(curr_char) <= 'f')))
                                    { instruction[sel] += curr_char; i++; }
                                if (i < 2)
                                {
                                    if (curr_char != '\'' && curr_char != '\n' && curr_char != EOF)
                                    {
                                        printf("Assembler error in %s, line %llu: Invalid escape sequence\n", 
                                            asm_filename.c_str(), line_num);
                                        fclose(asm_ptr);
                                        fclose(bin_ptr);
                                        if (data_ptr != NULL) { fclose(data_ptr); }
                                        return false;
                                    }

                                    if (curr_char == '\'' || curr_char == '\n') { fseek(asm_ptr, -1, SEEK_CUR); }
                                }
                            }
                            else
                            {
                                switch (curr_char)
                                {
                                    case '\\':
                                    case '\'':
                                    case '"':
                                    case '?':
                                    case 'a':
                                    case 'b':
                                    case 'f':
                                    case 'n':
                                    case 'r':
                                    case 't':
                                    case 'v':
                                        instruction[sel] += curr_char;
                                        break;
                                    default:
                                        printf("Assembler error in %s, line %llu: Invalid escape sequence\n", 
                                            asm_filename.c_str(), line_num);
                                        fclose(asm_ptr);
                                        fclose(bin_ptr);
                                        if (data_ptr != NULL) { fclose(data_ptr); }
                                        return false;
                                }
                            }

                            num_chars++;
                        }
                    }
                    if (curr_char == '\n' || curr_char == EOF)
                    {
                        printf("Assembler error in %s, line %llu: Unclosed single quotes\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                    else if (curr_char != '\'')
                    {
                        printf("Assembler error in %s, line %llu: Multiple characters in single quotes\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                    else if (num_chars == 0)
                    {
                        printf("Assembler error in %s, line %llu: Char literal not specified\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }

                    instruction[sel] += curr_char;

                    if ((curr_char = fgetc(asm_ptr)) != ' ' && curr_char != ',' && curr_char != '\n' && curr_char != EOF)
                    {
                        printf("Assembler error in %s, line %llu: Illegal character after char literal\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }

                    if (curr_char != EOF) { fseek(asm_ptr, -1, SEEK_CUR); }

                    break;
                }

                case '\"':  // read rest of the characters in between the double quotes
                {
                    bool escape_char = false;
                    byte num_chars = 0;
                    instruction[sel] += curr_char;
                    while(((curr_char = fgetc(asm_ptr)) != '\"' || escape_char) && curr_char != '\n' && curr_char != EOF)
                    {
                        if (!escape_char)
                        {
                            escape_char = curr_char == '\\'; 
                            num_chars = curr_char == '\\' ? num_chars : num_chars+1;
                            instruction[sel] += curr_char;
                        }
                        else
                        {
                            escape_char = false;
                            if (curr_char >= '0' && curr_char <= '7')  // octal or null character escape sequence
                            {
                                fseek(asm_ptr, -1, SEEK_CUR);

                                byte i = 0;
                                while (i < 3 && (curr_char = fgetc(asm_ptr)) >= '0' && curr_char <= '7') { instruction[sel] += curr_char; i++; }
                                if (i < 3 && curr_char != EOF) { fseek(asm_ptr, -1, SEEK_CUR); }
                            }
                            else if (curr_char == 'x')  // hex character escape sequence
                            {
                                instruction[sel] += curr_char;

                                byte i = 0;
                                while (i < 2 && (isdigit(curr_char = fgetc(asm_ptr)) || (tolower(curr_char) >= 'a' && tolower(curr_char) <= 'f')))
                                    { instruction[sel] += curr_char; i++; }
                                if (i < 2 && curr_char != EOF) { fseek(asm_ptr, -1, SEEK_CUR); }
                            }
                            else
                            {
                                switch (curr_char)
                                {
                                    case '\\':
                                    case '\'':
                                    case '"':
                                    case '?':
                                    case 'a':
                                    case 'b':
                                    case 'f':
                                    case 'n':
                                    case 'r':
                                    case 't':
                                    case 'v':
                                        instruction[sel] += curr_char;
                                        break;
                                    default:
                                        printf("Assembler error in %s, line %llu: Invalid escape sequence\n", 
                                            asm_filename.c_str(), line_num);
                                        fclose(asm_ptr);
                                        fclose(bin_ptr);
                                        if (data_ptr != NULL) { fclose(data_ptr); }
                                        return false;
                                }
                            }

                            num_chars++;
                        }
                    }
                    if (curr_char == '\n' || curr_char == EOF)
                    {
                        printf("Assembler error in %s, line %llu: Unclosed double quotes\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }

                    instruction[sel] += curr_char;

                    if ((curr_char = fgetc(asm_ptr)) != ' ' && curr_char != ',' && curr_char != '\n' && curr_char != EOF)
                    {
                        printf("Assembler error in %s, line %llu: Illegal character after string literal\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }

                    if (curr_char != EOF) { fseek(asm_ptr, -1, SEEK_CUR); }
                    
                    break;
                }

                case ' ':  // start reading characters in next section of instruction
                    while((curr_char = fgetc(asm_ptr)) == ' ') {}  // skip the spaces
                    if(curr_char != EOF) { fseek(asm_ptr, -1, SEEK_CUR); }  // move back cursor so non-space can be re-read
                    if (instruction[sel].length() > 0) { sel++; }
                    break;
                
                case ',':  // skip ',' and start reading characters in the next section of instruction
                    if (sel >= 1)
                    {
                        while((curr_char = fgetc(asm_ptr)) == ' ') {}  // skip the spaces
                        if(curr_char != EOF) { fseek(asm_ptr, -1, SEEK_CUR); }  // move back cursor so non-space can be re-read
                        if(curr_char == '\n' || curr_char == EOF)  // if there is no operand after the comma
                        {
                            printf("Assembler error in %s, line %llu: No operand exists after ','\n", 
                                asm_filename.c_str(), line_num);
                            fclose(asm_ptr);
                            fclose(bin_ptr);
                            if (data_ptr != NULL) { fclose(data_ptr); }
                            return false;
                        }
                        sel++;
                    }
                    else
                    {
                        printf("Assembler error in %s, line %llu: Incorrect use of ','\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                    break;
                
                case '(':
                    if (!open_parenthesis && instruction[sel].length() > 0 && (sel == 2 || (instruction[0].back() == ':' && sel == 3)))
                    {
                        instruction[sel] += curr_char;
                        open_parenthesis = true;
                    }
                    else
                    {
                        printf("Assembler error in %s, line %llu: Incorrect use of '('\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                    break;
                
                case ')':
                    if(open_parenthesis && (sel == 2 || (instruction[0].back() == ':' && sel == 3)))
                    {  
                        instruction[sel] += curr_char;
                        open_parenthesis = false;
                    }
                    else
                    {
                        printf("Assembler error in %s, line %llu: Incorrect use of ')'\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                    break;

                case '#':  // rest of the line is a comment, so it is to be ignored
                    while((curr_char = fgetc(asm_ptr)) != '\n' && curr_char != EOF) {}  // skip until end of the line or file
                    if(curr_char != EOF) { fseek(asm_ptr, -1, SEEK_CUR); }  // move back cursor so newline can be re-read
                    break;
                
                case ':':  // previous characters form a symbol
                    if(sel == 0)
                    {
                        if(instruction[sel].length() == 0)
                        {
                            printf("Assembler error in %s, line %llu: Symbol not specified\n", 
                                asm_filename.c_str(), line_num);
                            fclose(asm_ptr);
                            fclose(bin_ptr);
                            if (data_ptr != NULL) { fclose(data_ptr); }
                            return false;
                        }
                        if((curr_char = fgetc(asm_ptr)) != ' ' && curr_char != '\n')  // next character must be a space or newline
                        {
                            if(curr_char == EOF)
                            {
                                printf("Assembler error in %s, line %llu: File ends before symbol is instantiated\n", 
                                    asm_filename.c_str(), line_num);
                                fclose(asm_ptr);
                                fclose(bin_ptr);
                                if (data_ptr != NULL) { fclose(data_ptr); }
                                return false;
                            }
                            else
                            {
                                printf("Assembler error in %s, line %llu: Symbol is not spaced out\n", 
                                    asm_filename.c_str(), line_num);
                                fclose(asm_ptr);
                                fclose(bin_ptr);
                                if (data_ptr != NULL) { fclose(data_ptr); }
                                return false;
                            }
                        }
                        fseek(asm_ptr, -1, SEEK_CUR);
                        instruction[sel] += ':';  // ':' is included in section
                        while((curr_char = fgetc(asm_ptr)) == ' ') {}  // skip the spaces
                        if(curr_char != EOF) { fseek(asm_ptr, -1, SEEK_CUR); }  // move back cursor so non-space can be re-read
                        sel++;
                    }
                    else
                    {
                        printf("Assembler error in %s, line %llu: ':' is used in non-symbol\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                    break;

                default:  // everything else forms the current section of the instruction
                    instruction[sel] += curr_char;
                    break;
            }
        }

        while (instruction.size() > 0 && instruction.front().length() <= 0) { instruction.erase(instruction.begin()); }  // remove any empty strings in the front
        while (instruction.size() > 0 && instruction.back().length() <= 0) { instruction.pop_back(); }  // remove any empty strings in the back

        int temp_num; char temp_char;

        if (instruction.size() > 4 && instruction[0].front() != '.' && instruction[1].front() != '.' && instruction[4].length() > 0)  // if a normal instruction has more than 3 operands
        {
            if(instruction[0].back() != ':' || (instruction.size() > 5 && instruction[5].length() > 0))  // if a label has more than three operands
            {
                printf("Assembler error in %s, line %llu: Too many operands\n", 
                    asm_filename.c_str(), line_num);
                fclose(asm_ptr);
                fclose(bin_ptr);
                if (data_ptr != NULL) { fclose(data_ptr); }
                return false;
            }
        }

        temp_num = instruction[0].back() != ':' ? 2 : 3;
        if (instruction.size() > temp_num+1 && instruction[temp_num-2].front() != '.' && instruction[temp_num].find('(') != std::string::npos && instruction[temp_num+1].length() > 0)  // if the load or store instruction has more than 2 operands
        {
            printf("Assembler error in %s, line %llu: Too many operands\n", 
                asm_filename.c_str(), line_num);
            fclose(asm_ptr);
            fclose(bin_ptr);
            if (data_ptr != NULL) { fclose(data_ptr); }
            return false;
        }

        if (open_parenthesis)  // if a parenthesis was not properly closed
        {
            printf("Assembler error in %s, line %llu: Unclosed parenthesis\n", 
                asm_filename.c_str(), line_num);
            fclose(asm_ptr);
            fclose(bin_ptr);
            if (data_ptr != NULL) { fclose(data_ptr); }
            return false;
        }

        temp_num = instruction[0].back() != ':' ? 2 : 3;
        if (sel > temp_num && instruction[0].back() != ':' && instruction[temp_num].find('(') != std::string::npos && instruction[temp_num].find('(') != instruction[temp_num].rfind('('))  // if there are more than one sets of parenthesis
        {
            printf("Assembler error in %s, line %llu: Multiple sets of parenthesis\n", 
                asm_filename.c_str(), line_num);
            fclose(asm_ptr);
            fclose(bin_ptr);
            if (data_ptr != NULL) { fclose(data_ptr); }
            return false;
        }

        if (sel > temp_num && instruction[0].back() != ':' && instruction[temp_num].find('(') != std::string::npos && instruction[temp_num].find("()") != std::string::npos)  // if the parenthesis does not include a register
        {
            printf("Assembler error in %s, line %llu: Register is not included in indexing\n", 
                asm_filename.c_str(), line_num);
            fclose(asm_ptr);
            fclose(bin_ptr);
            if (data_ptr != NULL) { fclose(data_ptr); }
            return false;
        }

        if (instruction[0].back() == ':')  
        {
            if (sym_table.count(instruction[0].substr(0, instruction[0].length()-1)) > 0)  // if a symbol is being instantiated but already exists
            {
                if (curr_program_address != sym_table.at(instruction[0].substr(0, instruction[0].length()-1)) &&
                    curr_data_address    != sym_table.at(instruction[0].substr(0, instruction[0].length()-1)))
                {
                    printf("Assembler error in %s, line %llu: Symbol already exists\n", asm_filename.c_str(), line_num);
                    fclose(asm_ptr);
                    fclose(bin_ptr);
                    if (data_ptr != NULL) { fclose(data_ptr); }
                    return false;
                }
            }

            temp_num = 0; temp_char = 0;
            if (reg_names.count(instruction[0].substr(0, instruction[0].length()-1)) > 0 ||  // if a symbol is using a register alias
                (sscanf(instruction[0].c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == ':'))
            {
                printf("Assembler error in %s, line %llu: Symbol is using reserved alias\n", asm_filename.c_str(), line_num);
                fclose(asm_ptr);
                fclose(bin_ptr);
                if (data_ptr != NULL) { fclose(data_ptr); }
                return false;
            }

            if (sscanf(instruction[0].c_str(), "%d%c", &temp_num, &temp_char) == 2)
            {
                if (temp_char == ':')
                {
                    if (temp_num <= 0 || temp_num >= 100)  // if local label is not between 1 and 99 inclusive
                    {
                        printf("Assembler error in %s, line %llu: Local label must be between 1 and 99\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                }
                else  // if a global label starts with a number
                {
                    printf("Assembler error in %s, line %llu: Global label cannot start with number\n", 
                        asm_filename.c_str(), line_num);
                    fclose(asm_ptr);
                    fclose(bin_ptr);
                    if (data_ptr != NULL) { fclose(data_ptr); }
                    return false;
                }
            }

            sscanf(instruction[0].c_str(), scanlist.c_str(), &temp_char);
            if (temp_char != ':')  // if label name has an unallowed character
            {
                printf("Assembler error in %s, line %llu: '%c' character not allowed in global label name\n", 
                    asm_filename.c_str(), line_num, (temp_char) != 0 ? temp_char : instruction[0].front());
                fclose(asm_ptr);
                fclose(bin_ptr);
                if (data_ptr != NULL) { fclose(data_ptr); }
                return false;
            }
        }

        for(auto section : instruction) { printf("%s, %lu\n", section.c_str(), section.length()); }

        // Encode assembly instruction into machine code and store into executable
        if (instruction.size() > 0 && instruction[0].length() > 0)
        {
            for(word_size &operand : operands) { operand = 0; }


            if (instruction[0].back() == ':')  // if instruction is a symbol or label instantiation
            {
                // since we do not yet know whether the next symbol is for an instruction or variable, 
                // we set the symbol's value to -1 which will be properly assigned when the next 
                // instruction or variable is assembled
                std::string symbol = instruction[0].substr(0, instruction[0].length()-1);
                int temp_num = 0; char temp_char = 0;
                if (sscanf(instruction[0].c_str(), "%d%c", &temp_num, &temp_char) == 2)
                {
                    if (temp_num >= 1 && temp_num <= 99)  // if instantiating a local label
                    {
                        sym_table[symbol + "b"] = (word_size) (-1);  // place backwards ref in sym table
                        if (sym_table.count(symbol + "f") > 0 && !forward_ref_found) { sym_table.erase(symbol + "f"); }  // erase previous forward ref from sym table
                    }
                }
                else  // otherwise, instantiating a global label
                {
                    if (sym_table.count(symbol) <= 0)
                    { 
                        sym_table[symbol] = (word_size) (-1);
                    }
                }

                instruction.erase(instruction.begin());  // remove label from rest of instruction
            }

            if (instruction[0].front() == '.')  // if instruction is a directive
            {
                const std::unordered_map<std::string, byte> data_directives =   // {directive, num_bytes}
                    {{".byte", 1}, {".half", 2}, {".word", 4}, {".dword", 8},
                     {".ascii", 1}, {".asciz", 1}, {".string", 1}};

                if (data_directives.count(instruction[0]) > 0)  // if directive is a data directive
                {
                    // if assigning a new data variable, set any unassigned symbols to the current data address
                    // also check for unassigned symbols that match the target forward ref
                    int temp_num = 0; char temp_char = 0;
                    forward_ref_found = false;
                    std::vector<byte> data;
                    byte size = data_directives.at(instruction[0]);

                    // assign any unassigned labels to the current data address
                    for (auto& label : sym_table)
                    {
                        if (label.second == (word_size) (-1)) 
                        { 
                            label.second = curr_data_address;
                            if (!assembling)  // check if label is the forward ref we are looking for
                            {
                                if (sscanf(label.first.c_str(), "%d%c", &temp_num, &temp_char) == 2)  // if unassigned label is local
                                {
                                    if (temp_num == atoi(target_sym.substr(0, target_sym.find('f')).c_str()))
                                    {
                                        // add local forward reference to sym table
                                        sym_table[target_sym] = curr_data_address;
                                        // return to instruction that called forward reference
                                        fseek(asm_ptr, return_pos, SEEK_SET);
                                        curr_program_address = program_return_address;
                                        curr_data_address = data_return_address;
                                        line_num = return_line_num - 2;
                                        forward_ref_found = true;

                                        assembling = true;
                                        target_sym = "";
                                        return_line_num = 0;
                                        return_pos = 0;
                                        program_return_address = 0;
                                        data_return_address = 0;

                                        printf("*Forward ref found*\n");
                                    }
                                }
                                else  // otherwise, unassigned label is global
                                {
                                    if(label.first == target_sym)
                                    {
                                        // return to instruction that called forward reference
                                        fseek(asm_ptr, return_pos, SEEK_SET);
                                        curr_program_address = program_return_address;
                                        curr_data_address = data_return_address;
                                        line_num = return_line_num - 2;
                                        forward_ref_found = true;

                                        assembling = true;
                                        target_sym = "";
                                        return_line_num = 0;
                                        return_pos = 0;
                                        program_return_address = 0;
                                        data_return_address = 0;

                                        printf("*Forward ref found*\n");
                                    }
                                }
                            }
                        }
                    }

                    if (instruction[0].compare(".byte") ==  0  ||
                        instruction[0].compare(".half") ==  0  ||
                        instruction[0].compare(".word") ==  0  ||
                        instruction[0].compare(".dword") == 0 )
                    {
                        instruction.erase(instruction.begin());

                        if (instruction.size() < 1)
                        {
                            printf("Assembler error in %s, line %llu: No values being passed to .%s\n", 
                                asm_filename.c_str(), line_num, size == 1 ? "byte" : (size == 2 ? "half" : 
                                (size == 4 ? "word" : (size == 8 ? "dword" : "value"))));
                            fclose(asm_ptr);
                            fclose(bin_ptr);
                            if (data_ptr != NULL) { fclose(data_ptr); }
                            return false;
                        }

                        long long curr_num;
                        char* end_ptr;

                        printf("Values: ");
                        while (instruction.size() > 0)
                        {
                            curr_num = strtoll(instruction[0].c_str(), &end_ptr, 0);
                            
                            if (*end_ptr == '\'')  // char literal
                            {
                                if (instruction[0].at(1) != '\\') { curr_num = (long long) instruction[0].at(1); }
                                else
                                {
                                    if (instruction[0].at(2) >= '0' && instruction[0].at(2) <= '7')  // octal or null escape sequence
                                    {
                                        sscanf(instruction[0].c_str(), "'\\%llo'", &curr_num);
                                    }
                                    else if (instruction[0].at(2) == 'x')  // hex escape sequence
                                    {
                                        sscanf(instruction[0].c_str(), "'\\x%llx'", &curr_num);
                                    }
                                    else
                                    {
                                        switch(instruction[0].at(2))
                                        {
                                            case '\\':
                                                curr_num = (long long) '\\';
                                                break;
                                            case '\'':
                                                curr_num = (long long) '\'';
                                                break;
                                            case '"':
                                                curr_num = (long long) '\"';
                                                break;
                                            case '?':
                                                curr_num = (long long) '\?';
                                                break;
                                            case 'a':
                                                curr_num = (long long) '\a';
                                                break;
                                            case 'b':
                                                curr_num = (long long) '\b';
                                                break;
                                            case 'f':
                                                curr_num = (long long) '\f';
                                                break;
                                            case 'n':
                                                curr_num = (long long) '\n';
                                                break;
                                            case 'r':
                                                curr_num = (long long) '\r';
                                                break;
                                            case 't':
                                                curr_num = (long long) '\t';
                                                break;
                                            case 'v':
                                                curr_num = (long long) '\v';
                                                break;
                                        }
                                    }
                                }
                            }
                            else if (*end_ptr == '\"')  // string literal
                            {
                                printf("Assembler error in %s, line %llu: Cannot pass string literal as %s\n", 
                                    asm_filename.c_str(), line_num, size == 1 ? "byte" : (size == 2 ? "half" : 
                                    (size == 4 ? "word" : (size == 8 ? "dword" : "value"))));
                                fclose(asm_ptr);
                                fclose(bin_ptr);
                                if (data_ptr != NULL) { fclose(data_ptr); }
                                return false;
                            }
                            else if (*end_ptr != 0)
                            {
                                printf("Assembler error in %s, line %llu: Invalid data value\n", 
                                    asm_filename.c_str(), line_num);
                                fclose(asm_ptr);
                                fclose(bin_ptr);
                                if (data_ptr != NULL) { fclose(data_ptr); }
                                return false;
                            }
                            
                            if (endian == LITTLE)
                            {
                                for(int i = 0; i < size; i++)
                                {
                                    data.push_back((byte)(curr_num >> (8*i)));
                                }
                            }
                            else
                            {
                                for(int i = size-1; i >= 0; i--)
                                {
                                    data.push_back((byte)(curr_num >> (8*i)));
                                }
                            }

                            printf("%lld ", curr_num);
                            instruction.erase(instruction.begin());
                        }
                    }
                    else if (instruction[0].compare(".ascii")  == 0  ||
                             instruction[0].compare(".asciz")  == 0  ||
                             instruction[0].compare(".string") == 0)
                    {
                        bool null_terminated = instruction[0].compare(".asciz") == 0 || instruction[0].compare(".string") == 0;
                        instruction.erase(instruction.begin());

                        if (instruction.size() > 1)
                        {
                            printf("Assembler error in %s, line %llu: Passing in too many values to .string\n", 
                                asm_filename.c_str(), line_num);
                            fclose(asm_ptr);
                            fclose(bin_ptr);
                            if (data_ptr != NULL) { fclose(data_ptr); }
                            return false;
                        }
                        else if (instruction.size() < 1)
                        {
                            printf("Assembler error in %s, line %llu: No values being passed to .string\n", 
                                asm_filename.c_str(), line_num);
                            fclose(asm_ptr);
                            fclose(bin_ptr);
                            if (data_ptr != NULL) { fclose(data_ptr); }
                            return false;
                        }

                        instruction[0] = instruction[0].substr(1, instruction[0].length() - 2); // remove enclosing double quotes
                        char curr_char;
                        word char_len;
                        
                        printf("Values: ");
                        while (instruction[0].length() > 0)
                        {
                            char_len = 0;

                            if (instruction[0].front() != '\\')
                            {
                                curr_char = instruction[0].front();
                                char_len = 1;
                            }
                            else
                            {
                                if (instruction[0].at(1) >= '0' && instruction[0].at(1) <= '7')  // octal or null escape sequence
                                {
                                    char temp[4];
                                    sscanf(instruction[0].c_str(), "\\%3[0-7]", temp);
                                    curr_char = strtol(temp, NULL, 8);
                                    char_len = strlen(temp) + 1;
                                }
                                else if (instruction[0].at(1) == 'x')  // hex escape sequence
                                {
                                    char temp[3];
                                    sscanf(instruction[0].c_str(), "\\x%2[0-9,a-f,A-F]", temp);
                                    curr_char = strtol(temp, NULL, 16);
                                    char_len = strlen(temp) + 2;
                                }
                                else
                                {
                                    switch(instruction[0].at(1))
                                    {
                                        case '\\':
                                            curr_char = '\\';
                                            break;
                                        case '\'':
                                            curr_char = '\'';
                                            break;
                                        case '"':
                                            curr_char = '\"';
                                            break;
                                        case '?':
                                            curr_char = '\?';
                                            break;
                                        case 'a':
                                            curr_char = '\a';
                                            break;
                                        case 'b':
                                            curr_char = '\b';
                                            break;
                                        case 'f':
                                            curr_char = '\f';
                                            break;
                                        case 'n':
                                            curr_char = '\n';
                                            break;
                                        case 'r':
                                            curr_char = '\r';
                                            break;
                                        case 't':
                                            curr_char = '\t';
                                            break;
                                        case 'v':
                                            curr_char = '\v';
                                            break;
                                    }
                                    char_len = 2;
                                }
                            }
                        
                            data.push_back((byte)curr_char);
                            instruction[0] = instruction[0].substr(char_len);
                        }

                        // add null terminator if directive is .string or .asciz
                        if (null_terminated) { data.push_back('\0'); }

                        for(auto value : data) { printf("%d ", value); }
                    }

                    if (assembling && !forward_ref_found)
                    {
                        fwrite(data.data(), size, data.size() / size, data_ptr);  // write data into data section
                    }
                    if (!forward_ref_found) { curr_data_address += data.size(); }
                }

                printf("\n\n");
            }
            else if (instruction.size() > 0)  // the rest of the line is a normal instruction
            {
                int temp_num = 0; char temp_char = 0;
                forward_ref_found = false;

                // assign any unassigned labels to the current program address
                for (auto& label : sym_table)
                {
                    if (label.second == (word_size) (-1)) 
                    { 
                        label.second = curr_program_address;
                        if (!assembling)  // check if label is the forward ref we are looking for
                        {
                            if (sscanf(label.first.c_str(), "%d%c", &temp_num, &temp_char) == 2)  // if unassigned label is local
                            {
                                if (temp_num == atoi(target_sym.substr(0, target_sym.find('f')).c_str()))
                                {
                                    // add local forward reference to sym table
                                    sym_table[target_sym] = curr_program_address;
                                    // return to instruction that called forward reference
                                    fseek(asm_ptr, return_pos, SEEK_SET);
                                    curr_program_address = program_return_address;
                                    curr_data_address = data_return_address;
                                    line_num = return_line_num - 2;
                                    forward_ref_found = true;

                                    assembling = true;
                                    target_sym = "";
                                    return_line_num = 0;
                                    return_pos = 0;
                                    program_return_address = 0;
                                    data_return_address = 0;

                                    printf("*Forward ref found*\n");
                                }
                            }
                            else  // otherwise, unassigned label is global
                            {
                                if(label.first == target_sym)
                                {
                                    // return to instruction that called forward reference
                                    fseek(asm_ptr, return_pos, SEEK_SET);
                                    curr_program_address = program_return_address;
                                    curr_data_address = data_return_address;
                                    line_num = return_line_num - 2;
                                    forward_ref_found = true;

                                    assembling = true;
                                    target_sym = "";
                                    return_line_num = 0;
                                    return_pos = 0;
                                    program_return_address = 0;
                                    data_return_address = 0;

                                    printf("*Forward ref found*\n");
                                }
                            }
                        }
                    }
                }

                // convert operands into their numerical values
                char *end_ptr;
                for(int i = 0; i < instruction.size()-1; i++)
                {
                    if (instruction[i+1].front() == '%')  // if operand is a relative value
                    {

                    }
                    else if (instruction[i+1].front() == '"')  // if operand is a string literal
                    {
                        printf("Assembler error in %s, line %llu: Cannot pass string literal as instruction operand\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                    else if (instruction[i+1].front() == '\'')  // if operand is a char literal
                    {
                        if (instruction[i+1].at(1) != '\\') { operands[i] = (word_size) instruction[i+1].at(1); }
                        else
                        {
                            if (instruction[i+1].at(2) >= '0' && instruction[i+1].at(2) <= '9')  // octal or null escape sequence
                            {
                                sscanf(instruction[i+1].c_str(), "'\\%o'", &operands[i]);
                            }
                            else if (instruction[i+1].at(2) == 'x')  // hex escape sequence
                            {
                                sscanf(instruction[i+1].c_str(), "'\\x%x'", &operands[i]);
                            }
                            else
                            {
                                switch(instruction[i+1].at(2))
                                {
                                    case '\\':
                                        operands[i] = (word_size) '\\';
                                        break;
                                    case '\'':
                                        operands[i] = (word_size) '\'';
                                        break;
                                    case '"':
                                        operands[i] = (word_size) '\"';
                                        break;
                                    case '?':
                                        operands[i] = (word_size) '\?';
                                        break;
                                    case 'a':
                                        operands[i] = (word_size) '\a';
                                        break;
                                    case 'b':
                                        operands[i] = (word_size) '\b';
                                        break;
                                    case 'f':
                                        operands[i] = (word_size) '\f';
                                        break;
                                    case 'n':
                                        operands[i] = (word_size) '\n';
                                        break;
                                    case 'r':
                                        operands[i] = (word_size) '\r';
                                        break;
                                    case 't':
                                        operands[i] = (word_size) '\t';
                                        break;
                                    case 'v':
                                        operands[i] = (word_size) '\v';
                                        break;
                                }
                            }
                        }
                    }
                    else if (reg_names.count(instruction[i+1]) > 0)  // if operand is a register alias
                    {
                        operands[i] = reg_names.at(instruction[i+1]);
                    }
                    else if (sym_table.count(instruction[i+1]) > 0)  // if operand is in symbol table
                    {
                        operands[i] = sym_table.at(instruction[i+1]) - curr_program_address;
                    }
                    else if (instruction[i+1].find('(') != std::string::npos)  // if operand uses an offset of a register
                    {
                        std::size_t open_par_pos = instruction[i+1].find('('), closed_par_pos = instruction[i+1].find(')');
                        // get immediate offset
                        operands[i] = strtoull(instruction[i+1].substr(0, open_par_pos).c_str(), &end_ptr, 0);

                        if (*end_ptr != 0)
                        {
                            printf("Assembler error in %s, line %llu: Invalid offset value\n", 
                                asm_filename.c_str(), line_num);
                            fclose(asm_ptr);
                            fclose(bin_ptr);
                            if (data_ptr != NULL) { fclose(data_ptr); }
                            return false;
                        }

                        // get indexed register
                        if (reg_names.count(instruction[i+1].substr(open_par_pos+1, closed_par_pos - (open_par_pos+1))) > 0)
                        {
                            operands[i+1] = reg_names.at(instruction[i+1].substr(open_par_pos+1, closed_par_pos - (open_par_pos+1)));
                        }
                        else
                        {
                            operands[i+1] = strtoull(instruction[i+1].substr(open_par_pos+2, closed_par_pos - (open_par_pos+2)).c_str(), &end_ptr, 10);

                            if (*end_ptr != 0 || instruction[i+1].find("(x") == std::string::npos)
                            {
                                printf("Assembler error in %s, line %llu: Invalid register value\n", 
                                    asm_filename.c_str(), line_num);
                                fclose(asm_ptr);
                                fclose(bin_ptr);
                                if (data_ptr != NULL) { fclose(data_ptr); }
                                return false;
                            }
                        }
                    }
                    else if (sscanf((instruction[i+1]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1')  // if operand is a register
                    {
                        operands[i] = strtoull(instruction[i+1].substr(1).c_str(), &end_ptr, 10);
                        if (*end_ptr != 0 || temp_num < 0 || temp_num > 31)
                        {
                            printf("Assembler error in %s, line %llu: Invalid register value\n", 
                                asm_filename.c_str(), line_num);
                            fclose(asm_ptr);
                            fclose(bin_ptr);
                            if (data_ptr != NULL) { fclose(data_ptr); }
                            return false;
                        }
                    }
                    else  // otherwise, operand is an immediate value or possible forward ref
                    {
                        // base is determined from prefix (0b = binary, 0 = octal, 0x = hex, no prefix = decimal)
                        operands[i] = strtoull(instruction[i+1].c_str(), &end_ptr, 0);

                        if (*end_ptr != 0 && assembling && !forward_ref_found)
                        {
                            if (curr_char == EOF)  // can't search for forward reference if at end of file
                            {
                                printf("Assembler error in %s, line %llu: Symbol \"%s\" doesn't exist\n",
                                    asm_filename.c_str(), line_num, instruction[i+1].c_str());
                                fclose(asm_ptr);
                                fclose(bin_ptr);
                                if (data_ptr != NULL) { fclose(data_ptr); }
                                return false;
                            }

                            printf("*Searching for forward ref*\n");

                            if (sscanf(instruction[i+1].c_str(), "%d%c", &temp_num, &temp_char) == 2)
                            {
                                if (temp_num >= 1 && temp_num <= 99 && temp_char == 'f' && instruction[i+1].back() == 'f')  // if forward ref is a local label
                                {
                                    target_sym = instruction[i+1];
                                    return_line_num = line_num;
                                    program_return_address = curr_program_address;
                                    data_return_address = curr_data_address;
                                    assembling = false;
                                    return_pos = ftell(asm_ptr) - 1;
                                    do { fseek(asm_ptr, --return_pos, SEEK_SET); }  // set return_pos to start of line
                                        while (return_pos > 0 && (temp_char = fgetc(asm_ptr)) != '\n' && ungetc(temp_char, asm_ptr));
                                    while (fgetc(asm_ptr) != '\n') {}  // reset file pointer back to end of line

                                    // if instruction is likely a branch, call, jump, or tail instruction,
                                    if (tolower(instruction[0].front()) == 'b' || 
                                        tolower(instruction[0].front()) == 'c' || 
                                        tolower(instruction[0].front()) == 't' ||
                                        tolower(instruction[0].front()) == 'j')
                                    {
                                        operands[i] = 4;  // assume the forward reference is a label at the next address
                                    }
                                    else  // otherwise, instruction is likely a load or store instruction
                                    {
                                        operands[i] = curr_data_address - curr_program_address;  // assume the forward reference is the data section
                                    }
                                }
                                else if (temp_num >= 1 && temp_num <= 99 && temp_char == 'b' && instruction[i+1].back() == 'b')  // if using a non-existant backwards reference
                                {
                                    printf("Assembler error in %s, line %llu: Symbol \"%s\" doesn't exist\n", 
                                        asm_filename.c_str(), line_num, instruction[i+1].c_str());
                                    fclose(asm_ptr);
                                    fclose(bin_ptr);
                                    if (data_ptr != NULL) { fclose(data_ptr); }
                                    return false;
                                }
                                else
                                {
                                    printf("Assembler error in %s, line %llu: Invalid immediate value\n", 
                                        asm_filename.c_str(), line_num);
                                    fclose(asm_ptr);
                                    fclose(bin_ptr);
                                    if (data_ptr != NULL) { fclose(data_ptr); }
                                    return false;
                                }
                            }
                            else
                            {
                                sscanf((instruction[i+1] + "\1").c_str(), scanlist.c_str(), &temp_char);
                                if (temp_char == '\1')  // if forward ref is a global label
                                {
                                    target_sym = instruction[i+1];
                                    return_line_num = line_num;
                                    program_return_address = curr_program_address;
                                    data_return_address = curr_data_address;
                                    assembling = false;
                                    return_pos = ftell(asm_ptr) - 1;
                                    do { fseek(asm_ptr, --return_pos, SEEK_SET); }  // set return_pos to start of line
                                        while (return_pos > 0 && (temp_char = fgetc(asm_ptr)) != '\n' && ungetc(temp_char, asm_ptr));
                                    while (fgetc(asm_ptr) != '\n') {}  // reset file pointer back to end of line

                                    // if instruction is likely a branch, call, jump, or tail instruction,
                                    if (tolower(instruction[0].front()) == 'b' || 
                                        tolower(instruction[0].front()) == 'c' || 
                                        tolower(instruction[0].front()) == 't' ||
                                        tolower(instruction[0].front()) == 'j')
                                    {
                                        operands[i] = 4;  // assume the forward reference is a label at the next address
                                    }
                                    else  // otherwise, instruction is likely a load or store instruction
                                    {
                                        operands[i] = curr_data_address - curr_program_address;  // assume the forward reference is the data section
                                    }
                                }
                                else
                                {
                                    printf("Assembler error in %s, line %llu: Invalid immediate value\n", 
                                        asm_filename.c_str(), line_num);
                                    fclose(asm_ptr);
                                    fclose(bin_ptr);
                                    if (data_ptr != NULL) { fclose(data_ptr); }
                                    return false;
                                }
                            }
                        }
                        else if (*end_ptr != 0 && !assembling && !forward_ref_found)  // already searching for a forward ref
                        {
                           // if instruction is likely a branch, call, jump, or tail instruction,
                            if (tolower(instruction[0].front()) == 'b' || 
                                tolower(instruction[0].front()) == 'c' || 
                                tolower(instruction[0].front()) == 't' ||
                                tolower(instruction[0].front()) == 'j')
                            {
                                operands[i] = 4;  // assume the forward reference is a label at the next address
                            }
                            else  // otherwise, instruction is likely a load or store instruction
                            {
                                operands[i] = curr_data_address - curr_program_address;  // assume the forward reference is the data section
                            }
                        }
                    }
                }

                printf("Operands: ");
                for(auto operand : operands) { printf("%d ", operand); }
                printf("\n\n");

                // set all characters lowercase
                for(int i = 0; i < instruction[0].length(); i++) { instruction[0][i] = tolower(instruction[0][i]); }

                if      (instruction[0].compare("add") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0, 0}, R)}; }
                else if (instruction[0].compare("addi") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[2], (byte)operands[0], (byte)operands[1], 0, 0, 0}, I)}; }
                else if (instruction[0].compare("addiw") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I_W, (word)operands[2], (byte)operands[0], (byte)operands[1], 0, 0, 0}, I)}; }
                else if (instruction[0].compare("addw") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0, 0}, R)}; }
                else if (instruction[0].compare("and") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x7, 0}, R)}; }
                else if (instruction[0].compare("andi") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[2], (byte)operands[0], (byte)operands[1], 0, 0x7, 0}, I)}; }
                else if (instruction[0].compare("auipc") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(operands[1] << 12), (byte)operands[0], 0, 0, 0, 0}, U)}; }
                else if (instruction[0].compare("beq") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[0], (byte)operands[1], 0, 0}, B)}; }
                else if (instruction[0].compare("beqz") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[1], 0, (byte)operands[0], (byte)reg_names.at("zero"), 0, 0}, B)}; }  // beq rs1, x0, imm
                else if (instruction[0].compare("bge") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[0], (byte)operands[1], 0x5, 0}, B)}; }
                else if (instruction[0].compare("bgez") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[1], 0, (byte)operands[0], (byte)reg_names.at("zero"), 0x5, 0}, B)}; }  // bge rs1, x0, imm
                else if (instruction[0].compare("bgeu") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[0], (byte)operands[1], 0x7, 0}, B)}; }
                else if (instruction[0].compare("bgt") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[1], (byte)operands[0], 0x4, 0}, B)}; }  // blt rs2, rs1, imm
                else if (instruction[0].compare("bgtu") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[1], (byte)operands[0], 0x6, 0}, B)}; }  // bltu rs2, rs1, imm
                else if (instruction[0].compare("bgtz") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[1], 0, (byte)reg_names.at("zero"), (byte)operands[0], 0x4, 0}, B)}; }  // blt x0, rs1, imm
                else if (instruction[0].compare("ble") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[1], (byte)operands[0], 0x5, 0}, B)}; }  // bge rs2, rs1, imm
                else if (instruction[0].compare("bleu") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[1], (byte)operands[0], 0x7, 0}, B)}; }  // bgeu rs2, rs1, imm
                else if (instruction[0].compare("blez") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[1], 0, (byte)reg_names.at("zero"), (byte)operands[0], 0x5, 0}, B)}; }  // bge x0, rs1, imm
                else if (instruction[0].compare("blt") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[0], (byte)operands[1], 0x4, 0}, B)}; }
                else if (instruction[0].compare("bltu") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[0], (byte)operands[1], 0x6, 0}, B)}; }
                else if (instruction[0].compare("bltz") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[1], 0, (byte)operands[0], (byte)reg_names.at("zero"), 0x4, 0}, B)}; }  // blt rs1, x0, imm
                else if (instruction[0].compare("bne") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[2], 0, (byte)operands[0], (byte)operands[1], 0x1, 0}, B)}; }
                else if (instruction[0].compare("bnez") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, BRANCH, (word)operands[1], 0, (byte)operands[0], (byte)reg_names.at("zero"), 0x1, 0}, B)}; }  // bnez rs1, x0, imm
                else if (instruction[0].compare("call") == 0)
                {
                    word_size imm = instruction.size() > 2 ? operands[1] : operands[0];
                    byte rd = instruction.size() > 2 ? operands[0] : reg_names.at("ra");  // jal will link to x1 if rd is not specified
                    word_size upper_imm = (word)(imm & 0xFFFFF000) - ((((imm >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                    machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_imm), (byte)rd, 0, 0, 0, 0}, U),           // auipc rd, imm[31:12]
                                    getEncodedInstructionFromFormat({true, JALR, (word)(imm & 0xFFE), (byte)rd, (byte)rd, 0, 0, 0}, I)};  // jalr rd, rd, imm[11:0]
                }
                else if (instruction[0].compare("div") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x4, 0x1}, R)}; }
                else if (instruction[0].compare("divu") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x5, 0x1}, R)}; }
                else if (instruction[0].compare("divuw") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x5, 0x1}, R)}; }
                else if (instruction[0].compare("divw") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x4, 0x1}, R)}; }
                else if (instruction[0].compare("ebreak") == 0)  { machine_code = {getEncodedInstructionFromFormat({true, ENVIRONMENT, 0x1, 0, 0, 0, 0, 0}, I)}; }
                else if (instruction[0].compare("ecall") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, ENVIRONMENT, 0, 0, 0, 0, 0, 0}, I)}; }
                else if (instruction[0].compare("j") == 0)
                {
                    if (((operands[0] & (word_size)(-1048576)) == (word_size)(-1048576)) || ((operands[0] & (word_size)(-1048576)) == 0))   // if the signed immediate can fit in 21 bits
                        { machine_code = {getEncodedInstructionFromFormat({true, JAL, (word)(operands[0] & (word_size)(-2)), (byte)reg_names.at("zero"), 0, 0, 0, 0}, J)}; }  // jal x0, imm[20:1]
                    else
                    {
                        printf("Assembler error in %s, line %llu: Offset too large for \"j\"; use \"tail\" instead\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                }
                else if (instruction[0].compare("jal") == 0)
                {
                    word_size imm = instruction.size() > 2 ? operands[1] : operands[0];
                    byte rd = instruction.size() > 2 ? operands[0] : reg_names.at("ra");  // jal will link to x1 if rd is not specified
                    if (((imm & (word_size)(-1048576)) == (word_size)(-1048576)) || ((imm & (word_size)(-1048576)) == 0))   // if the signed immediate can fit in 21 bits
                        { machine_code = {getEncodedInstructionFromFormat({true, JAL, (word)(imm & (word_size)(-2)), (byte)rd, 0, 0, 0, 0}, J)}; }  // jal rd, imm[20:1]
                    else
                    {
                        printf("Assembler error in %s, line %llu: Offset too large for \"jal\"; use \"call\" instead\n", 
                            asm_filename.c_str(), line_num);
                        fclose(asm_ptr);
                        fclose(bin_ptr);
                        if (data_ptr != NULL) { fclose(data_ptr); }
                        return false;
                    }
                }
                else if (instruction[0].compare("jalr") == 0)
                {
                    byte rd = 0, rs1 = 0;
                    word imm = 0;
                    
                    if (instruction.size() == 2)  // jalr x1, rs1, 0
                    {
                        rd = reg_names.at("ra");
                        rs1 = operands[0];
                        imm = 0;
                    }
                    else if (instruction.size() == 3)  // jalr rd, rs1, 0
                    {
                        rd = operands[0];
                        rs1 = operands[1];
                        imm = 0;
                    }
                    else if (instruction.size() >= 4)  // jalr rd, rs1, imm
                    {
                        rd = operands[0];
                        rs1 = operands[1];
                        imm = operands[2];
                    }

                    machine_code = {getEncodedInstructionFromFormat({true, JALR, (word)imm, (byte)rd, (byte)rs1, 0, 0, 0}, I)};
                }
                else if (instruction[0].compare("jr") == 0)
                {
                    byte rs1 = 0;
                    word imm = 0;

                    if (instruction.size() == 2)  // jalr x0, rs1, 0
                    {
                        rs1 = operands[0];
                        imm = 0;
                    }
                    else if (instruction.size() >= 3)  // jalr x0, rs1, imm
                    {
                        rs1 = operands[0];
                        imm = operands[1];
                    }

                    machine_code = {getEncodedInstructionFromFormat({true, JALR, (word)imm, (byte)reg_names.at("zero"), (byte)rs1, 0, 0, 0}, I)}; 
                }
                else if (instruction[0].compare("la") == 0)
                {
                    word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                    machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)operands[0], 0, 0, 0, 0}, U),                                   // auipc rd, sym[31:12]
                                    getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)(operands[1] & 0xFFF), (byte)operands[0], (byte)operands[0], 0, 0, 0}, I)};  // addi rd, rd, sym[11:0]
                }
                else if (instruction[0].compare("lb") == 0) 
                {
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, (word)operands[1], (byte)operands[0], (byte)operands[2], 0, 0, 0}, I)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, 0, (byte)operands[0], (byte)operands[1], 0, 0, 0}, I)}; }  // lb rd, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)operands[0], 0, 0, 0, 0}, U),                            // auipc rd, sym[31:12]
                                        getEncodedInstructionFromFormat({true, LOAD, (word)(operands[1] & 0xFFF), (byte)operands[0], (byte)operands[0], 0, 0, 0}, I)};  // lb rd, sym[11:0](rd)
                    }
                }
                else if (instruction[0].compare("lbu") == 0) 
                { 
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, (word)operands[1], (byte)operands[0], (byte)operands[2], 0, 0x4, 0}, I)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, 0, (byte)operands[0], (byte)operands[1], 0, 0x4, 0}, I)}; }  // lbu rd, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)operands[0], 0, 0, 0, 0}, U),                              // auipc rd, sym[31:12]
                                        getEncodedInstructionFromFormat({true, LOAD, (word)(operands[1] & 0xFFF), (byte)operands[0], (byte)operands[0], 0, 0x4, 0}, I)};  // lbu rd, sym[11:0](rd)
                    }
                }
                else if (instruction[0].compare("ld") == 0)
                {
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, (word)operands[1], (byte)operands[0], (byte)operands[2], 0, 0x3, 0}, I)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, 0, (byte)operands[0], (byte)operands[1], 0, 0x3, 0}, I)}; }  // ld rd, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)operands[0], 0, 0, 0, 0}, U),                              // auipc rd, sym[31:12]
                                        getEncodedInstructionFromFormat({true, LOAD, (word)(operands[1] & 0xFFF), (byte)operands[0], (byte)operands[0], 0, 0x3, 0}, I)};  // ld rd, sym[11:0](rd)
                    }
                }
                else if (instruction[0].compare("lh") == 0)
                { 
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, (word)operands[1], (byte)operands[0], (byte)operands[2], 0, 0x1, 0}, I)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, 0, (byte)operands[0], (byte)operands[1], 0, 0x1, 0}, I)}; }  // lh rd, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)operands[0], 0, 0, 0, 0}, U),                              // auipc rd, sym[31:12]
                                        getEncodedInstructionFromFormat({true, LOAD, (word)(operands[1] & 0xFFF), (byte)operands[0], (byte)operands[0], 0, 0x1, 0}, I)};  // lh rd, sym[11:0](rd)
                    }
                }
                else if (instruction[0].compare("lhu") == 0) 
                { 
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, (word)operands[1], (byte)operands[0], (byte)operands[2], 0, 0x5, 0}, I)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, 0, (byte)operands[0], (byte)operands[1], 0, 0x5, 0}, I)}; }  // lhu rd, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)operands[0], 0, 0, 0, 0}, U),                              // auipc rd, sym[31:12]
                                        getEncodedInstructionFromFormat({true, LOAD, (word)(operands[1] & 0xFFF), (byte)operands[0], (byte)operands[0], 0, 0x5, 0}, I)};  // lhu rd, sym[11:0](rd)
                    }
                }
                else if (instruction[0].compare("li") == 0)
                {
                    if (((operands[1] & (word_size)(-2048)) == (word_size)(-2048)) || ((operands[1] & (word_size)(-2048)) == 0))                                                     // if the signed immediate can fit in 12 bits
                        { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[1], (byte)operands[0], (byte)reg_names.at("zero"), 0, 0, 0}, I)}; }  // addi rd x0 imm
                    else if ((sizeof(word_size) <= 4) || ((operands[1] & (word_size)(-2147483648)) == (word_size)(-2147483648)) || ((operands[1] & (word_size)(-2147483648)) == 0))  // if the signed immediate can fit in 32 bits
                    {
                        word_size upper_imm = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, LUI, (word)(upper_imm), (byte)operands[0], 0, 0, 0, 0}, U),                                     // lui rd, imm[31:12]
                                        getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)(operands[1] & 0xFFF), (byte)operands[0], (byte)operands[0], 0, 0, 0}, I)};  // addi rd, rd, imm[11:0]
                    }
                    else                                                                                                                                                             // otherwise, signed immediate is 64 bits
                    {
                        double_word upper_word_20 = operands[1] & 0xFFFFF00000000000;
                        double_word upper_word_12 = operands[1] & 0x00000FFF00000000;
                        double_word lower_word_20 = operands[1] & 0x00000000FFFFF000;
                        double_word lower_word_12 = operands[1] & 0x0000000000000FFF;
                        double_word temp = operands[1];

                        if (((temp >> 11) & 1) == 1)
                        {
                            temp = (upper_word_20 | upper_word_12 | lower_word_20) - 0xFFFFFFFFFFFFF000;
                            upper_word_20 = temp & 0xFFFFF00000000000;
                            upper_word_12 = temp & 0x00000FFF00000000;
                            lower_word_20 = temp & 0x00000000FFFFF000;
                        }
                        if(((temp >> 31) & 1) == 1)
                        {
                            temp = (upper_word_20 | upper_word_12) - 0xFFFFFFFF00000000;
                            upper_word_20 = temp & 0xFFFFF00000000000;
                            upper_word_12 = temp & 0x00000FFF00000000;
                        }
                        if(((temp >> 43) & 1) == 1)
                        {
                            temp = upper_word_20 - 0xFFFFF00000000000;
                            upper_word_20 = temp & 0xFFFFF00000000000;
                        }

                        machine_code = {getEncodedInstructionFromFormat({true, LUI, (word)(upper_word_20 >> 32), (byte)operands[0], 0, 0, 0, 0}, U),                                  // lui rd, imm[63:44]
                                        getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)(upper_word_12 >> 32), (byte)operands[0], (byte)operands[0], 0, 0, 0}, I),          // addi rd, rd, imm[43:32]
                                        getEncodedInstructionFromFormat({true, ARITH_LOG_I, 32, (byte)operands[0], (byte)operands[0], 0, 0x1, 0}, I),                                 // slli rd, rd, 32
                                        getEncodedInstructionFromFormat({true, LUI, (word)(lower_word_20), (byte)reg_names.at("t6"), 0, 0, 0, 0}, U),                                 // lui t6, imm[31:12]
                                        getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)(lower_word_12), (byte)reg_names.at("t6"), (byte)reg_names.at("t6"), 0, 0, 0}, I),  // addi t6, t6, imm[11:0]
                                        getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[0], (byte)reg_names.at("t6"), 0, 0}, R),             // add rd, rd, t6
                                        getEncodedInstructionFromFormat({true, ARITH_LOG_I, 0, (byte)reg_names.at("t6"), (byte)reg_names.at("zero"), 0, 0, 0}, I)};                   // addi t6, x0, 0
                    }
                }
                else if (instruction[0].compare("lui") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, LUI, (word)(operands[1] << 12), (byte)operands[0], 0, 0, 0, 0}, U)}; }
                else if (instruction[0].compare("lw") == 0)
                {
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, (word)operands[1], (byte)operands[0], (byte)operands[2], 0, 0x2, 0}, I)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, 0, (byte)operands[0], (byte)operands[1], 0, 0x2, 0}, I)}; }  // lw rd, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)operands[0], 0, 0, 0, 0}, U),                              // auipc rd, sym[31:12]
                                        getEncodedInstructionFromFormat({true, LOAD, (word)(operands[1] & 0xFFF), (byte)operands[0], (byte)operands[0], 0, 0x2, 0}, I)};  // lw rd, sym[11:0](rd)
                    }
                }
                else if (instruction[0].compare("lwu") == 0)
                {
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, (word)operands[1], (byte)operands[0], (byte)operands[2], 0, 0x6, 0}, I)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, LOAD, 0, (byte)operands[0], (byte)operands[1], 0, 0x6, 0}, I)}; }  // lwu rd, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)operands[0], 0, 0, 0, 0}, U),                              // auipc rd, sym[31:12]
                                        getEncodedInstructionFromFormat({true, LOAD, (word)(operands[1] & 0xFFF), (byte)operands[0], (byte)operands[0], 0, 0x6, 0}, I)};  // lwu rd, sym[11:0](rd)
                    }
                }
                else if (instruction[0].compare("mul") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0, 0x1}, R)}; }
                else if (instruction[0].compare("mulh") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x1, 0x1}, R)}; }
                else if (instruction[0].compare("mulhsu") == 0)  { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x2, 0x1}, R)}; }
                else if (instruction[0].compare("mulhu") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x3, 0x1}, R)}; }
                else if (instruction[0].compare("mulw") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0, 0x1}, R)}; }
                else if (instruction[0].compare("mv") == 0)      { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, 0, (byte)operands[0], (byte)operands[1], 0, 0, 0}, I)}; }  // addi rd, rs1, 0
                else if (instruction[0].compare("neg") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)reg_names.at("zero"), (byte)operands[1], 0, 0x20}, R)}; }  // sub rd, x0, rs1
                else if (instruction[0].compare("negw") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)reg_names.at("zero"), (byte)operands[1], 0, 0x20}, R)}; }  // subw rd, x0, rs1
                else if (instruction[0].compare("nop") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, 0, 0, 0, 0, 0, 0}, I)}; }  // addi x0, x0, 0
                else if (instruction[0].compare("not") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word) (-1), (byte)operands[0], (byte)operands[1], 0, 0x4, 0}, I)}; }  // xori rd, rs1, -1
                else if (instruction[0].compare("or") == 0)      { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x6, 0}, R)}; }
                else if (instruction[0].compare("ori") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[2], (byte)operands[0], (byte)operands[1], 0, 0x6, 0}, I)}; }
                else if (instruction[0].compare("rem") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x6, 0x1}, R)}; }
                else if (instruction[0].compare("remu") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x7, 0x1}, R)}; }
                else if (instruction[0].compare("remuw") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x7, 0x1}, R)}; }
                else if (instruction[0].compare("remw") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x6, 0x1}, R)}; }
                else if (instruction[0].compare("ret") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, JALR, 0, 0, (byte)reg_names.at("ra"), 0, 0, 0}, I)}; } // jr ra, 0
                else if (instruction[0].compare("sb") == 0)
                {
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, STORE, (word)operands[1], 0, (byte)operands[2], (byte)operands[0], 0, 0}, S)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, STORE, 0, 0, (byte)operands[1], (byte)operands[0], 0, 0}, S)}; }  // sb rs2, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        byte temp_reg = (instruction.size() > 3) ? operands[2] : reg_names.at("t6");
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)temp_reg, 0, 0, 0, 0}, U),                             // auipc rt, sym[31:12]
                                        getEncodedInstructionFromFormat({true, STORE, (word)(operands[1] & 0xFFF), 0, (byte)temp_reg, (byte)operands[0], 0, 0}, S)};  // sb rs2, sym[11:0](rt)
                        if (instruction.size() <= 3) 
                            { machine_code.push_back(getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)reg_names.at("t6"), (byte)reg_names.at("zero"), (byte)reg_names.at("zero"), 0, 0}, R)); }  // add t6, x0, x0
                    }
                }
                else if (instruction[0].compare("sd") == 0)
                { 
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, STORE, (word)operands[1], 0, (byte)operands[2], (byte)operands[0], 0x3, 0}, S)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, STORE, 0, 0, (byte)operands[1], (byte)operands[0], 0x3, 0}, S)}; }  // sd rs2, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        byte temp_reg = (instruction.size() > 3) ? operands[2] : reg_names.at("t6");
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)temp_reg, 0, 0, 0, 0}, U),                               // auipc rt, sym[31:12]
                                        getEncodedInstructionFromFormat({true, STORE, (word)(operands[1] & 0xFFF), 0, (byte)temp_reg, (byte)operands[0], 0x3, 0}, S)};  // sd rs2, sym[11:0](rt)
                        if (instruction.size() <= 3) 
                            { machine_code.push_back(getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)reg_names.at("t6"), (byte)reg_names.at("zero"), (byte)reg_names.at("zero"), 0, 0}, R)); }  // add t6, x0, x0
                    }
                }
                else if (instruction[0].compare("seqz") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, 0x1, (byte)operands[0], (byte)operands[1], 0, 0x3, 0}, I)}; }  // sltiu rd, rs1, 1
                else if (instruction[0].compare("sext.b") == 0)
                {
                    word shamt = sizeof(word_size)*8 - 8;
                    machine_code = { getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)shamt & 0xBFF, (byte)operands[0], (byte)operands[1], 0, 0x1, 0}, I),    // slli rd, rs1, shamt
                                     getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)shamt | 0x400, (byte)operands[0], (byte)operands[0], 0, 0x5, 0}, I) };  // srai rd, rd, shamt
                }
                else if (instruction[0].compare("sext.h") == 0)
                {
                    word shamt = sizeof(word_size)*8 - 16;
                    machine_code = { getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)shamt & 0xBFF, (byte)operands[0], (byte)operands[1], 0, 0x1, 0}, I),    // slli rd, rs1, shamt
                                     getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)shamt | 0x400, (byte)operands[0], (byte)operands[0], 0, 0x5, 0}, I) };  // srai rd, rd, shamt
                }
                else if (instruction[0].compare("sext.w") == 0)
                {
                    if (sizeof(word_size) <= 4) { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, 0, (byte)operands[0], (byte)operands[1], 0, 0, 0}, I)}; }    // addi rd, rs1, 0
                    else                        { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I_W, 0, (byte)operands[0], (byte)operands[1], 0, 0, 0}, I)}; }  // addiw rd, rs1, 0
                }
                else if (instruction[0].compare("sgt") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[2], (byte)operands[1], 0x2, 0}, R)}; }  // slt rd, rs2, rs1
                else if (instruction[0].compare("sgtu") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[2], (byte)operands[1], 0x3, 0}, R)}; }  // sltu rd, rs2, rs1
                else if (instruction[0].compare("sgtz") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)reg_names.at("zero"), (byte)operands[1], 0x2, 0}, R)}; }  // slt rd, x0, rs1
                else if (instruction[0].compare("sh") == 0)
                { 
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, STORE, (word)operands[1], 0, (byte)operands[2], (byte)operands[0], 0x1, 0}, S)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, STORE, 0, 0, (byte)operands[1], (byte)operands[0], 0x1, 0}, S)}; }  // sh rs2, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        byte temp_reg = (instruction.size() > 3) ? operands[2] : reg_names.at("t6");
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)temp_reg, 0, 0, 0, 0}, U),                             // auipc rt, sym[31:12]
                                        getEncodedInstructionFromFormat({true, STORE, (word)(operands[1] & 0xFFF), 0, (byte)temp_reg, (byte)operands[0], 0x1, 0}, S)};  // sh rs2, sym[11:0](rt)
                        if (instruction.size() <= 3) 
                            { machine_code.push_back(getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)reg_names.at("t6"), (byte)reg_names.at("zero"), (byte)reg_names.at("zero"), 0, 0}, R)); }  // add t6, x0, x0
                    }
                }
                else if (instruction[0].compare("sll") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x1, 0}, R)}; }
                else if (instruction[0].compare("slli") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[2] & 0xBFF, (byte)operands[0], (byte)operands[1], 0, 0x1, 0}, I)}; }
                else if (instruction[0].compare("slliw") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I_W, (word)operands[2] & 0xBFF, (byte)operands[0], (byte)operands[1], 0, 0x1, 0}, I)}; }
                else if (instruction[0].compare("sllw") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x1, 0}, R)}; }
                else if (instruction[0].compare("slt") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x2, 0}, R)}; }
                else if (instruction[0].compare("slti") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[2], (byte)operands[0], (byte)operands[1], 0, 0x2, 0}, I)}; }
                else if (instruction[0].compare("sltiu") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[2], (byte)operands[0], (byte)operands[1], 0, 0x3, 0}, I)}; }
                else if (instruction[0].compare("sltu") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x3, 0}, R)}; }
                else if (instruction[0].compare("sltz") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)reg_names.at("zero"), 0x2, 0}, R)}; }  // slt rd, rs1, x0
                else if (instruction[0].compare("snez") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)reg_names.at("zero"), (byte)operands[1], 0x3, 0}, R)}; }  // sltu rd, x0, rs1
                else if (instruction[0].compare("sra") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x5, 0x20}, R)}; }
                else if (instruction[0].compare("srai") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[2] | 0x400, (byte)operands[0], (byte)operands[1], 0, 0x5, 0}, I)}; }
                else if (instruction[0].compare("sraiw") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I_W, (word)operands[2] | 0x400, (byte)operands[0], (byte)operands[1], 0, 0x5, 0}, I)}; }
                else if (instruction[0].compare("sraw") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x5, 0x20}, R)}; }
                else if (instruction[0].compare("srl") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x5, 0}, R)}; }
                else if (instruction[0].compare("srli") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[2] & 0xBFF, (byte)operands[0], (byte)operands[1], 0, 0x5, 0}, I)}; }
                else if (instruction[0].compare("srliw") == 0)   { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I_W, (word)operands[2] & 0xBFF, (byte)operands[0], (byte)operands[1], 0, 0x5, 0}, I)}; }
                else if (instruction[0].compare("srlw") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x5, 0}, R)}; }
                else if (instruction[0].compare("sub") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0, 0x20}, R)}; }
                else if (instruction[0].compare("subw") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R_W, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0, 0x20}, R)}; }
                else if (instruction[0].compare("sw") == 0)
                { 
                    if (instruction[2].find('(') != std::string::npos)  // if loading from an indexed register
                        { machine_code = {getEncodedInstructionFromFormat({true, STORE, (word)operands[1], 0, (byte)operands[2], (byte)operands[0], 0x2, 0}, S)}; }
                    else if (reg_names.count(instruction[2]) > 0 || (sscanf((instruction[2]+"\1").c_str(), "x%d%c", &temp_num, &temp_char) >= 1 && temp_char == '\1'))  // if loading directly from a register
                        { machine_code = {getEncodedInstructionFromFormat({true, STORE, 0, 0, (byte)operands[1], (byte)operands[0], 0x2, 0}, S)}; }  // sw rs2, 0(rs1)
                    else  // otherwise, loading from a symbol address
                    {
                        byte temp_reg = (instruction.size() > 3) ? operands[2] : reg_names.at("t6");
                        word_size upper_sym = (word)(operands[1] & 0xFFFFF000) - ((((operands[1] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                        machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_sym), (byte)temp_reg, 0, 0, 0, 0}, U),                               // auipc rt, sym[31:12]
                                        getEncodedInstructionFromFormat({true, STORE, (word)(operands[1] & 0xFFF), 0, (byte)temp_reg, (byte)operands[0], 0x2, 0}, S)};  // sw rs2, sym[11:0](rt)
                        if (instruction.size() <= 3) 
                            { machine_code.push_back(getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)reg_names.at("t6"), (byte)reg_names.at("zero"), (byte)reg_names.at("zero"), 0, 0}, R)); }  // add t6, x0, x0
                    }
                }
                else if (instruction[0].compare("tail") == 0)
                {
                    word_size upper_imm = (word)(operands[0] & 0xFFFFF000) - ((((operands[0] >> 11) & 1) == 0) ? 0 : 0xFFFFF000);
                    machine_code = {getEncodedInstructionFromFormat({true, AUIPC, (word)(upper_imm), (byte)reg_names.at("t6"), 0, 0, 0, 0}, U),                                    // auipc t6, imm[31:12]
                                    getEncodedInstructionFromFormat({true, JALR, (word)(operands[0] & 0xFFE), (byte)reg_names.at("zero"), (byte)reg_names.at("t6"), 0, 0, 0}, I),  // jalr x0, t6, imm[11:0]
                                    getEncodedInstructionFromFormat({true, ARITH_LOG_I, 0, (byte)reg_names.at("t6"), (byte)reg_names.at("zero"), 0, 0, 0}, I)};                    // addi t6, x0, 0
                }
                else if (instruction[0].compare("xor") == 0)     { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_R, 0, (byte)operands[0], (byte)operands[1], (byte)operands[2], 0x4, 0}, R)}; }
                else if (instruction[0].compare("xori") == 0)    { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)operands[2], (byte)operands[0], (byte)operands[1], 0, 0x4, 0}, I)}; }
                else if (instruction[0].compare("zext.b") == 0)  { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)0x0FF, (byte)operands[0], (byte)operands[1], 0, 0x7, 0}, I)}; }  // andi rd, rs1, 0x0FF
                else if (instruction[0].compare("zext.h") == 0)
                {
                    word shamt = sizeof(word_size)*8 - 16;
                    machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)shamt & 0xBFF, (byte)operands[0], (byte)operands[1], 0, 0x1, 0}, I),   // slli rd, rs1, shamt
                                    getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)shamt & 0xBFF, (byte)operands[0], (byte)operands[0], 0, 0x5, 0}, I)};  // srli rd, rd, shamt
                }
                else if (instruction[0].compare("zext.w") == 0)
                {
                    word shamt = sizeof(word_size)*8 - 32;
                    if (sizeof(word_size) <= 4) { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, 0, (byte)operands[0], (byte)operands[1], 0, 0, 0}, I)}; }                      // addi rd, rs1, 0
                    else                        { machine_code = {getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)shamt & 0xBFF, (byte)operands[0], (byte)operands[1], 0, 0x1, 0}, I),     // slli rd, rs1, shamt
                                                                  getEncodedInstructionFromFormat({true, ARITH_LOG_I, (word)shamt & 0xBFF, (byte)operands[0], (byte)operands[0], 0, 0x5, 0}, I)}; }  // srli rd, rd, shamt
                }
                else  // ILLEGAL INSTRUCTION
                {
                    printf("Assembler error in %s, line %llu: Illegal instruction\n", 
                        asm_filename.c_str(), line_num);
                    fclose(asm_ptr);
                    fclose(bin_ptr);
                    if (data_ptr != NULL) { fclose(data_ptr); }
                    return false;
                }
                
                if (assembling && !forward_ref_found)
                {
                    fwrite(machine_code.data(), sizeof(word), machine_code.size(), bin_ptr);  // write instruction's machine code into binary file
                }
                if (!forward_ref_found) { curr_program_address += (4 * machine_code.size()); }
            }
        }

        instruction = {};  // reset strings
    }
    
    // check for any unassigned symbols
    for (const auto& symbol : sym_table)
    {
        if (symbol.second == (word_size) (-1))
        {
            printf("Assembler error in %s: Unassigned symbol \"%s\"\n", 
                asm_filename.c_str(), symbol.first.c_str());
            fclose(asm_ptr);
            fclose(bin_ptr);
            if (data_ptr != NULL) { fclose(data_ptr); }
            return false;
        }
    }

    // check for any non-existant symbols
    if(!assembling)
    {
        printf("Assembler error in %s, line %llu: Symbol \"%s\" doesn't exist\n",
            asm_filename.c_str(), return_line_num, target_sym.c_str());
        fclose(asm_ptr);
        fclose(bin_ptr);
        if (data_ptr != NULL) { fclose(data_ptr); }
        return false;
    }

    // Close files
    fclose(asm_ptr);
    fclose(bin_ptr);
    if (data_ptr != NULL) { fclose(data_ptr); }

    return true;
}

// Assemble all files in Programs directory
template <typename word_size = word>
bool assemble(endian_t endian = LITTLE)
{
    if(!assemble<word_size>("./Programs/bootloader.s", endian)) { return false; } // assemble bootloader
    printf("Successfully assembled bootloader\n");
    if(!assemble<word_size>("./Programs/program.s", endian, 0x40000000)) { return false; } // assemble main program
    printf("Successfully assembled main program\n");
    if(!assemble<word_size>("./Programs/interrupt_handler.s", endian)) { return false; } // assemble interrupt handler
    printf("Successfully assembled interrupt handler\n\n");
    return true;
}

#endif