#include "lib/lualink.h"


#include <string.h> // strcmp(), strlen()
#include <stdlib.h> // malloc(), free()

// Lua itself
#include "../submodules/lua/src/lua.h"
#include "../submodules/lua/src/lauxlib.h"
#include "../submodules/lua/src/lualib.h"

// Hardware IO
#include "ll/debug_usart.h" // U_Print*()
#include "lib/slews.h"      // S_toward
#include "lib/caw.h"        // Caw_send_*()
#include "lib/ii.h"         // II_*()
#include "lib/bootloader.h" // bootloader_enter()
#include "lib/metro.h"      // metro_start() metro_stop() metro_set_time()
#include "lib/io.h"         // IO_GetADC()
#include "lib/flash.h"      // Flash_*()

// Lua libs wrapped in C-headers: Note the extra '.h'
#include "lua/bootstrap.lua.h" // MUST LOAD THIS MANUALLY FIRST
#include "lua/crowlib.lua.h"
#include "lua/asl.lua.h"
#include "lua/asllib.lua.h"
#include "lua/metro.lua.h"
#include "lua/input.lua.h"
#include "lua/output.lua.h"

struct lua_lib_locator{ const char* name; const char* addr_of_luacode; };
const struct lua_lib_locator Lua_libs[] =
    { { "lua_crowlib", lua_crowlib }
    , { "lua_asl"    , lua_asl     }
    , { "lua_asllib" , lua_asllib  }
    , { "lua_metro"  , lua_metro   }
    , { "lua_input"  , lua_input   }
    , { "lua_output" , lua_output  }
    , { NULL         , NULL        }
    };

// Basic crow script
#include "lua/default.lua.h"

// Private prototypes
static void Lua_linkctolua( lua_State* L );
static uint8_t Lua_eval( lua_State*     L
                       , const char*    script
                       , size_t         script_len
                       , ErrorHandler_t errfn
                       );

lua_State* L; // global access for 'reset-environment'

// repl / script load stuff (TODO needs its own file)
char*    new_script;
uint16_t new_script_len;
static void Lua_new_script_buffer( void );
L_repl_mode repl_mode = REPL_normal;

// Public functions
void Lua_Init(void)
{
    L = luaL_newstate();
    luaL_openlibs(L);
    Lua_linkctolua(L);
    Lua_eval(L, lua_bootstrap
              , strlen(lua_bootstrap)
              , U_PrintLn
              ); // redefine dofile(), print(), load crowlib
    // TODO fallback if error
    if( Flash_is_user_script() ){
        Lua_new_script_buffer();
        if( Flash_read_user_script( new_script, &new_script_len ) ){
            U_PrintLn("can't find user script");
        }
        if( Lua_eval( L, new_script
                       , new_script_len
                       , Caw_send_luaerror
                       ) ){
            U_PrintLn("failed to load user script");
        }
        U_PrintLn("free(script)");
        free(new_script);
    } else {
        Lua_eval(L, lua_default
                  , strlen(lua_default)
                  , U_PrintLn
                  ); // run default script
    }
}

void Lua_DeInit(void)
{
    lua_close(L);
}

// C-fns accessible to lua

