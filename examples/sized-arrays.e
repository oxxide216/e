proc main() -> u8
  let array = [u8;; 8]

  let i = 0 as u8
  while i < (lenof array) as u8
    array[i] = i * 2 as u8

    i = i + 1 as u8
  end

  retval array[lenof array - 1 as u64]
end
