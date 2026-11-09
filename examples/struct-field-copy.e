struct A
  x: s64,
  y: s64,
end

struct B
  foo: s64,
  field: A,
end

proc main() -> s64
  let b0 = B:
    foo = 0,
    field = A: 
      x = 2,
      y = 4,
    end,
  end
  let b1 = B:
    foo = 0,
    field = A: 
      x = 3,
      y = 7,
    end,
  end

  let arr = [A; b0.field, b1.field]
  arr[1] = arr[0]

  b0.field = b1.field

  retval b0.field.x + b1.field.y + arr[1].x + arr[1].y
end
