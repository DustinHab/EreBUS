# The program interface

A program for the machine is C in the shape its compiler reads (MANUAL.md 10.3),
entered as `long main(long console, long inbox)` and holding nothing but those
two capabilities until something is given to it. `erebus.h` is the calling
convention written down once: the eight system calls, the rights, the message
layout, and a few helpers. MANUAL.md chapter 18 is the reference; the header
follows it and nothing else.

`hello.c` is the smallest program that uses it. Two ways to run it:

- On the machine: send both files in through the door (or type them), lay them
  in one list, stand on the list, `compile hello.c`, then `run hello.c code`.
  What it says lands in the journal under its name. `give <a text> to hello.c
  code` within its first second and it says the text's length (it says the
  length of everything it is given, its own start gift included).
- On the host, to see that it builds: `make cchost && build/cchost sdk/hello.c
  sdk/erebus.h` writes `sdk/hello.c.asm` and `sdk/hello.c.img` beside it. The
  image runs only on the machine.

`tools/sdktest.sh` does the first, through the door, and checks what the
program said. Nothing in this directory is part of the kernel: it is the
outside looking in, kept here so the interface cannot drift without a test
noticing.
