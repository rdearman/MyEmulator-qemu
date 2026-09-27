# Inside the REM Computer

## Edition 2 --- Workbook Revision

# Part III --- Terminal Programming

# Chapter 5 --- The Terminal Model

A terminal is not a screen.

This is the first concept a programmer coming from ncurses must
understand.

A terminal application does not draw pixels. It sends bytes.

The terminal emulator receives those bytes and converts them into a
visual display.

The path is:

    Application

        |

        v

    stdout / stdin

        |

        v

    Linux tty layer

        |

        v

    terminal emulator

        |

        v

    pixels

The application controls a stream of characters and control sequences.

It does not directly control the display hardware.

------------------------------------------------------------------------

# 5.1 What ncurses normally hides

A curses programmer normally thinks in terms of:

    move cursor

    print character

    read key

    refresh screen

The library hides several layers:

-   cursor addressing;
-   screen buffering;
-   terminal capabilities;
-   keyboard escape sequences;
-   terminal modes.

REMEDIT must build the pieces it needs itself.

------------------------------------------------------------------------

# 5.2 REM console and Linux terminal layers

The REM hardware console is a byte-oriented device.

The hardware does not understand:

-   lines;
-   prompts;
-   ANSI colours;
-   cursor movement;
-   command editing.

Those belong to higher layers.

The model is:

    REM console device

            |

            v

    Linux kernel console driver

            |

            v

    tty subsystem

            |

            v

    application

For a normal Linux application:

    application

            |

            v

    tty device

            |

            v

    terminal emulator

The terminal emulator interprets ANSI escape sequences.

------------------------------------------------------------------------

# 5.3 ANSI control sequences

A terminal understands escape sequences.

Example:

Clear screen:

    ESC [ 2 J

Move cursor:

    ESC [ row ; column H

The application sends these bytes.

The terminal performs the action.

------------------------------------------------------------------------

# 5.4 Terminal capabilities

Not every terminal behaves identically.

Differences include:

-   supported colours;
-   function key sequences;
-   cursor behaviour;
-   special control sequences.

Large applications use capability databases.

A first REM editor should support a known subset and avoid unnecessary
complexity.

------------------------------------------------------------------------

# Chapter 6 --- Terminal Modes and termios

The shell and a text editor need different input behaviour.

The kernel terminal driver controls this behaviour.

Applications configure it through termios.

------------------------------------------------------------------------

# 6.1 Canonical mode

Canonical mode is line-oriented.

Example:

User types:

    hello

The application receives the input after Enter.

The terminal driver performs:

-   line buffering;
-   erase handling;
-   editing behaviour.

This is suitable for shells.

------------------------------------------------------------------------

# 6.2 Raw editor input

An editor requires character-level input.

The typical sequence is:

    tcgetattr()

            |

            v

    copy current settings

            |

            v

    modify flags

            |

            v

    tcsetattr()

            |

            v

    editor runs

            |

            v

    restore original settings

Never leave the terminal modified.

------------------------------------------------------------------------

# 6.3 Important termios flags

The editor must understand the settings it changes.

## ICANON

Canonical input mode.

Enabled:

    line input

Disabled:

    character input

------------------------------------------------------------------------

## ECHO

Controls local echo.

Enabled:

    typed characters appear automatically

Disabled:

    application controls display

Editors normally disable echo.

------------------------------------------------------------------------

## ISIG

Controls signal-generating characters.

Examples:

    Ctrl-C

    Ctrl-Z

    Ctrl-\

The editor must decide whether it handles these itself or allows the
kernel to generate signals.

------------------------------------------------------------------------

## IXON

Software flow control.

This controls characters such as:

    Ctrl-S

    Ctrl-Q

Whether this is relevant depends on the deployment environment.

------------------------------------------------------------------------

## VMIN and VTIME

These control read behaviour in noncanonical mode.

Important:

A key press does not necessarily equal one read() returning one byte.

The result depends on:

-   buffering;
-   VMIN;
-   VTIME;
-   escape sequence timing.

------------------------------------------------------------------------

# 6.4 Terminal recovery

A crashed editor can leave the terminal unusable.

The editor must:

1.  save original settings;
2.  restore settings on normal exit;
3.  restore settings on error paths.

Development versions should provide a manual recovery command:

    reset

------------------------------------------------------------------------

# Chapter 7 --- Keyboard Decoding

The terminal does not send keys.

It sends bytes.

The keyboard layer converts byte streams into editor events.

------------------------------------------------------------------------

# 7.1 Characters versus keys

A normal character:

    a

is one byte.

An arrow key is a sequence.

Example:

    ESC [ A

The decoder converts this to:

    KEY_UP

------------------------------------------------------------------------

# 7.2 Escape sequences are variable length

The decoder cannot assume:

    ESC = error

or:

    ESC = complete key

ESC may be:

-   part of an arrow key;
-   part of a function key;
-   an actual Escape key press.

The decoder needs a policy.

------------------------------------------------------------------------

# 7.3 Keyboard state machine

Example:

    NORMAL

     |

     | ESC

     v

    ESCAPE

     |

     | [

     v

    CSI

     |

     v

    KEY IDENTIFIER

Possible results:

    CHARACTER

    KEY_UP

    KEY_DOWN

    KEY_LEFT

    KEY_RIGHT

    KEY_DELETE

    KEY_HOME

    KEY_END

------------------------------------------------------------------------

# 7.4 Decoder timing

Because escape sequences arrive as bytes, the decoder must handle
incomplete input.

Example:

Received:

    ESC

Possible meanings:

    Escape key

or:

    beginning of arrow sequence

The implementation must use:

-   read timing;
-   buffering;
-   state.

------------------------------------------------------------------------

# 7.5 Deterministic keyboard testing

Do not test the editor first.

Test the decoder.

Example:

Input byte stream:

    1B 5B 41

Expected:

    KEY_UP

Test cases:

    normal character

    escape key

    arrow keys

    delete

    home

    end

    function keys

The keyboard decoder should work without a screen.

------------------------------------------------------------------------

# Chapter milestones

## terminal-test

Purpose:

Verify terminal output.

Expected:

-   writes bytes;
-   moves cursor;
-   clears screen.

------------------------------------------------------------------------

## termios-test

Purpose:

Verify terminal control.

Expected:

-   enters noncanonical mode;
-   disables echo;
-   reads characters;
-   restores original state.

------------------------------------------------------------------------

## keyboard-test

Purpose:

Verify input decoding.

Expected:

-   captured byte sequences become editor events.

Status:

    Host QEMU:

    test required


    Fold:

    validate after deployment