// NB these static functions are prefixed  with '_'
// to avoid shadowing similar-named extern functions in other modules
// and also to distinguish from extern 'L_' functions.
static int _dofile( lua_State *L )
{
    const char* l_name = luaL_checkstring(L, 1);
    lua_pop( L, 1 );
    uint8_t i = 0;
    while( Lua_libs[i].addr_of_luacode != NULL ){
        if( !strcmp( l_name, Lua_libs[i].name ) ){ // if the strings match
            if( luaL_dostring( L, Lua_libs[i].addr_of_luacode ) ){
                U_Print("can't load library: ");
                U_PrintLn( (char*)Lua_libs[i].name );
                // lua error
                U_PrintLn( (char*)lua_tostring( L, -1 ) );
                lua_pop( L, 1 );
                goto fail;
            }
            return 1; // table is left on the stack as retval
        }
        i++;
    }
    U_Print("can't find library: ");
    U_PrintLn( (char*)l_name );
fail:
    lua_pushnil(L);
    return 1;
}
static int _debug( lua_State *L )
{
    const char* msg = luaL_checkstring(L, 1);
    lua_pop( L, 1 );
    U_PrintLn( (char*)msg);
    return 0;
}
static int _print_serial( lua_State *L )
{
    Caw_send_luachunk( (char*)luaL_checkstring(L, 1) );
    lua_pop( L, 1 );
    return 0;
}
static int _bootloader( lua_State *L )
{
    bootloader_enter();
    return 0;
}
static int _go_toward( lua_State *L )
{
    //const char* shape = luaL_checkstring(L, 4);
    S_toward( luaL_checkinteger(L, 1)-1 // C is zero-based
            , luaL_checknumber(L, 2)
            , luaL_checknumber(L, 3) * 1000.0
            , SHAPE_Linear // Shape_t
            , L_handle_toward
            );
    lua_pop( L, 4 );
    return 0;
}
static int _get_state( lua_State *L )
{
    float s = S_get_state( luaL_checkinteger(L, 1)-1 );
    lua_pop( L, 1 );
    lua_pushnumber( L, s );
    return 1;
}
static int _io_get_input( lua_State *L )
{
    float adc = IO_GetADC( luaL_checkinteger(L, 1)-1 );
    lua_pop( L, 1 );
    lua_pushnumber( L, adc );
    return 1;
}
static int _set_input_mode( lua_State *L )
{
    IO_SetADCaction( luaL_checkinteger(L, 1)-1
                   , luaL_checkstring(L, 2)
                   );
    lua_pop( L, 2 );
    return 0;
}
static int _send_usb( lua_State *L )
{
    // pattern match on type: handle values vs strings vs chunk
    const char* msg = luaL_checkstring(L, 1);
    lua_pop( L, 1 );
    uint32_t len = strlen(msg);
    Caw_send_raw( (uint8_t*) msg, len );
    return 0;
}
static int _send_ii( lua_State *L )
{
    // pattern match on broadcast vs query
    uint8_t istate = 4;
    II_broadcast( II_FOLLOW, 1, &istate, 1 );
    return 0;
}
static int _set_ii_addr( lua_State *L )
{
    // pattern match on broadcast vs query
    uint8_t istate = 4;
    II_broadcast( II_FOLLOW, 1, &istate, 1 );
    return 0;
}
static int _metro_start( lua_State* L )
{
    static int idx = 0;
    float seconds = -1.0; // metro will re-use previous value
    int count = -1; // default: infinite
    int stage = 0;

    int nargs = lua_gettop(L);
    if (nargs > 0) { idx = (int) luaL_checkinteger(L, 1) - 1; } // 1-ix'd
    if (nargs > 1) { seconds = (float)luaL_checknumber(L, 2); }
    if (nargs > 2) { count = (int)luaL_checkinteger(L, 3); }
    if (nargs > 3) { stage = (int)luaL_checkinteger(L, 4) - 1; } // 1-ix'd
    lua_pop( L, 4 );

    Metro_start( idx+2, seconds, count, stage ); // +2 for adc
    lua_settop(L, 0);
    return 0;
}
static int _metro_stop( lua_State* L )
{
    if( lua_gettop(L) != 1 ){ return luaL_error(L, "wrong number of arguments"); }

    int idx = (int)luaL_checkinteger(L, 1) - 1; // 1-ix'd
    lua_pop( L, 1 );
    Metro_stop(idx+2); // +2 for adc
    lua_settop(L, 0);
    return 0;
}
static int _metro_set_time( lua_State* L )
{
    if( lua_gettop(L) != 2 ){ return luaL_error(L, "wrong number of arguments"); }

    int idx = (int)luaL_checkinteger(L, 1) - 1; // 1-ix'd
    float sec = (float) luaL_checknumber(L, 2);
    lua_pop( L, 2 );
    Metro_set_time(idx+2, sec); // +2 for adc
    lua_settop(L, 0);
    return 0;
}

// array of all the available functions
static const struct luaL_Reg libCrow[]=
        // bootstrap
    { { "c_dofile"       , _dofile           }
    , { "debug_usart"    , _debug            }
    , { "print_serial"   , _print_serial     }
        // system
    , { "sys_bootloader" , _bootloader       }
    //, { "sys_cpu_load"   , _sys_cpu          }
        // io
    , { "go_toward"      , _go_toward        }
    , { "get_state"      , _get_state        }
    , { "io_get_input"   , _io_get_input     }
    , { "set_input_mode" , _set_input_mode   }
        // usb
    , { "send_usb"       , _send_usb         }
        // i2c
    , { "send_ii"        , _send_ii          }
    , { "set_ii_addr"    , _set_ii_addr      }
        // metro
    , { "metro_start"    , _metro_start      }
    , { "metro_stop"     , _metro_stop       }
    , { "metro_set_time" , _metro_set_time   }

    , { NULL             , NULL              }
    };
// make functions available to lua
static void Lua_linkctolua( lua_State *L )
{
    // Make C fns available to Lua
    uint8_t fn = 0;
    while( libCrow[fn].func != NULL ){
        lua_pushcfunction( L, libCrow[fn].func );
        lua_setglobal( L, libCrow[fn].name );
        fn++;
    }
}

