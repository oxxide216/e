use unix
use str

proc print(str: str)
  let len = str.len
  write(1, str.ptr, len as u64)
end

proc println(str: str)
  print(str)
  write(1, "\n".ptr, 1 as u64)
end

proc print(str: &u8)
  let len = str_len(str)
  write(1, str, len as u64)
end

proc println(str: &u8)
  print(str)
  write(1, "\n".ptr, 1 as u64)
end
