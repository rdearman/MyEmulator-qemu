#!/usr/bin/env python3
"""Structural validation for the MyEmulator 2.0 design manifest."""
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class MyEmulator2SpecificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with (ROOT / "docs" / "myemulator2-encoding.json").open(
                encoding="utf-8") as stream:
            cls.spec = json.load(stream)

    def test_word_and_format_widths(self):
        self.assertEqual(self.spec["instruction_bits"], 32)
        self.assertEqual(self.spec["primary_opcode_bits"], 6)
        self.assertEqual(len(self.spec["primary_opcodes"]), 13)

    def test_primary_opcodes_are_unique_and_six_bit(self):
        values = list(self.spec["primary_opcodes"].values())
        self.assertEqual(len(values), len(set(values)))
        self.assertTrue(all(0 <= value < 64 for value in values))

    def test_subopcode_families_are_unique(self):
        for name in ("r_operations", "alu_immediate_subops", "load_sizes",
                     "store_sizes", "branch_conditions", "system_operations",
                     "system_registers"):
            values = list(self.spec[name].values())
            self.assertEqual(len(values), len(set(values)), name)

    def test_vectors_are_unique_and_in_range(self):
        vectors = self.spec["vectors"]
        values = [value for name, value in vectors.items()
                  if name not in ("reserved_start", "reserved_end")]
        self.assertEqual(len(values), len(set(values)))
        self.assertTrue(all(0 <= value < 256 for value in values))
        self.assertEqual([vectors[f"irq{n}"] for n in range(1, 8)],
                         list(range(16, 23)))
        self.assertEqual(vectors["nmi"], 14)
        self.assertEqual(vectors["double_fault"], 15)
        self.assertEqual(vectors["reserved_start"], 23)
        self.assertEqual(vectors["reserved_end"], 255)

    def test_vector_table_does_not_overlap_reset_words(self):
        vectors = self.spec["vectors"]
        reset = self.spec["reset"]
        self.assertEqual(vectors["illegal_instruction"] * 4, 0x00000000)
        self.assertEqual(vectors["privilege_violation"] * 4, 0x00000004)
        self.assertEqual(reset["initial_ssp_address"], 0x00000400)
        self.assertEqual(reset["initial_pc_address"], 0x00000404)
        vector_addresses = {number * 4 for number in vectors.values()}
        self.assertNotIn(reset["initial_ssp_address"], vector_addresses)
        self.assertNotIn(reset["initial_pc_address"], vector_addresses)
        self.assertNotEqual(reset["initial_ssp_address"],
                            reset["initial_pc_address"])

    def test_required_operations_are_present(self):
        self.assertEqual(set(self.spec["r_operations"]), {
            "ADD", "ADC", "SUB", "SBC", "MUL", "MULH", "MULHU",
            "DIV", "DIVU", "REM", "REMU", "AND", "OR", "XOR", "NOT",
            "SLL", "SRL", "SRA", "ROL", "ROR", "SEQ", "SNE", "SLT",
            "SGE", "SLTU", "SGEU",
        })
        self.assertEqual(set(self.spec["alu_immediate_subops"]), {
            "ADDI", "SUBI", "ANDI", "ORI", "XORI", "SLLI", "SRLI", "SRAI",
        })
        self.assertEqual(self.spec["primary_opcodes"]["CAS"], 12)
        self.assertEqual(self.spec["formats"]["CAS"],
                         "op[31:26], rd[25:21], rs[20:16], ra[15:11], reserved[10:0]=0")
        self.assertEqual(self.spec["system_registers"]["DFSP"], 11)


if __name__ == "__main__":
    unittest.main()
