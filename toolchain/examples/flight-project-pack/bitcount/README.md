# bitcount

Count bytes and set bits in a file. This is useful for checking generated
binary blobs, masks, and compressed-data fixtures. The application reads
large files incrementally and calls a small `bitcount32(unsigned int)`
routine for each byte.

## Build

The REM build demonstrates C calling target assembly:

```sh
./build.sh rem
./bitcount-rem samples/two-bytes.bin
```

It uses `chibicc`, native `as`, and the configured native linker. With
accepted Samurai, `samu -f build.ninja bitcount-rem` builds the same target.
`bitcount_rem.S` follows the REM register ABI and is intentionally isolated
behind a plain C function declaration.

For x86, ARM, or other Linux hosts, use the portable C implementation:

```sh
./build.sh portable
./bitcount-portable samples/two-bytes.bin
```

## Expected output

The sample contains the bytes `0x41 0x0a` (`A` and newline):

```text
bytes=2 set-bits=4
```

The REM assembly and portable C implementations should produce identical
results for all inputs. Extend the sample to compare both implementations
before replacing the portable routine with target-specific assembly on a new
architecture.
