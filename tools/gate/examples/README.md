# EreBUS Gate examples

Two forms of the same distributed task -- sum every number in a range:

- `sum.c` -- a compiled far task (c for the far-task ABI). The node compiles it and runs the image.
- `sum.recipe` -- the same in the little interpreter language; check **recipe** in Gate, no compiler needed on the node.

## Run in EreBUS Gate

1. **source**: browse to (or drag in) `sum.c`. For `sum.recipe`, check **recipe**.
2. Check **split**, set **from** 1, **to** 1000000, **pieces** 8, **combine** sum.
3. Name a **node** (`user@host` or an ssh alias); its door must hold your ssh key. Press **test** to check it.
4. Press **send**. Gate feeds the package to the node's desk, which deals the pieces to the willing machines; the folded result (500000500000) is read back into **output**.

A payload rides one datagram (at most 1 KiB), and no foreign binaries run -- the payload is EreBUS c or a recipe.

## Command line

The same without the window:

    EreBUS-Gate.exe --headless --source sum.c --node user@host --split 1 1000000 --pieces 8 --combine sum
