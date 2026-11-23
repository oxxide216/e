use libe::vec

proc main() -> s64
  let vec = vec::vec\s64/()
  vec::push\u64/(&vec, 13 as u64)

  retval 0
end
