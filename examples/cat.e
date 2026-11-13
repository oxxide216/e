use libe::io

proc main(argc: s32, argv: &&u8) -> s64
  if argc < 2 as s32
    io::println("[ERROR] Not enough arguments")
    retval 1
  end

  let i = 1
  while i < argc as s64
    let content = io::read_file(argv[i])
    if content == null as &u8
      io::println("[ERROR] Could not read input file")
      retval 1
    end

    io::print(content)
    i = i + 1
  end

  retval 0
end
