do return
{ module_name  = 'TXi'
, manufacturer = 'bpc'
, i2c_address  = 0x68
, lua_name     = 'txi'
, commands     =
  { { name = 'nop'
    , cmd = 99
    , args = { 'ignore', s8 }
    }
  }
, getters =
  { { name = 'in'
    , cmd  = 0 + get_offset
    , args = { 'channel', u8 }
    , retval = { 'value', s16 }
    }
  , { name = 'param'
    , cmd  = 2 + get_offset
    , args = { 'channel', u8 }
    , retval = { 'value', u16 }
    }
  }
}
end
