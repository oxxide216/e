use libe::unix

proc main(argc: s32, argv: &&u8) -> s64
  let fs = FileStat: end
  let result = stat(argv[1], &fs)
  if result < 0
    retval 0 - result
  end
  retval fs.st_size
end
