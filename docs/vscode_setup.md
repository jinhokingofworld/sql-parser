# VS Code Setup

This repository builds as C11 with project headers in `include/` and some test-only
headers in `tests/support/`.

## Recommended workflow

Open the repository in the dev container so IntelliSense can use the same Linux
toolchain and system headers as the real build.

1. Install the `Dev Containers` extension in VS Code.
2. Run `Dev Containers: Reopen in Container`.
3. In the C/C++ configuration picker, select `Dev Container (Linux)`.

## Why the red include errors happen

If VS Code opens the repository directly on a Windows host, C/C++ IntelliSense may
not know about:

- `${workspaceFolder}/include`
- `${workspaceFolder}/tests/support`
- Linux system headers such as `dirent.h`
- the GCC toolchain used by `make`

The checked-in `.vscode` settings provide the project include paths, but POSIX
headers still require the dev container or another compatible Linux toolchain.
