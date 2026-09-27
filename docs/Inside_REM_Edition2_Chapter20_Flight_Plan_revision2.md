# Inside the REM Computer

## Edition 2 — Workbook Revision

# Part VII — Flight Projects

# Chapter 20 — The Two Flight Plan (Revised)

The purpose of this chapter is to turn the previous chapters into a practical implementation sequence.

The objective is not to build the complete REM development environment in one step.

The objective is to build small, verified components until REMEDIT becomes useful.

Each milestone should leave behind:

- working code;
- tests;
- documented interfaces;
- a known status.

---

# 20.1 Development rules

## Keep the project organised

Maintain:

```
src/

tests/

build/

docs/
```

The editor, libraries, tests, and documentation should evolve together.

---

## Never leave the system broken

A development session should finish with one of:

- working code;
- a failing test that clearly identifies the problem;
- a documented incomplete feature.

Do not continue building on an unknown broken state.

---

# 20.2 Project status labels

Each project should identify its current state:

```
Host QEMU verified

    tested in REM emulator environment


Guest Linux target

    intended to run inside REM Linux


Fold deployment

    requires phone validation


Planned

    design exists but implementation is incomplete
```

---

# 20.3 The outbound flight

The outbound flight builds the minimum usable editor.

Dependency order:

```
terminal

    |

    v

keyboard

    |

    v

buffer

    |

    v

editor core
```

---

# 20.4 Project 1 — Terminal library

Build:

```
terminal.S
```

Responsibilities:

```
terminal_init()

terminal_restore()

terminal_clear()

terminal_move()

terminal_write()
```

Tests:

```
terminal-test
```

Success criteria:

- output bytes reach the terminal;
- cursor control works;
- terminal state can be restored.

Status:

```
Guest Linux target

Fold validation later
```

---

# 20.5 Project 2 — Keyboard decoder

Build:

```
keyboard.S
```

Responsibilities:

- read input bytes;
- decode escape sequences;
- generate editor events.

Tests:

```
keyboard-test
```

Success criteria:

Input:

```
ESC [ A
```

produces:

```
KEY_UP
```

Status:

```
Guest Linux target
```

---

# 20.6 Project 3 — Document buffer

Build:

```
buffer.S
```

Responsibilities:

- line storage;
- insert;
- delete;
- split;
- join.

Tests:

```
buffer-test
```

Success criteria:

The buffer correctly modifies documents without requiring a terminal.

Status:

```
Host QEMU verified first
```

---

# 20.7 Project 4 — Editor core

Combine:

```
terminal

keyboard

buffer
```

The editor loop:

```
read key

    |

    v

execute command

    |

    v

modify document

    |

    v

render
```

Tests:

```
editor-core-test
```

Success criteria:

The user can:

- move;
- insert text;
- delete text;
- maintain document state.

Status:

```
Guest Linux target
```

---

# 20.8 The return flight

The return flight adds development productivity.

Dependency order:

```
syntax highlighting

    |

    v

build integration

    |

    v

REM tools
```

---

# 20.9 Project 5 — Syntax highlighting

Add:

- lexer;
- token stream;
- renderer integration.

Tests:

```
lexer-test
```

Success criteria:

REM assembly displays:

- opcodes;
- registers;
- labels;
- directives;
- comments.

Status:

```
Guest Linux target
```

---

# 20.10 Project 6 — Build integration

Add:

```
save

    |

    v

run build

    |

    v

collect result

    |

    v

display errors
```

Tests:

```
process-test

build-capture-test

error-parser-test
```

Minimum success criteria:

- document saves before building;
- build command executes;
- exit status is preserved;
- diagnostics are captured;
- selecting an error moves the cursor to the source location.

A clickable interface is a later improvement.

Status:

```
Guest Linux target

depends on verified process interfaces
```

---

# 20.11 Project 7 — REM tools

Integrate:

```
disassembler

debugger interfaces

monitor tools
```

Priority:

```
disassembler

then

debugger support

then

monitoring
```

Reason:

The REM disassembler is supported by existing toolchain components.

Debugger and monitor features require clearly defined interfaces.

Tests:

```
disassembler-test

debugger-test

monitor-test
```

Status:

```
Disassembler:

implementable now


Debugger:

host/QEMU or future interface


Monitor:

planned until data interfaces exist
```

---

# 20.12 Flight checklist

## Outbound flight

```
[ ] terminal

    terminal-test


[ ] keyboard

    keyboard-test


[ ] buffer

    buffer-test


[ ] editor core

    editor-core-test
```

---

## Return flight

```
[ ] syntax highlighting

    lexer-test


[ ] build integration

    process-test
    build-capture-test
    error-parser-test


[ ] REM tooling

    disassembler-test
    debugger-test
    monitor-test
```

---

# Final milestone

The goal is a native REM development environment:

```
REM CPU

    |

    v

REM Linux

    |

    v

REM toolchain

    |

    v

REMEDIT

    |

    v

native REM application development
```

The target is a system capable of building and running useful REM software.

