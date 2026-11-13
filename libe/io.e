use unix -> O_RDONLY
use str
use mem

proc print(str: str)
  let len = str.len
  unix::write(1, str.ptr, len as u64)
end

proc println(str: str)
  print(str)
  unix::write(1, "\n".ptr, 1 as u64)
end

proc print(str: &u8)
  let len = str::cstr_len(str)
  unix::write(1, str, len as u64)
end

proc println(str: &u8)
  print(str)
  unix::write(1, "\n".ptr, 1 as u64)
end

# TODO: make it return str. This requires changes to the compiler
proc read_file(path: &u8) -> &u8
  let fs = unix::FileStat: end
  if unix::stat(path, &fs) < 0
    retval null
  end

  let fd = unix::open(path, O_RDONLY, 0 as s32)
  if fd < 0
    retval null
  end

  let result = mem::alloc((fs.st_size + 1) as u64)
  if result as s64 < 0
    retval null
  end

  unix::read(fd, result, fs.st_size as u64)
  result[fs.st_size] = 0 as u8
  unix::close(fd)

  retval result
end

proc read_file(path: str) -> &u8
  retval read_file(path.ptr)
end
