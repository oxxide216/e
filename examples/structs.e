struct IVec3
  x: s64,
  y: s64,
  z: s64
end

proc main() -> s64
  let vec = IVec3:
    x = 2,
    y = 5,
    z = 2,
  end

  let vec = ivec3_id(vec)
  ivec3_modify_x(&vec)

  retval vec.x
end

proc ivec3_id(vec: IVec3) -> IVec3
  retval vec
end

proc ivec3_modify_x(vec: &IVec3)
  vec.x = vec.x + vec.y + vec.z
end
