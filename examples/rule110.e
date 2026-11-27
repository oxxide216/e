# This same thing in C: https://gist.github.com/rexim/c595009436f87ca076e7c4a2fb92ce10

use libe::mem
use libe::io

const N = 80
const TABLE = [u8; 0, 1, 1, 1, 0, 1, 1, 0]

proc main() -> s64
  let world = [u8;; 80]
  mem::set(&world[0], 0 as u8, N as u64)
  world[N - 2] = 1 as u8

  display(&world[0])

  let i = 0
  while i < N - 2
    step(&world[0])
    display(&world[0])

    i = i + 1
  end

  retval 0
end

proc display(world: &u8)
  let i = 0
  while i < N
    if world[i] == 0 as u8
      io::print("  ")
    else
      io::print("* ")
    end

    i = i + 1
  end
  io::print("\n")
end

proc step(world: &u8)
  let mask = world[0] << 1 as u8 | world[1]
  let i = 1
  while i + 1 < N
    mask = (mask << 1 as u8 | world[i + 1]) & 7 as u8
    world[i] = TABLE[mask]

    i = i + 1
  end
end
