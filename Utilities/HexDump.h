#ifndef HEX_DUMP_H
#define HEX_DUMP_H

#include <stdio.h>
#include <string>
#include "DataTypes.h"
#include "../Components/Memory.h"

// hex dump from a file
void hexDump(std::string filename, word start = 0, word end = -1)
{
    FILE *file_ptr;
    file_ptr = fopen(filename.c_str(), "rb");

    if (file_ptr == NULL) 
    {
        perror("Error opening file");
        return;
    }

    if (start > end) 
    {
        printf("Start must be before end\n");
        return;
    }

    bool print_file_size = start == 0 && end == -1;  // print file size if default parameters are passed
    word file_size = 0;

    word new_start = start & 0xFFFFFFF0;  // set to start of line
    fseek(file_ptr, new_start, SEEK_SET);  // skip to new start

    word byte_num = new_start;  // byte number (printed for every line / 16 bytes)
    int curr_byte;  // current byte being read
    std::string ascii = "................";  // ascii representation of line of bytes
    bool print_byte;  // flag to either print byte or dashes

    while((((curr_byte = fgetc(file_ptr)) != EOF) && (byte_num <= end)) || (byte_num % 16 != 0))
    {
        print_byte = curr_byte != EOF && byte_num >= start && byte_num <= end;
        if (curr_byte != EOF) { file_size++; }

        if (byte_num % 16 == 0) { printf("%08X:  ", byte_num); }  // start with byte number in hex

        if (print_byte) { printf("%02X%s", curr_byte, (byte_num % 8 == 7) ? "  " : " "); }  // print byte
        else { printf("--%s", (byte_num % 8 == 7) ? "  " : " "); }  // otherwise, print dashes

        if (print_byte && curr_byte >= ' ' && curr_byte <= '~') { ascii[byte_num % 16] = curr_byte; }  // set ascii

        if (byte_num % 16 == 15)  // if last byte in line
        {
            printf("[%s]\n", ascii.c_str());
            ascii = "................";  // reset ascii back to all dots
        }

        byte_num++;
    }

    if (print_file_size) { printf("\nFile size: %u bytes\n", file_size); }

    fclose(file_ptr);
}

// hex dump from a memory object
template <typename word_size = word>
void hexDump(Memory<word_size> &mem, word_size start, word_size end)
{
    if (start > end) 
    {
        printf("Start must be before end\n");
        return;
    }

    word new_start = start & 0xFFFFFFF0;  // set to start of line

    word_size byte_num = new_start;  // byte number (printed for every line / 16 bytes)
    byte curr_byte;  // current byte being read
    std::string ascii = "................";  // ascii representation of line of bytes
    bool print_byte;  // flag to either print byte or dashes

    while((byte_num <= end) || (byte_num % 16 != 0))
    {
        print_byte = byte_num >= start && byte_num <= end;

        curr_byte = mem.getByte(byte_num);

        if (byte_num % 16 == 0)
        { 
            printf(sizeof(word_size) <= 4 ? "%08X:  " : "%016llX:  ", byte_num);  // start with byte number in hex
        }

        if (print_byte) { printf("%02X%s", curr_byte, (byte_num % 8 == 7) ? "  " : " "); }  // print byte
        else { printf("--%s", (byte_num % 8 == 7) ? "  " : " "); }  // otherwise, print dashes

        if (print_byte && curr_byte >= ' ' && curr_byte <= '~') { ascii[byte_num % 16] = curr_byte; }  // set ascii

        if (byte_num % 16 == 15)  // if last byte in line
        {
            printf("[%s]\n", ascii.c_str());
            ascii = "................";  // reset ascii back to all dots
        }

        byte_num++;
    }
}
#endif