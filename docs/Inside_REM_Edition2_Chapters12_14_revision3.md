# Inside the REM Computer

## Edition 2 — Workbook Revision

# Part V — Making REMEDIT a Development Environment

# Chapter 12 — File Handling (Revised)

The editor has two different representations of text:

```
Filesystem

    bytes

      |

      v

Loader

      |

      v

Document model

      |

      v

Editor

      |

      v

Saver

      |

      v

Filesystem
```

The filesystem does not know about:

- lines;
- cursor position;
- undo;
- modified state.

The editor must keep those concepts separate.

---

# 12.1 File descriptors and the filesystem boundary

The editor communicates with files through the operating system interface.

The basic operations are:

```
open

read

write

close
```

The file descriptor is the handle returned by opening a file.

The editor should not scatter file operations throughout the code.

Use a file module:

```
file.S

    file_open()

    file_read()

    file_write()

    file_close()
```

---

# 12.2 Loading a file

Loading converts bytes into the document model.

Sequence:

```
open filename

        |

        v

read bytes

        |

        v

normalise line endings

        |

        v

create Line objects

        |

        v

create Document
```

The loader must define:

- empty files;
- missing files;
- permissions;
- read failures.

Example errors:

```
ENOENT

file does not exist


EACCES

permission denied


EIO

I/O failure
```

---

# 12.3 Line endings

Files may contain different line endings:

```
LF

CRLF

CR
```

The document model uses one internal representation.

Conversion happens during load and save.

The editor should not allow line-ending differences to leak into editing operations.

---

# 12.4 Loading strategy

The first REMEDIT version can load the complete file into memory.

Advantages:

- simple;
- easy to debug;
- predictable.

Large-file strategies can come later:

- memory mapping;
- paged loading;
- lazy buffers.

Do not optimise before measuring.

---

# 12.5 Modified state

The editor tracks whether the document differs from disk.

Example:

```
modified = false

after typing:

modified = true
```

Saving clears the modified state.

Closing a modified document requires:

```
save

discard

cancel
```

---

# 12.6 Saving safely

A save operation should protect against partial writes.

The sequence:

```
create temporary file

        |

        v

write all data

        |

        v

check write results

        |

        v

close file

        |

        v

rename temporary file
```

The rename is the commit point.

This protects against many interrupted-write failures.

It should not be described as complete power-loss durability unless flushing and storage guarantees are verified.

---

# 12.7 Temporary file ownership

The editor must define:

- temporary filename;
- cleanup after failure;
- ownership of temporary storage.

A failed save must not leave confusing recovery files.

---

# 12.8 Recovery

Future versions may add:

- recovery journals;
- autosave files;
- crash recovery.

The first version must at least:

- report errors;
- preserve the in-memory document;
- never silently discard changes.

---

# Chapter 13 — Search and Replace (Revised)

Search operates on the document model.

The terminal only displays the result.

---

# 13.1 Search state

A search operation stores:

```
search text

direction

case mode

current match position
```

This allows:

```
Find Next
```

to continue from the previous result.

---

# 13.2 Search direction

Support:

```
forward search

backward search
```

A forward search begins at the current cursor position.

A backward search begins before the current cursor position.

---

# 13.3 Wrapping

The editor must define whether searches wrap.

REMEDIT v1:

```
search reaches end

    |

    v

optionally continue from beginning
```

The behaviour must be consistent and visible to the user.

---

# 13.4 Case handling

Search modes:

```
case sensitive

case insensitive
```

The comparison routine must match the selected mode.

---

# 13.5 Replace

Replacement is an edit command.

Example:

Before:

```
add r1,r2,r3
```

Replace:

```
r3 -> r4
```

After:

```
add r1,r2,r4
```

The command stores enough information for undo.

---

# 13.6 Replace all

Replace-all is a compound command.

One user operation:

```
replace all
```

creates:

```
one undo record
```

A new edit after undo clears redo history.

Large replacements require a memory policy:

- maximum undo size;
- command compression;
- refusal when limits are exceeded.

---

# Chapter 14 — REM Assembly Syntax Highlighting (Revised)

Syntax highlighting is a lexer problem.

It is not an assembler.

The lexer identifies text categories.

The assembler validates instructions.

---

# 14.1 Token types

Initial token set:

```
opcode

register

number

identifier

label

directive

string

comment
```

Example:

```asm
msg:
    .ascii "hello"
    add r1,r2,r3   # total
```

Tokens:

```
label

directive

string

opcode

registers

comment
```

---

# 14.2 Follow REM GAS syntax

The lexer must match the actual assembler environment.

It must understand:

- REM register syntax;
- GAS directives;
- GAS comments;
- numeric formats.

The editor must not invent a different assembly language.

---

# 14.3 Lexer state machine

Example:

```
START

 |

 +-- identifier

 |        |

 |        +-- opcode/register/label

 |

 +-- digit

 |        |

 |        +-- number

 |

 +-- quote

          |

          +-- string
```

---

# 14.4 Highlighting architecture

Keep tokenisation separate from rendering.

```
Source

   |

   v

Lexer

   |

   v

Tokens

   |

   v

Renderer

   |

   v

Terminal colours
```

---

# Tests

## file-test

Input:

```
two line file
```

Expected:

```
document line_count = 2

line contents identical
```

---

## search-test

Input:

```
replace one occurrence
```

Expected:

```
new text present

undo restores original bytes
```

---

## lexer-test

Input:

```asm
add r1,r2,r3 # total
```

Expected:

```
opcode

register

register

register

comment
```

Status:

```
Host QEMU:

test required


Fold:

validate separately
```
