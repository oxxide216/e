use unix -> O_RDONLY
use str
use mem

proc print(str: str)
  unix::write(1, str.ptr, str.len as u64)
end

proc println(str: str)
  print(str)
  unix::write(1, "\n".ptr, 1 as u64)
end

proc print(str: &u8)
  unix::write(1, str, str::cstr_len(str) as u64)
end

proc println(str: &u8)
  print(str)
  unix::write(1, "\n".ptr, 1 as u64)
end

# TODO: make it return str. This requires changes to the compiler
proc read_file(path: &u8) -> str
  let fs = unix::FileStat: end
  if unix::stat(path, &fs) < 0
    retval null as str
  end

  let fd = unix::open(path, O_RDONLY, 0 as s32)
  if fd < 0
    retval null as str
  end

  let result = mem::alloc(sizeof u32 + fs.st_size as u64 + 1 as u64)
  if result == null
    retval null as str
  end

  let size_ptr = result as &u32
  size_ptr := fs.st_size as u32

  unix::read(fd, result + sizeof u32, fs.st_size as u64)
  result[sizeof u32 + fs.st_size as u64] = 0 as u8
  unix::close(fd)

  retval result as str
end

proc read_file(path: str) -> str
  retval read_file(path.ptr)
end
