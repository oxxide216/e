proc main() -> s64
  let array = [s64; 0, 1, 2, 3, 4]
  let i = 0 as u64
  while i < lenof array
    array[i] := array[i] + 1
    i = i + 1 as u64
  end
  retval array[2]
end
