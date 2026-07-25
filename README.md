# The E Programming Language

My own low-level, strictly statically typed language compiled to native code.

```e
use libe::io

proc main() -> s32
  println("Hello, World!")
  retval 1
end
```

For more examples, see [examples directory](./examples).

## Building

E uses my custom build system [nsb](https://github.com/oxxide216/nsb).
To build it, do the following:

```shell
cc -o nsb nsb.c
./nsb -a
```
