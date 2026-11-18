use libe::io

proc main() -> s64
  io::println("I am recursive!")
  retval main()
end
