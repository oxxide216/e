use libe::io

proc main() -> s64
  while true
    break
  nobreak
    io::println("This should not happen")
  end

  let i = 0
  while i < 1
    i = i + 1
  nobreak
    io::println("This should happen")
  end

  retval 0
end
