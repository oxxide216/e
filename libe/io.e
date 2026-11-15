use unix -> O_RDONLY
use str
use mem

struct File
  fd: s64,
  eof: bool,
  error: s64
end

const EPERM: s32 = -1      # Operation not permitted
const ENOENT: s32 = -2      # No such file or directory
const ESRCH: s32 = -3      # No such process
const EINTR: s32 = -4      # Interrupted system call
const EIO: s32 = -5      # I/O error
const ENXIO: s32 = -6      # No such device or address
const E2BIG: s32 = -7      # Argument list too long
const ENOEXEC: s32 =  -8      # Exec format error
const EBADF: s32 = -9      # Bad file number
const ECHILD: s32 = -10      # No child processes
const EAGAIN: s32 = -11      # Try again
const ENOMEM: s32 = -12      # Out of memory
const EACCES: s32 = -13      # Permission denied
const EFAULT: s32 = -14      # Bad address
const ENOTBLK: s32 = -15      # Block device required
const EBUSY: s32 = -16      # Device or resource busy
const EEXIST: s32 = -17      # File exists
const EXDEV: s32 = -18      # Cross-device link
const ENODEV: s32 = -19      # No such device
const ENOTDIR: s32 = -20      # Not a directory
const EISDIR: s32 = -21      # Is a directory
const EINVAL: s32 = -22      # Invalid argument
const ENFILE: s32 = -23      # File table overflow
const EMFILE: s32 = -24      # Too many open files
const ENOTTY: s32 = -25      # Not a typewriter
const ETXTBSY: s32 = -26      # Text file busy
const EFBIG : s32 = -27      # File too large
const ENOSPC: s32 = -28      # No space left on device
const ESPIPE: s32 = -29      # Illegal seek
const EROFS: s32 = -30      # Read-only file system
const EMLINK: s32 = -31      # Too many links
const EPIPE: s32 = -32      # Broken pipe
const EDOM: s32 = -33      # Math argument out of domain of func
const ERANGE: s32 = -34      # Math result not representable

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

proc fopen(path: &u8, flags: s32, file: &File) -> bool
  let fd = unix::open(path, flags, 0 as s32)

  if fd < 0
    retval false
  end
  
  file.fd = fd
  file.eof = false
  file.error = 0

  retval true
end

proc fread(file: &File, buffer: &u8, size: u64) -> s64
  let result = EINTR as s64
  while result == EINTR as s64
    result = unix::read(file.fd, buffer, size)
  end

  if result == 0
    file.eof = true
  end

  if result < 0
    file.error = result
  end

  if result > 0 
    file.eof = false
    file.error = 0
  end

  retval result
end

proc fwrite(file: &File, buffer: &u8, size: u64) -> s64
  let result = EINTR as s64
  while result == EINTR as s64
    result = unix::write(file.fd, buffer, size)
  end

  if result < 0
    file.error = result
  end

  if result > 0
    file.error = 0
  end

  retval result
end

proc fclose(file: &File) -> s64
  retval unix::close(file.fd)
end

proc fread_all(file: &File, buffer: &u8, size: u64) -> s64
  let total = 0 as u64

  while total < size
    let n = fread(file, buffer + total, size - total)

    if n <= 0
      break
    end

    total = total + n as u64
  end

  retval total as s64
end

proc fwrite_all(file: &File, buffer: &u8, size: u64) -> s64
    let offset = 0 as u64

    while offset < size
        let result = fwrite(file, buffer + offset, size - offset)

        if result <= 0
            retval -1
        end

        offset = offset + result as u64
    end

    retval offset as s64
end

# TODO: make it return String when such type will be introduced
proc read_file(path: &u8) -> &u8
  let fs = unix::FileStat: end
  if unix::stat(path, &fs) < 0
    retval null
  end

  let fd = unix::open(path, O_RDONLY, 0 as s32)
  if fd < 0
    retval null
  end

  let result = mem::alloc(fs.st_size as u64 + 1 as u64)
  if result == null
    retval null
  end

  unix::read(fd, result, fs.st_size as u64)
  result[fs.st_size as u64] = 0 as u8
  unix::close(fd)

  retval result
end

proc read_file(path: str) -> &u8
  retval read_file(path.ptr)
end
