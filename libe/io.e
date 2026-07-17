use unix

proc print(str: str)
  let len = str.len
  write(1, str.ptr, len as u64)
end

proc println(str: str)
  print(str)
  write(1, "\n".ptr, 1 as u64)
end
