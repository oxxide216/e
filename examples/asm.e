proc main() -> s64
  let str = "Hello, World!\n"
  let ptr = str.ptr
  let len = str.len
  asm "  mov rax,1"
  asm "  mov rdi,1"
  asm "  mov rsi,", ptr
  asm "  mov edx,", len
  asm "  syscall"

  retval 0
end
