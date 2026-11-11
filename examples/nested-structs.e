use libe::io

struct A
  b: B
end

struct B
  field: s64
end

proc drop(self: &B)
  println("B drop!")
end

proc drop(self: &A)
  println("A drop!")
end

proc main() -> s64
  let a = A:
    b = B:
      field = 13,
    end,
  end

  let ref = &a.b
  ref.field = 17

  retval a.b.field
end
