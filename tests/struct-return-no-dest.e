struct Foo
  a: s64,
  b: s64,
  c: s64,
  d: s64,
end

proc main() -> s64
  asm "mov rdi,0"
  foo()
  retval 0
end

proc foo() -> Foo
  retval Foo: end
end
