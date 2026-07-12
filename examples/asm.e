proc str_len(str: &u8) -> s64
  let len = 0
  while *str != 0 as u8
    str = str + 1
    len = len + 1
  end
  retval len
end

proc main() -> s64
  let str = "Hello, World!\n"
  let len = str_len(str)
  asm "  mov rax,1"
  asm "  mov rdi,1"
  asm "  mov rsi,", str
  asm "  mov rdx,", len
  asm "  syscall"

  retval 0
end
