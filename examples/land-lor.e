use libe::io

proc should() -> bool
  io::println("This should happen")
  retval true
end

proc should_not() -> bool
  io::println("This should not happen")
  retval false
end

proc main() -> s64
  if true && should()
    io::println("true")
  else
    io::println("false")
  end

  if false && should_not()
    io::println("true")
  else
    io::println("false")
  end

  if true || should_not()
    io::println("true")
  else
    io::println("false")
  end

  if false || should()
    io::println("true")
  else
    io::println("false")
  end

  retval 0
end
