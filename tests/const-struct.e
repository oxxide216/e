struct Foo
  a: s64,
  b: s64,
  c: s64,
  d: s64,
end

const FOO = Foo:
  a = 0,
  b = 1,
  c = 2,
  d = 3,
end

proc main() -> s64
  retval FOO.a + FOO.b + FOO.c + FOO.d
end
