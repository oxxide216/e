proc main() -> s64
  retval get_x(10)
end

proc get_x(n: s64) -> s64
  let x = 0

  while x < n
    x = x + 1
  end

  retval x
end
