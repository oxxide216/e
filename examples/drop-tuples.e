use libe::io

struct Foo
  field: s64,
end

proc drop(self: &Foo)
  println("Foo drop!")
end

proc main() -> s64
  let foo = (
    Foo: field = 0 end,
    (  
      Foo: field = 1 end,
      Foo: field = 2 end,
    ),
  )

  retval 0
end
