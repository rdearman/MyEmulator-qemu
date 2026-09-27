# linegrep

Search text files or standard input for a fixed substring. It supports line
numbers and ASCII case-insensitive matching, searches long lines without
truncating them, and uses grep-style exit statuses: 0 found, 1 no match, 2
input/output error. Matching is literal; this is useful for logs when regular
expressions are unnecessary.

## Build

Use the matching native REM compiler driver in the flight image. From this
directory:

```sh
./build.sh
./linegrep -n ERROR samples/flight.log
```

The direct build is Make-independent and runs `chibicc -o linegrep linegrep.c`.
When Samurai is accepted into the selected image, the equivalent Ninja graph
is `build.ninja` and can be run with `samu`.

On an x86/ARM Linux host, use `CC=cc ./build.sh` to build with the host C
compiler instead.

For a hosted-C link, chibicc, native `as`/`ld`, CRT objects, musl, and the
matching `libgcc.a` must come from one validated runtime tuple. See the pack
README and `docs/REM_VALIDATED_RUNTIME_TUPLES.md`.

## Expected output

```text
samples/flight.log:3:Sep 27 08:02 ERROR open failed: /dev/ttyMY0
```

Try `./linegrep -i -n disk samples/flight.log` for a case-insensitive search.
Without file arguments it reads standard input. The sample data is synthetic.
