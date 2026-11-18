use libe::io

struct Foo
  field: s64,
end

proc drop(self: &Foo)
  io::println("Foo dropped!")
end

proc main() -> s64
  let foo0 = Foo:
    field = 0,
  end

  let foo1 = copy(&foo0)

  retval foo0.field
end
