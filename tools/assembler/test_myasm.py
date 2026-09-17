import tempfile, unittest
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).parent))
from myasm import Assembler, AsmError, main

def assemble(src):
    return Assembler("test.asm", src).assemble()

class AssemblerTests(unittest.TestCase):
    def test_firmware_image_is_compact_and_absolute(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source = directory / "firmware.s"
            output = directory / "firmware.bin"
            debug = directory / "firmware.debug.json"
            source.write_text(""".org 0xF100
reset:
    li r0,#7
    halt
.org 0xFFFC
.word 0xF000
.word reset
""")
            self.assertEqual(main([str(source), "-o", str(output), "--firmware",
                                   "--debug-map", str(debug)]), 0)
            image = output.read_bytes()
            self.assertEqual(len(image), 0x0f00)
            self.assertEqual(image[:4], bytes([0x07, 0x10, 0x80, 0xf0]))
            self.assertEqual(image[-4:], bytes([0x00, 0xf0, 0x00, 0xf1]))
            self.assertEqual(image[4:-4], b'\xff' * (0x0f00 - 8))
            metadata = __import__('json').loads(debug.read_text())
            self.assertEqual(metadata['image_type'], 'firmware')
            self.assertEqual(metadata['symbols']['reset'], 0xf100)
            self.assertEqual(metadata['locations'][0]['address'], 0xf100)

    def test_firmware_rejects_data_outside_rom_window(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source = directory / "bad.s"
            output = directory / "bad.bin"
            source.write_text(".org 0xF000\n.byte 1\n")
            self.assertEqual(main([str(source), "-o", str(output), "--firmware"]), 1)

    def test_all_confirmed_instruction_families(self):
        a=assemble('''
li r0,#0xff
ld r1,[a2-128]
st r3,[a1+127]
add r0,r1,#2
sub r2,r3,#1
and r0,r1,#0xff
or r2,r3,#0x80
xor r1,r0,#0xfc
shl r3,r2,#8
shr r0,r3,#0
add r0,r1
sub r2,r3
and r0,r1
or r2,r3
xor r1,r0
shl r3,r2
shr r0,r3
cmp r0,r2
jal here
beq here
bne here
blt here
bge here
bltu here
bgeu here
br here
lda a0,r0,r1
gta r2,r3,a0
mva lr,sp
ada a3,#-5
gf r2
sf r3
push {r0,r1,r2,r3,lr}
pop {lr,r0}
ret
rti
halt
here:
''')
        self.assertEqual(len(a.bytes), 37*2)
        self.assertEqual(a.bytes[0], 0xff)
        self.assertEqual(a.bytes[37*2-2], 0x80)

    def test_literals_constants_expressions_and_strings(self):
        a=assemble("""COUNT = 2\nX = 0x4300 + 0x72\n.org 0x20\n.byte 42, 0x2A, 0b0010_1010, 0o52, 'A'\n.word X\n.ascii \"a\\n\\\";b\"\n.asciz \"z\"\n""")
        self.assertEqual(bytes(a.bytes[i] for i in range(0x20,0x20+14)), bytes([42,42,42,42,65,0x72,0x43,97,10,34,59,98,122,0]))

    def test_labels_and_address_pseudo(self):
        a=assemble('''la a0,#message,r2,r3\nmessage:\n.byte 1''')
        self.assertEqual(bytes(a.bytes[i] for i in range(7)), bytes([0,0x18,6,0x1c,0x2c,0x71,1]))

    def test_range_and_alignment_errors(self):
        for src, text in [('li r0,#256','unsigned 8-bit'), ('ld r0,[a0+129]','signed 8-bit'), ('br far\n.org 0x200\nfar: halt','out of range'), ('br odd\n.org 3\nodd: halt','aligned')]:
            with self.subTest(text=text):
                with self.assertRaises(AsmError) as e: assemble(src)
                self.assertIn(text,e.exception.message)

    def test_branch_limits_and_all_conditions(self):
        for mnemonic in ('br','beq','bne','blt','bge','bltu','bgeu','jal'):
            with self.subTest(mnemonic=mnemonic):
                self.assertEqual(len(assemble(f'{mnemonic} near\nnear: halt').bytes),4)
                self.assertEqual(assemble(f'{mnemonic} far\n.org 0x100\nfar: halt').bytes.get(0), 0x7f if mnemonic != 'jal' else 0x7f)
        self.assertEqual(assemble('.org 0x100\nbr back\n.org 0x2\nback: halt').bytes[0x100], 0x80)

    def test_register_variants_and_mva_set(self):
        for r in ('r0','r1','r2','r3'):
            self.assertEqual(len(assemble(f'gf {r}\nsf {r}\nli {r},#1').bytes),6)
        for d in ('a0','a1','a2','a3','lr','sp'):
            for s in ('a0','a1','a2','a3','lr','sp'):
                assemble(f'mva {d},{s}')

    def test_word_labels_and_overlap(self):
        a=assemble('.word handler\nhandler: halt')
        self.assertEqual(bytes(a.bytes[i] for i in range(2)), b'\x02\x00')
        with self.assertRaisesRegex(AsmError,'overlapping output'):
            assemble('.org 0x10\n.byte 1\n.org 0x10\n.byte 2')

    def test_alu_encodings_and_malformed_operands(self):
        for op, base, selector in (
            ('add', 0x3000, 0), ('sub', 0x4000, 1), ('and', 0x9000, 2),
            ('or', 0xa000, 3), ('xor', 0xb000, 4), ('shl', 0xc000, 5),
            ('shr', 0xd000, 6)):
            immediate = bytes(assemble(f'{op} r0,r1,#0xff').bytes[i] for i in range(2))
            self.assertEqual(immediate, bytes((0xff, (base | 0x0100) >> 8)))
            register = bytes(assemble(f'{op} r2,r3').bytes[i] for i in range(2))
            word = 0x7700 | (selector << 4) | (2 << 2) | 3
            self.assertEqual(register, bytes((word & 0xff, word >> 8)))
        for op in ('add','and','or','xor','shl','shr'):
            with self.assertRaisesRegex(AsmError,'expects 3 operands|valid data register'):
                assemble(f'{op} r0,#1')

    def test_all_immediate_values_are_representable(self):
        for op in ('add', 'sub', 'and', 'or', 'xor', 'shl', 'shr'):
            for value in range(256):
                with self.subTest(op=op, value=value):
                    self.assertEqual(len(assemble(
                        f'{op} r0,r1,#{value}').bytes), 2)
        with self.assertRaisesRegex(AsmError,'valid address register'): assemble('ld r0,[a4]')
        with self.assertRaisesRegex(AsmError,'MVA operands'): assemble('mva pc,a0')
        with self.assertRaisesRegex(AsmError,'valid PUSH/POP'): assemble('push {s0}')

    def test_diagnostics_and_masks(self):
        with self.assertRaisesRegex(AsmError,'duplicate label'): assemble('x: halt\nx: halt')
        with self.assertRaisesRegex(AsmError,'undefined symbol'): assemble('br nowhere')
        with self.assertRaisesRegex(AsmError,'duplicate register'): assemble('push {r0,r0}')
        self.assertEqual(assemble('push {r0,r2,lr}').bytes[0],0x15)
        self.assertEqual(assemble('pop {r0,r2,lr}').bytes[1],0xf0)

if __name__ == '__main__': unittest.main()
