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

    EreBUS-Gate-api.exe --headless --source sum.c --node user@host --lo 1 --hi 1000000 --pieces 8 --combine sum

## Machine interface

`EreBUS-Gate-api.exe` is the same program built for a console: `api <verb>` speaks the node's
binary control protocol over ssh and prints one json object on stdout (an error object with
`"ok":false` and a non-zero exit on failure); `api watch` streams one json event per line. It
shares the saved nodes with the window, so `--node` also takes a saved node's name.

Use this one wherever the output is redirected or piped. `EreBUS-Gate.exe` is the window, and
Windows gives a window program no standard handles, so `EreBUS-Gate.exe api status > out.json`
writes nothing -- a program that spawns it with pipes of its own reads it either way.

    EreBUS-Gate-api.exe api schema
    EreBUS-Gate-api.exe api status  --node user@host --key C:\keys\id
    EreBUS-Gate-api.exe api jobs    --node user@host
    EreBUS-Gate-api.exe api submit  --source sum.c --node user@host --split 1..1000000 --pieces 8 --combine sum --wait 6
    EreBUS-Gate-api.exe api watch   --node user@host --for 60
    EreBUS-Gate-api.exe api log     --node user@host --lines 20
    EreBUS-Gate-api.exe api peers   --node user@host
    EreBUS-Gate-api.exe api cluster --node user@host
    EreBUS-Gate-api.exe api door    --grant id.pub

A split job is dealt to the machines a scan finds that welcome work, and a job without a split goes
to the node's `peer`; a node standing alone answers `no machine accepts work` or `no peer is named
in the settings`, and the refusal is written into its ledger. A node welcomes work only when its
settings say so (`work | welcomed`); the default is `refused`.

`submit` hands the package to the desk over the control channel and returns a handle; with `--wait`
it reads the folded result back in the same session. `watch` subscribes and streams a job event
(`queued`, `done`, `failed`) as the node pushes it -- no polling. `cluster` aggregates this node and
every peer it has heard, each with its live job count. On a node from before the control channel,
`status` and `submit` fall back to the line protocol (`"via":"line"`).
