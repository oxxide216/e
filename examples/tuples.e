proc main() -> s64
  let tuple = (68, 70)
  tuple.0 = tuple.0 + tuple.1
  let (x, y) = tuple

  retval x
end
