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

const PAGE_SIZE: u64 = 4096

const O_RDONLY: s32 = 0
const O_WRONLY: s32 = 1
const O_RDWR: s32 = 2

const SEEK_SET: s32 = 0  # Seek from the beginning of the file
const SEEK_CUR: s32 = 1  # Seek from the current file position
const SEEK_END: s32 = 2  # Seek from the end of the file

const PROT_READ: u64 = 1
const PROT_WRITE: u64 = 2
const PROT_EXEC: u64 = 4

const MAP_SHARED: u64 = 1
const MAP_PRIVATE: u64 = 2
const MAP_ANONYMOUS: u64 = 32

const POLLIN: s16 = 1
const POLLOUT: s16 = 4
const POLLERR: s16 = 8
const POLLHUP: s16 = 16
const POLLNVAL: s16 = 32

const AT_FDCWD: s32 = -100
const AT_REMOVEDIR: s32 = 512
const AT_SYMLINK_NOFOLLOW: s32 = 256

const O_CREAT: s32 = 64
const O_EXCL: s32 = 128
const O_TRUNC: s32 = 512
const O_APPEND: s32 = 1024
const O_NONBLOCK: s32 = 2048
const O_DIRECTORY: s32 = 65536
const O_NOFOLLOW: s32 = 131072
const O_CLOEXEC: s32 = 524288

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

proc read(fd: s64, str: &u8, len: u64) -> s64
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

proc mmap(addr: u64, len: u64, prot: u64, flags: u64, fd: s64, off: u64) -> s64
  retval syscall(9, addr as s64, len as s64, prot as s64, flags as s64, fd, off as s64)
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

# Open file relative to directory FD
proc openat(dirfd: s32, filename: &u8, flags: s32, mode: s32) -> s64
  retval syscall(257, dirfd as s64, filename as s64, flags as s64, mode as s64)
end

# Get file metadata relative to directory FD
proc newfstatat(dirfd: s32, filename: &u8, statbuf: &FileStat, flags: s32) -> s64
  retval syscall(262, dirfd as s64, filename as s64, statbuf as s64, flags as s64)
end

# Read directory entries
proc getdents64(fd: s32, dirp: &u8, count: u64) -> s64
  retval syscall(217, fd as s64, dirp as s64, count as s64)
end

# Create directory
proc mkdirat(dirfd: s32, pathname: &u8, mode: s32) -> s64
  retval syscall(258, dirfd as s64, pathname as s64, mode as s64)
end

# Remove file or directory
proc unlinkat(dirfd: s32, pathname: &u8, flags: s32) -> s64
  retval syscall(263, dirfd as s64, pathname as s64, flags as s64)
end

# Rename file
proc renameat(olddirfd: s32, oldpath: &u8, newdirfd: s32, newpath: &u8) -> s64
  retval syscall(264, olddirfd as s64, oldpath as s64, newdirfd as s64, newpath as s64)
end

# Create symbolic link
proc symlinkat(target: &u8, newdirfd: s32, linkpath: &u8) -> s64
  retval syscall(266, target as s64, newdirfd as s64, linkpath as s64)
end

# Read symbolic link
proc readlinkat(dirfd: s32, pathname: &u8, buf: &u8, bufsiz: u64) -> s64
  retval syscall(267, dirfd as s64, pathname as s64, buf as s64, bufsiz as s64)
end

# Duplicate file descriptor
proc dup(fd: s32) -> s64
  retval syscall(32, fd as s64)
end

# Duplicate file descriptor to specific number
proc dup2(oldfd: s32, newfd: s32) -> s64
  retval syscall(33, oldfd as s64, newfd as s64)
end

# Control file descriptor
proc fcntl(fd: s32, cmd: s32, arg: s64) -> s64
  retval syscall(72, fd as s64, cmd as s64, arg)
end

proc fsync(fd: s32) -> s64
  retval syscall(74, fd as s64)
end

proc fdatasync(fd: s32) -> s64
  retval syscall(75, fd as s64)
end

proc ftruncate(fd: s32, length: s64) -> s64
  retval syscall(77, fd as s64, length)
end