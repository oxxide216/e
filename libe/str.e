proc str_len(str: &u8) -> u64
  let len = 0 as u64
  while str[len] != 0 as u8
    len = len + 1 as u64
  end
  retval len
end
