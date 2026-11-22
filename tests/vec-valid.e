use libe::vec

proc main() -> s64
  let element = 0
  let vec = vec::Vec\s64\:
    items = &element,
    count = 0 as u32,
    cap = 0 as u32,
  end

  retval 0
end
