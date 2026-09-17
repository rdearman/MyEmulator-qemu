#!/usr/bin/env python3
import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("format_asm.py")
SPEC = importlib.util.spec_from_file_location("format_asm", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class FormatAsmTests(unittest.TestCase):
    def test_preserves_myemulator_comments_and_formats_source(self):
        source = '''; header
.equ CONSOLE_DATA, 0xF010
reset: li r0,#'R' ; load a character
    .section .text
1: br 1b
.asciz "a;b"
'''
        self.assertEqual(MODULE.format_source(source), '''; header
.equ CONSOLE_DATA, 0xF010
reset:
\tli r0,#'R'    ; load a character
.section .text
1:
\tbr 1b
.asciz "a;b"
''')

    def test_round_trip_output_assembles(self):
        source = '''.org 0
start: li r0,#1
       add r0,r0,#2 ; arithmetic
       halt
'''
        formatted = MODULE.format_source(source)
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            input_file = directory / "input.s"
            output_file = directory / "output.s"
            binary = directory / "output.bin"
            input_file.write_text(formatted)
            MODULE.format_file(input_file, output_file)
            self.assertEqual(output_file.read_text(), formatted)
            assembler = Path(__file__).with_name("myasm")
            # Exercise the public wrapper without depending on the current cwd.
            import subprocess
            subprocess.run([str(assembler), str(output_file), "-o", str(binary)], check=True)
            self.assertEqual(binary.read_bytes(), bytes((1, 0x10, 2, 0x30, 0x80, 0xF0)))


if __name__ == "__main__":
    unittest.main()
