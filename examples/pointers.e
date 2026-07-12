proc main() -> s64
  let a = 5
  inc(&a)
  retval a
end

proc inc(ptr: &s64)
  ptr := *ptr + 1
end
