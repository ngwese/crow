local M = {}

-- common value OR'd with command numbers to indicate get (versus set) operation
M.get_offset = 0x80

function M.base_address(f)
  local a = f.i2c_address
  if type(a) == 'table' then
    return a[1]
  end
  return a
end

function M.addresses(f)
  local a = f.i2c_address
  if type(a) == 'table' then
    return a
  end
  -- only one address; make it a table of length one
  return {a}
end

return M