do return
{ module_name  = 'TXi'
, manufacturer = 'bpc'
, i2c_address  = 0x68
, lua_name     = 'txi'
, getter_impl  = 'txi_get'
, commands     =
  { { name = 'nop'
    , cmd = 99
    , args = { 'ignore', s8 }
    }
  }
, getters =
  {
    { name = 'param'
    , cmd  = 0x00
    , args = { 'channel', u8 }
    , retval = { 'value', u16 }
    }
    , { name = 'in'
    , cmd  = 0x40
    , args = { 'channel', u8 }
    , retval = { 'value', s16 }
    }
  }
}
end
