use str
use unix

proc print(str: &u8)
  let len = str_len(str)
  write(1, str, len)
end

proc println(str: &u8)
  print(str)
  write(1, "\n", 1 as u64)
end
