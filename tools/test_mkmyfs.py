import runpy
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
mkmyfs = type("MkMyFS", (), runpy.run_path(str(ROOT / "tools/mkmyfs")))

class MyFSTests(unittest.TestCase):
    def test_image_layout_and_contiguous_files(self):
        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            boot = td / "boot"; boot.write_bytes(b"boot")
            one = td / "one"; one.write_bytes(b"abc")
            two = td / "two"; two.write_bytes(bytes(range(256)) + b"x")
            image = td / "disk.img"
            self.assertEqual(mkmyfs.main(["--boot", str(boot), "--output", str(image),
                                          "--sectors", "8", "--file", f"ONE.TXT={one}",
                                          "--file", f"TWO.BIN={two}"]), 0)
            data = image.read_bytes()
            self.assertEqual(len(data), 8 * 256)
            self.assertEqual(data[:4], b"boot")
            self.assertEqual(data[256:260], b"MYFS")
            self.assertEqual(data[262:264], b"\x02\x00")
            self.assertEqual(data[768:771], b"abc")
            self.assertEqual(data[1024:1280], bytes(range(256)))
            self.assertEqual(data[1280], ord("x"))
            directory = data[512:768]
            self.assertEqual(directory[0:11], b"ONE     TXT")
            self.assertEqual(int.from_bytes(directory[12:14], "little"), 3)
            self.assertEqual(int.from_bytes(directory[14:16], "little"), 3)
            self.assertEqual(int.from_bytes(directory[28:30], "little"), 4)
            self.assertEqual(int.from_bytes(directory[30:32], "little"), 257)

    def test_nested_input_preserves_command_bytes_in_image(self):
        """The raw executable must begin at its recorded sector offset."""
        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            source = td / "project" / "myfs"
            source.mkdir(parents=True)
            boot = source / "boot.bin"
            command = source / "COMMAND.COM"
            boot.write_bytes(b"boot")
            command_bytes = bytes(range(1, 33))
            command.write_bytes(command_bytes)
            image = td / "build" / "myemulator.img"
            image.parent.mkdir()

            self.assertEqual(mkmyfs.main([
                "--boot", str(boot), "--output", str(image), "--sectors", "8",
                "--file", f"COMMAND.COM={command}"]), 0)

            data = image.read_bytes()
            entry = data[512:528]
            start = int.from_bytes(entry[12:14], "little")
            size = int.from_bytes(entry[14:16], "little")
            self.assertEqual(size, len(command_bytes))
            self.assertEqual(data[start * 256:start * 256 + size], command_bytes)

    def test_rejects_invalid_inputs(self):
        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            boot = td / "boot"; boot.write_bytes(b"x" * 257)
            file = td / "file"; file.write_bytes(b"x")
            with self.assertRaises(ValueError):
                mkmyfs.build(type("Args", (), {"boot": str(boot), "sectors": 8,
                    "file": [f"BAD.NAME.TOO.LONG={file}"], "output": str(td / "x")})())
            boot.write_bytes(b"x")
            with self.assertRaises(ValueError):
                mkmyfs.build(type("Args", (), {"boot": str(boot), "sectors": 8,
                    "file": [f"A.TXT={file}", f"a.txt={file}"], "output": str(td / "x")})())

if __name__ == "__main__":
    unittest.main()
