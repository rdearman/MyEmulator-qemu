# Inside the REM Computer

## Edition 2 --- Workbook Revision

# Part IV --- Building REMEDIT

# Chapter 8 --- Editor Architecture

A text editor is not a terminal program that happens to display text.

The terminal is a view layer.

The editor is a document model plus commands that transform that model.

The fundamental separation:

    Document model

            |

            v

    Editor logic

            |

            v

    Renderer

            |

            v

    Terminal

The document must remain correct even when the display is temporarily
incorrect.

------------------------------------------------------------------------

# 8.1 The editor loop

The editor loop is:

    read input

        |

        v

    interpret command

        |

        v

    modify document

        |

        v

    update logical state

        |

        v

    render display

Do not mix document changes with terminal output.

A rendering problem should not corrupt the document.

------------------------------------------------------------------------

# 8.2 Model versus display

The terminal cursor is not the editor cursor.

The editor tracks a logical position:

    document line

    document column

The renderer converts this into:

    screen row

    screen column

Example:

    Document:

    1000 lines


    Terminal:

    40 visible lines

The editor needs:

-   complete document;
-   logical cursor;
-   viewport position.

------------------------------------------------------------------------

# 8.3 Editor state

A first REMEDIT version requires explicit state:

    EditorState

        document

        cursor_x

        cursor_y

        preferred_x

        scroll_x

        scroll_y

        modified flag

        mode

The state must be testable without a terminal.

------------------------------------------------------------------------

# Chapter 9 --- Text Storage

The first REMEDIT version uses a simple line-based document model.

The goal is:

-   easy debugging;
-   predictable ownership;
-   simple testing.

------------------------------------------------------------------------

# 9.1 Document representation

REMEDIT v1 uses:

    Document

        line_count

        lines[]

The document always contains at least one line.

An empty file is:

    line_count = 1

    line[0] = empty string

This avoids special cases during editing.

------------------------------------------------------------------------

# 9.2 Line representation

A line contains:

    Line

        buffer address

        length

        capacity

Example:

    buffer -> "hello"

    length = 5

    capacity = 16

Each line owns its character buffer.

The owner is responsible for:

-   allocation;
-   resizing;
-   freeing.

------------------------------------------------------------------------

# 9.3 Buffer growth

Do not allocate one byte at a time.

Growth:

    existing buffer

        |

        v

    allocate larger buffer

        |

        v

    copy contents

        |

        v

    replace buffer

The growth algorithm can change later.

The interface should remain stable.

------------------------------------------------------------------------

# 9.4 Newline ownership

Lines do not store newline characters.

The document structure represents line boundaries.

Example:

    Line 0: hello

    Line 1: world

Saving converts the structure into:

    hello\nworld\n

------------------------------------------------------------------------

# 9.5 Future storage models

REMEDIT v1 deliberately does not start with advanced structures.

Possible future designs:

## Gap buffer

A movable gap improves insertion near the cursor.

Advantages:

-   good typing performance;
-   relatively simple.

## Piece table

Stores references to original and modified text.

Advantages:

-   excellent undo support;
-   efficient for large files.

## Rope

A tree structure representing text.

Advantages:

-   large-file performance.

The first version favours correctness over optimisation.

------------------------------------------------------------------------

# Chapter 10 --- Editing Operations

Editing operations modify the document model.

The terminal is not involved.

Every operation should have:

-   inputs;
-   output state;
-   boundary rules;
-   tests.

------------------------------------------------------------------------

# 10.1 Insert character

Input:

    line

    column

    character

Before:

    ABC

Insert X:

    ABXC

Result:

    cursor moves one position right

------------------------------------------------------------------------

# 10.2 Backspace

Rules:

If:

    column > 0

remove the previous character.

If:

    column == 0

join with the previous line.

Example:

Before:

    ABC
    DEF

Backspace at start of second line:

    ABCDEF

------------------------------------------------------------------------

# 10.3 Delete

Rules:

If a character exists under the cursor:

    remove it

If cursor is at end of line:

    join with next line

Example:

Before:

    ABC
    DEF

Delete at end of first line:

    ABCDEF

------------------------------------------------------------------------

# 10.4 Split line

Enter creates a new line.

Before:

    ABCDEF

Cursor after C:

    ABC
    DEF

------------------------------------------------------------------------

# 10.5 Join lines

Joining reverses splitting.

Before:

    ABC
    DEF

After:

    ABCDEF

------------------------------------------------------------------------

# 10.6 Cursor movement and viewport

Cursor movement changes logical position.

Rendering decides visibility.

The editor maintains:

    cursor position

    preferred column

    horizontal scroll

    vertical scroll

Scrolling is a view operation, not a document operation.

------------------------------------------------------------------------

# Chapter 11 --- Undo and Redo

Undo is part of the editor architecture.

A command stores enough information to reverse an operation.

------------------------------------------------------------------------

# 11.1 Command structure

Example:

    Command

        type

        position

        old data

        new data

The command owns any stored text required for reversal.

------------------------------------------------------------------------

# 11.2 Edit lifecycle

    User action

        |

        v

    Create command

        |

        v

    Apply command

        |

        v

    Push undo history

        |

        v

    Clear redo history

A new edit invalidates redo history.

------------------------------------------------------------------------

# 11.3 Undo lifecycle

    Undo request

        |

        v

    Remove command from undo stack

        |

        v

    Reverse command

        |

        v

    Push onto redo stack

Undoing must not create a new undo record.

------------------------------------------------------------------------

# 11.4 Redo lifecycle

    Redo request

        |

        v

    Remove command from redo stack

        |

        v

    Apply command again

        |

        v

    Push onto undo stack

------------------------------------------------------------------------

# 11.5 History policy

The editor must define:

-   maximum number of commands;
-   maximum memory usage;
-   cleanup rules.

Compound commands are possible:

Example:

    Replace All

may become one undo operation containing many changes.

------------------------------------------------------------------------

# Tests

## buffer-test

Input:

    empty document

    insert A

Expected:

    Line 0 = A

    length = 1

------------------------------------------------------------------------

## editor-core-test

Input:

    type HELLO

    move left

    backspace

Expected:

    HELO

------------------------------------------------------------------------

## undo-test

Input:

    insert A

    insert B

    undo

    undo

Expected:

    empty document

Redo twice:

    AB

------------------------------------------------------------------------

# Status

Host QEMU:

    test with current REM environment

Fold:

    validate separately
