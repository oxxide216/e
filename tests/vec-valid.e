use libe::vec

proc main() -> s64
  let vec = vec::vec\s64/()
  vec::push\s64/(&vec, 13)
  retval vec.items[0]
end
