use mem

struct Vec\T/
  items: &T,
  count: u32,
  cap: u32,
end

proc vec\T/() -> Vec\T/
  retval Vec\T/:
    items = null,
    count = 0 as u32,
    cap = 0 as u32,
  end
end

proc drop\T/(self: &Vec\T/)
  if self.items != null
    mem::free(self.items as &u8)
  end
end

proc push\T/(self: &Vec\T/, element: T)
  if self.count >= self.cap
    if self.cap == 0 as u32
      self.cap = 16 as u32
    else
      self.cap = self.cap * 2 as u32
    end

    let new_items = mem::alloc(self.cap as u64 * sizeof T) as &T
    let i = 0 as u32
    while i < self.count
      new_items[i] = self.items[i]
      i = i + 1 as u32
    end
    if self.items != null
      mem::free(self.items as &u8)
    end
    self.items = new_items
  end

  self.items[self.count] = element
  self.count = self.count + 1 as u32
end
