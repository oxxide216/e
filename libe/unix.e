# https://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/

proc syscall(type: s64)
  asm "mov rax,", type
  asm "syscall"
end

proc syscall(type: s64, arg0: s64)
  asm "mov rax,", type
  asm "mov rdi,", arg0
  asm "syscall"
end

proc syscall(type: s64, arg0: s64, arg1: s64, arg2: s64)
  asm "mov rax,", type
  asm "mov rdi,", arg0
  asm "mov rsi,", arg1
  asm "mov rdx,", arg2
  asm "syscall"
end

proc read(fd: s64, str: &u8, len:u64)
  syscall(0, fd, str as s64, len as s64)
end

proc write(fd: s64, str: &u8, len: u64)
  syscall(1, fd, str as s64, len as s64)
end

proc open(filename: &u8, flags: s32, mode: s32)
  syscall(2, filename as s64, flags, mode)
end

proc close(fd: s64)
  syscall(3, fd)
end

proc lseek(fd: s64, s8: offset, u32 origin)
  syscall(8, fd, offset, origin)
end

proc exit(code: u8)
  syscall(60, code as s64)
end
