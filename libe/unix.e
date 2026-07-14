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

proc write(fd: s64, str: &u8, len: u64)
  syscall(1, fd, str as s64, len as s64)
end

proc exit(code: u8)
  syscall(60, code as s64)
end
