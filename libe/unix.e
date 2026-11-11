# https://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/

struct Timespec
  tv_sec: s64,   # Seconds since the Unix Epoch
  tv_nsec: s64   # Additional nanoseconds (0-999999999)
end

struct FileStat      # Linux x86-64 ABI Layout
  # Device and inode information
  st_dev: u64,       # ID of device containing the file
  st_ino: u64,       # Inode number
  st_nlink: u64,     # Number of hard links

  # File type and permissions
  st_mode: u32,      # File type and permission bits
  st_uid: u32,       # User ID of file owner
  st_gid: u32,       # Group ID of file owner
  __pad0: u32,       # Padding for ABI alignment

  # Special device information
  st_rdev: u64,      # Device ID (for special files)

  # File size and allocation
  st_size: s64,      # Total file size in bytes
  st_blksize: s64,   # Preferred block size for filesystem I/O
  st_blocks: s64,    # Number of allocated 512-byte blocks

  # File timestamps
  st_atim: Timespec, # Last access time
  st_mtim: Timespec, # Last modification time
  st_ctim: Timespec, # Last status change time

  # Reserved fields (must remain in the ABI layout)
  __reserved: [s64;; 3]
end

const SEEK_SET = 0  # Seek from the beginning of the file
const SEEK_CUR = 1  # Seek from the current file position
const SEEK_END = 2  # Seek from the end of the file

const POLLIN = 1
const POLLOUT = 4
const POLLERR = 8
const POLLHUP = 16
const POLLNVAL = 32

struct PollFd
  fd: s32,
  events: s16,   # Requested events
  revents: s16   # Returned events
end


proc syscall(type: s64) -> s64                                       # 0 args
  let result = 0 as s64
  asm "mov rax,", type
  asm "syscall"
  asm "mov ", result, ",rax"
  retval result
end

proc syscall(type: s64, arg0: s64) -> s64                            # 1 arg
  let result = 0 as s64
  asm "mov rax,", type
  asm "mov rdi,", arg0
  asm "syscall"
  asm "mov ", result, ",rax"
  retval result
end

proc syscall(type: s64, arg0: s64, arg1: s64) -> s64                 # 2 args
  let result = 0 as s64
  asm "mov rax,", type
  asm "mov rdi,", arg0
  asm "mov rsi,", arg1
  asm "syscall"
  asm "mov ", result, ",rax"
  retval result
end

proc syscall(type: s64, arg0: s64, arg1: s64, arg2: s64) -> s64      # 3 args
  let result = 0 as s64
  asm "mov rax,", type
  asm "mov rdi,", arg0
  asm "mov rsi,", arg1
  asm "mov rdx,", arg2
  asm "syscall"
  asm "mov ", result, ",rax"
  retval result
end

proc syscall(type: s64, arg0: s64, arg1: s64, arg2: s64, arg3: s64) -> s64      # 4 args
  let result = 0 as s64
  asm "mov rax,", type
  asm "mov rdi,", arg0
  asm "mov rsi,", arg1
  asm "mov rdx,", arg2
  asm "mov r10,", arg3
  asm "syscall"
  asm "mov ", result, ",rax"
  retval result
end

proc syscall(type: s64, arg0: s64, arg1: s64, arg2: s64, arg3: s64, arg4: s64) -> s64      # 5 args
  let result = 0 as s64
  asm "mov rax,", type
  asm "mov rdi,", arg0
  asm "mov rsi,", arg1
  asm "mov rdx,", arg2
  asm "mov r10,", arg3
  asm "mov r8,",  arg4
  asm "syscall"
  asm "mov ", result, ",rax"
  retval result
end

proc syscall(type: s64, arg0: s64, arg1: s64, arg2: s64, arg3: s64, arg4: s64, arg5: s64) -> s64      # 6 args
  let result = 0 as s64
  asm "mov rax,", type
  asm "mov rdi,", arg0
  asm "mov rsi,", arg1
  asm "mov rdx,", arg2
  asm "mov r10,", arg3
  asm "mov r8,",  arg4
  asm "mov r9,",  arg5
  asm "syscall"
  asm "mov ", result, ",rax"
  retval result
end

proc read(fd: s64, str: &u8, len:u64) -> s64
  retval syscall(0, fd, str as s64, len as s64)
end

proc write(fd: s64, str: &u8, len: u64) -> s64
  retval syscall(1, fd, str as s64, len as s64)
end

proc open(filename: &u8, flags: s32, mode: s32) -> s64
  retval syscall(2, filename as s64, flags as s64, mode as s64)
end

proc close(fd: s64) -> s64
  retval syscall(3, fd)
end

proc stat(filename: &u8, statbuf: &FileStat) -> s64
  retval syscall(4, filename as s64, statbuf as s64)
end

proc fstat(fd: s64, statbuf: &FileStat) -> s64
  retval syscall(5, fd, statbuf as s64)
end

proc lstat(filename: &u8, statbuf: &FileStat) -> s64
  retval syscall(6, filename as s64, statbuf as s64)
end

proc poll(fds: &PollFd, nfds: u64, timeout: s32) -> s64
  retval syscall(7, fds as s64, nfds as s64, timeout as s64)
end

proc lseek(fd: s32, offset: s64, whence: s32) -> s64
  retval syscall(8, fd as s64, offset, whence as s64)
end

proc mmap(addr: u64, len: u64, prot: u64, flags: u64, fd: u64, off: u64) -> s64
  retval syscall(9, addr as s64, len as s64, prot as s64, flags as s64, fd as s64, off as s64)
end

proc mprotect(start: u64, len: u64, prot: u64) -> s64
  retval syscall(10, start as s64, len as s64, prot as s64)
end

proc munmap(addr: u64, len: u64) -> s64
  retval syscall(11, addr as s64, len as s64)
end

proc brk(brk: u64) -> s64
  retval syscall(12, brk as s64)
end

proc exit(code: u8) -> s64
  retval syscall(60, code as s64)
end
