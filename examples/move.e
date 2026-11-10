struct Foo
  field: s64
end

proc main() -> s64
  let a = Foo: field = 0 end
  let b = a

  retval a.field
end
