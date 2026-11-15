use libe::io

proc main() -> s64
  while true
    break
  end

  let i = 0
  while i < 1
    i = i + 1
    continue

    io::println("This should not happen")
  end

  retval 0
end
