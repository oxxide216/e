struct A
  b: B
end

struct B
  field: s64
end

proc main() -> s64
  let a = A:
    b = B:
      field = 13,
    end,
  end

  a.b.field = 17

  retval a.b.field
end