static uint8_t Lua_eval( lua_State*     L
                       , const char*    script
                       , size_t         script_len
                       , ErrorHandler_t errfn
                       ){
    int error;
    if( (error = luaL_loadbuffer( L, script, script_len, "eval" )
              || lua_pcall( L, 0, 0, 0 )
        ) ){
        //(*errfn)( (char*)lua_tostring( L, -1 ) );
        Caw_send_luachunk( (char*)lua_tostring( L, -1 ) );
        lua_pop( L, 1 );
        switch( error ){
            case LUA_ERRSYNTAX: U_PrintLn("!load script: syntax"); break;
            case LUA_ERRMEM:    U_PrintLn("!load script: memory"); break;
            case LUA_ERRRUN:    U_PrintLn("!exec script: runtime"); break;
            case LUA_ERRERR:    U_PrintLn("!exec script: err in err handler"); break;
            default: break;
        }
        return 1;
    }
    return 0;
}

void Lua_crowbegin( void )
{
    U_PrintLn("init()"); // call in C to avoid user seeing in lua
    lua_getglobal(L,"init");
    lua_pcall(L,0,0,0);
}

// TODO the repl/state/reception logic should be its own file
void Lua_repl_mode( L_repl_mode mode )
{
    repl_mode = mode;
    if( repl_mode == REPL_reception ){ // begin a new transmission
        Lua_new_script_buffer();
    } else { // end of a transmission
        if( !Lua_eval( L, new_script
                        , new_script_len
                        , Caw_send_luaerror
                        ) ){ // successful load
            // TODO if we're setting init() should check it doesn't crash
            if( Flash_write_user_script( new_script
                                       , new_script_len
                                       ) ){
                Caw_send_luachunk("flash write failed");
            }
            U_PrintLn("script saved");
        } else { U_PrintLn("new user script failed test"); }
        free(new_script); // cleanup memory
    }
}

void Lua_repl( char* buf, uint32_t len, ErrorHandler_t errfn )
{
    if( repl_mode == REPL_normal ){
        Lua_eval( L, buf
                   , len
                   , errfn
                   );
    } else {
        Lua_receive_script( buf, len, errfn );
    }
}

static void Lua_new_script_buffer( void )
{
    // TODO call to Lua to free resources from current script
    U_PrintLn("malloc(buf)");
    new_script = malloc(USER_SCRIPT_SIZE);
    if(new_script == NULL){
        Caw_send_luachunk("!script: out of memory");
        //(*errfn)("!script: out of memory");
        return; // how to deal with this situation?
        // FIXME: should respond over usb stating out of memory?
        //        try allocating a smaller amount and hope it fits?
        //        retry?
    }
    new_script_len = 0;
}

void Lua_receive_script( char* buf, uint32_t len, ErrorHandler_t errfn )
{
    memcpy( &new_script[new_script_len], buf, len );
    new_script_len += len;
}

void Lua_print_script( void )
{
    if( Flash_is_user_script() ){
        Lua_new_script_buffer();
        Flash_read_user_script( new_script, &new_script_len );
        uint16_t send_len = new_script_len;
        uint8_t page_count = 0;
        while( send_len > 0x200 ){
            Caw_send_raw( (uint8_t*)&new_script[(page_count++)*0x200], 0x200 );
            send_len -= 0x200;
        }
        Caw_send_raw( (uint8_t*)&new_script[page_count*0x200], send_len );
        free(new_script);
    } else {
        Caw_send_luachunk("no user script.");
    }
}

// Public Callbacks from C to Lua
void L_handle_toward( int id )
{
    lua_getglobal(L, "toward_handler");
    lua_pushinteger(L, id+1); // 1-ix'd
    if( lua_pcall(L, 1, 0, 0) != LUA_OK ){
        //U_PrintLn("error running toward_handler");
        Caw_send_luachunk("error running toward_handler");
        U_PrintLn( (char*)lua_tostring(L, -1) );
        lua_pop( L, 1 );
    }
}

void L_handle_metro( const int id, const int stage)
{
    lua_getglobal(L, "metro_handler");
    lua_pushinteger(L, id+1 -2); // 1-ix'd, less 2 for adc rebase
    lua_pushinteger(L, stage+1); // 1-ix'd
    if( lua_pcall(L, 2, 0, 0) != LUA_OK ){
        Caw_send_luachunk("error running metro_handler");
        Caw_send_luachunk( (char*)lua_tostring(L, -1) );
        lua_pop( L, 1 );
    }
}

void L_handle_in_stream( int id, float value )
{
    lua_getglobal(L, "stream_handler");
    lua_pushinteger(L, id+1); // 1-ix'd
    lua_pushnumber(L, value);
    if( lua_pcall(L, 2, 0, 0) != LUA_OK ){
        Caw_send_luachunk("error: input stream");
        Caw_send_luachunk( (char*)lua_tostring(L, -1) );
        lua_pop( L, 1 );
    }
}
