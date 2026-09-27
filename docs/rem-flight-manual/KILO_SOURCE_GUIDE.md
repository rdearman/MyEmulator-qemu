# Kilo Source Guide

The accompanying `kilo-source/kilo.c` is the canonical antirez Kilo 0.0.1 source snapshot. Its source header grants permission to redistribute the source with its copyright and BSD-style license notice intact.

## Source structure

Kilo is intentionally a single C translation unit. The main areas are:

- editor state and syntax definitions;
- terminal and raw-mode helpers;
- input decoding;
- row/buffer management;
- screen rendering;
- file I/O;
- search and command handling;
- `main()` initialization and event loop.

## Startup

`main()` initializes the editor state, reads the optional filename, enables raw terminal mode, obtains terminal dimensions with `TIOCGWINSZ`, loads the file if supplied, and enters the editor loop. REM's validated terminal reports 24 by 80 columns, allowing normal startup without a cursor-position fallback.

## Raw mode

Kilo obtains the current `termios` structure, disables canonical input, echo, and signal-generated line processing, and registers restoration with `atexit()`. This makes individual key presses available to the editor. Terminal restoration is essential when exiting or receiving an error.

## Input loop

The input routine reads bytes from standard input. Printable bytes become insertion operations. Escape sequences are decoded into arrow, page, home/end, and delete keys. Control keys dispatch commands such as save, quit, and search. Reads are deliberately small so a key can be acted on immediately.

## Screen drawing

The refresh path builds an output buffer containing ANSI cursor movement, clear-screen operations, visible rows, a status bar, and the message bar. It writes the buffer to standard output in one or a few operations, then positions the cursor at the logical editor location. Scrolling offsets map the logical cursor into the visible window.

## Buffers and rows

Each file line is represented by an editor row containing the original characters and a rendered form. Tab expansion and syntax highlighting operate on the rendered representation. Insertion, deletion, splitting, and joining update row lengths and the modified flag.

## File loading and saving

Loading reads the file line by line, strips line endings, and creates rows. Saving serializes rows with newline separators into a contiguous buffer, opens or creates the target file, truncates it, writes the bytes, and reports errors in the status bar. The modified flag is cleared only after a successful save.

## Search

Search walks rows and rendered text, updates the cursor and scroll position to the match, and supports repeated search movement through the input callback. The search prompt temporarily reads a line while preserving the editor state.

## Event loop

The central loop redraws the screen, reads one key, and dispatches it. This simple loop is the whole editor scheduler: terminal input drives state changes, and every state change is followed by a redraw.

## Safe modifications

Build and test a changed copy outside the flight image. Keep raw-mode restoration, bounds checks, allocation failure handling, and file-save error paths intact. Kilo is small enough that a change should be tested with open, insert, navigation, save, quit, reopen, and terminal restoration.
