#pragma once

#include <stdint.h>

typedef enum{ ii_void
    , ii_u8
    , ii_s8
    , ii_u16
    , ii_s16
    , ii_s16V
    , ii_float   // 32bit (for crow to crow comm'n)
} ii_Type_t;

typedef struct{
    uint8_t cmd;
    uint8_t args;
    ii_Type_t return_type;
    ii_Type_t argtype[];
} ii_Cmd_t;

extern const char* ii_module_list;

const ii_Cmd_t* ii_find_command( uint8_t address, uint8_t cmd );
const char* ii_list_commands( uint8_t address );