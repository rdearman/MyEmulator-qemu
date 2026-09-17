#!/usr/bin/env python3
import importlib.machinery
import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("mydebug")
LOADER = importlib.machinery.SourceFileLoader("mydebug", str(SCRIPT))
SPEC = importlib.util.spec_from_loader("mydebug", LOADER)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DebugMapPathTests(unittest.TestCase):
    def test_current_map_is_relative_to_debug_map_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            project = root / "rikmon"; project.mkdir()
            source = project / "rikmon.s"; source.write_text("halt\n")
            metadata_file = project / "rikmon.debug.json"
            metadata_file.write_text(json.dumps({"source_path_base": "debug-map-directory"}))
            resolved = MODULE.resolve_source_path("rikmon.s", metadata_file, json.loads(metadata_file.read_text()))
            self.assertEqual(resolved, source.resolve())

    def test_legacy_nested_path_falls_back_to_invocation_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            project = root / "rikmon"; project.mkdir()
            source = project / "rikmon.s"; source.write_text("halt\n")
            metadata_file = project / "rikmon.debug.json"
            old_cwd = Path.cwd()
            try:
                os.chdir(root)
                resolved = MODULE.resolve_source_path("rikmon/rikmon.s", metadata_file, {})
            finally:
                os.chdir(old_cwd)
            self.assertEqual(resolved, source.resolve())

    def test_included_source_resolves_from_map_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            project = root / "rikmon"; project.mkdir()
            include = project / "include"; include.mkdir()
            source = include / "myemulator.inc"; source.write_text(".equ X, 1\n")
            metadata_file = project / "rikmon.debug.json"
            resolved = MODULE.resolve_source_path("include/myemulator.inc", metadata_file,
                                                 {"source_path_base": "debug-map-directory"})
            self.assertEqual(resolved, source.resolve())


if __name__ == "__main__":
    unittest.main()
