use unix -> PROT_READ, PROT_WRITE, MAP_PRIVATE, MAP_ANONYMOUS

proc alloc(size: u64) -> &u8
  retval unix::mmap(0 as u64, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 as u64) as &u8
end
