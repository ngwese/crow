do return
{ module_name  = 'TXi'
, manufacturer = 'bpc'
, i2c_address  = 0x68
, lua_name     = 'txi'
, commands     = {}
, getters =
  { { name = 'in'
    , cmd  = 0
    , args = { 'channel', s8 }
    , retval = { 'value', u16 }
    }
  , { name = 'param'
    , cmd  = 2
    , args = { 'channel', s8 }
    , retval = { 'value', u16 }
    }
  }
}
end
