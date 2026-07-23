proc str_len(str: &u8) -> u32
  let len = 0 as u32
  while str[len]
    len = len + 1 as u32
  end
  retval len
end
