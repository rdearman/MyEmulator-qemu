#!/usr/bin/env python3
"""Two-pass assembler for the MyEmulator ISA.

The source language is flat (there is no object format or linker), but is
deliberately GNU-as inspired: constants, includes, local labels, expressions,
sections and data directives are available while .org controls placement.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

REGS = {f"r{i}": i for i in range(4)}
AREGS = {f"a{i}": i for i in range(4)}
MVA_REGS = {**AREGS, "lr": 4, "sp": 5}
BRANCHES = {"beq": 0, "bne": 1, "blt": 2, "bge": 3,
            "bltu": 4, "bgeu": 5, "br": 6}
ALU_OPS = {"add": 0, "sub": 1, "and": 2, "or": 3, "xor": 4,
           "shl": 5, "shr": 6}
TOKEN = re.compile(
    r"\s*(0[xX][0-9a-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|"
    r"[0-9][0-9_]*[fb]|[0-9][0-9_]*|"
    r"'(?:\\(?:x[0-9a-fA-F]{2}|.)|[^'\\])'|"
    r"[A-Za-z_.$][\w.$]*|<<|>>|[()+\-*/%&|^~])")


class AsmError(Exception):
    def __init__(self, message, line=None, filename=None):
        self.message, self.line, self.filename = message, line, filename
        super().__init__(message)


def no_comment(s):
    quoted = esc = False
    for i, c in enumerate(s):
        if quoted:
            if esc: esc = False
            elif c == "\\": esc = True
            elif c == '"': quoted = False
        elif c == '"': quoted = True
        elif c == ';': return s[:i]
    return s


def split_args(s):
    out, start, depth, quoted, esc = [], 0, 0, False, False
    for i, c in enumerate(s):
        if quoted:
            if esc: esc = False
            elif c == "\\": esc = True
            elif c == '"': quoted = False
        elif c == '"': quoted = True
        elif c in '[{(': depth += 1
        elif c in ']})': depth -= 1
        elif c == ',' and depth == 0:
            out.append(s[start:i].strip()); start = i + 1
    tail = s[start:].strip()
    if tail or out: out.append(tail)
    return out


def parse_string(s, line):
    if len(s) < 2 or s[0] != '"' or s[-1] != '"':
        raise AsmError("expected quoted string", line)
    escapes = {'n': 10, 'r': 13, 't': 9, '\\': 92, '"': 34, '0': 0}
    out, i = bytearray(), 1
    while i < len(s) - 1:
        if s[i] != '\\': out.extend(s[i].encode('utf-8')); i += 1; continue
        i += 1
        if i >= len(s) - 1: raise AsmError("unterminated string escape", line)
        c = s[i]
        if c == 'x':
            digits = s[i + 1:i + 3]
            if len(digits) != 2 or not re.fullmatch(r'[0-9a-fA-F]{2}', digits):
                raise AsmError("\\x escape requires exactly two hex digits", line)
            out.append(int(digits, 16)); i += 3; continue
        if c not in escapes: raise AsmError(f"unsupported string escape \\{c}", line)
        out.append(escapes[c]); i += 1
    return bytes(out)


class Expr:
    PREC = {'|': 1, '^': 2, '&': 3, '<<': 4, '>>': 4,
            '+': 5, '-': 5, '*': 6, '/': 6, '%': 6}

    def __init__(self, text, line):
        self.tokens, pos = [], 0
        while pos < len(text):
            match = TOKEN.match(text, pos)
            if not match: raise AsmError(f"malformed expression near '{text[pos:]}'", line)
            self.tokens.append(match.group(1)); pos = match.end()
        self.i, self.line = 0, line
        self.tree = self.expression(0)
        if self.i != len(self.tokens): raise AsmError("malformed expression", line)

    def peek(self): return self.tokens[self.i] if self.i < len(self.tokens) else None
    def take(self):
        token = self.peek()
        if token is not None: self.i += 1
        return token

    def expression(self, minimum):
        node = self.unary()
        while self.peek() in self.PREC and self.PREC[self.peek()] >= minimum:
            op = self.take(); node = (op, node, self.expression(self.PREC[op] + 1))
        return node

    def unary(self):
        token = self.take()
        if token is None: raise AsmError("missing expression", self.line)
        if token in ('+', '-', '~'): return ('u', token, self.unary())
        if token == '(':
            node = self.expression(0)
            if self.take() != ')': raise AsmError("missing ')' in expression", self.line)
            return node
        if self.peek() == '(' and token.lower() in ('hi', 'lo'):
            self.take(); node = self.expression(0)
            if self.take() != ')': raise AsmError("missing ')' in function", self.line)
            return (token.lower(), node)
        if token.startswith("'"):
            body = token[1:-1]
            if body.startswith('\\'):
                code = body[1:]
                if code.startswith('x') and len(code) == 3: return ('num', int(code[1:], 16))
                chars = {'n': 10, 'r': 13, 't': 9, '\\': 92, "'": 39, '0': 0}
                if code not in chars: raise AsmError("invalid character literal", self.line)
                return ('num', chars[code])
            if len(body) != 1: raise AsmError("character literal must contain one character", self.line)
            return ('num', ord(body))
        if token[0].isdigit():
            if re.fullmatch(r'[0-9][0-9_]*[fb]', token):
                return ('local', int(token[:-1].replace('_', '')), token[-1])
            try: return ('num', int(token.replace('_', ''), 0))
            except ValueError: raise AsmError(f"invalid number '{token}'", self.line)
        return ('sym', token.lower())


def trunc_div(a, b):
    if b == 0: raise ValueError("division by zero")
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


def eval_tree(tree, symbols, local_labels=None, current=0, stack=()):
    kind = tree[0]
    if kind == 'num': return tree[1]
    if kind == 'sym':
        name = tree[1]
        if name not in symbols: raise KeyError(name)
        val = symbols[name]
        if isinstance(val, tuple):
            if name in stack: raise ValueError("cyclic constant definition")
            return eval_tree(val, symbols, local_labels, current, stack + (name,))
        return val
    if kind == 'local':
        labels = (local_labels or {}).get(tree[1], [])
        candidates = [x for x in labels if (x > current if tree[2] == 'f' else x < current)]
        if not candidates: raise KeyError(f"{tree[1]}{tree[2]}")
        return min(candidates) if tree[2] == 'f' else max(candidates)
    if kind == 'u':
        val = eval_tree(tree[2], symbols, local_labels, current, stack)
        return {'+': val, '-': -val, '~': ~val}[tree[1]]
    if kind in ('hi', 'lo'):
        val = eval_tree(tree[1], symbols, local_labels, current, stack)
        return (val >> 8) & 0xff if kind == 'hi' else val & 0xff
    left = eval_tree(tree[1], symbols, local_labels, current, stack)
    right = eval_tree(tree[2], symbols, local_labels, current, stack)
    if kind == '+': return left + right
    if kind == '-': return left - right
    if kind == '*': return left * right
    if kind == '/': return trunc_div(left, right)
    if kind == '%': return left - trunc_div(left, right) * right
    if kind == '<<':
        if right < 0: raise ValueError("negative shift count")
        return left << right
    if kind == '>>':
        if right < 0: raise ValueError("negative shift count")
        return left >> right
    if kind == '&': return left & right
    if kind == '|': return left | right
    if kind == '^': return left ^ right
    raise ValueError(f"unknown expression operator {kind}")


def freeze_dot(tree, address):
    if tree[0] == 'sym' and tree[1] == '.': return ('num', address)
    if tree[0] in ('num', 'sym', 'local'): return tree
    if tree[0] == 'u': return ('u', tree[1], freeze_dot(tree[2], address))
    if tree[0] in ('hi', 'lo'): return (tree[0], freeze_dot(tree[1], address))
    return (tree[0], freeze_dot(tree[1], address), freeze_dot(tree[2], address))


def replace_symbol(tree, name, replacement):
    """Capture the old value for the GAS idiom `.set X, X + 1`."""
    if tree[0] == 'sym' and tree[1] == name:
        return replacement
    if tree[0] in ('num', 'sym', 'local'):
        return tree
    if tree[0] == 'u':
        return ('u', tree[1], replace_symbol(tree[2], name, replacement))
    if tree[0] in ('hi', 'lo'):
        return (tree[0], replace_symbol(tree[1], name, replacement))
    return (tree[0], replace_symbol(tree[1], name, replacement),
            replace_symbol(tree[2], name, replacement))


def value(text, symbols, line, local_labels=None, current=0):
    try: return eval_tree(Expr(text, line).tree, symbols, local_labels, current)
    except KeyError as exc: raise AsmError(f"undefined symbol '{exc.args[0]}'", line)
    except (ValueError, ZeroDivisionError) as exc: raise AsmError(str(exc), line)


@dataclass
class Source:
    filename: str
    line: int
    text: str
    address: int | None = None
    data: bytes = b''


class Assembler:
    def __init__(self, filename, text, include_dirs=None):
        self.filename = str(filename)
        self.include_dirs = [Path(x) for x in (include_dirs or [])]
        self.symbols, self.symbol_kinds = {}, {}
        self.exports, self.locals, self.local_labels = set(), set(), {}
        self.sources = self.expand_includes(Path(filename), text, ())
        self.bytes, self.loc = {}, 0

    def err(self, msg, n, filename=None): raise AsmError(msg, n, filename or self.filename)

    def expand_includes(self, filename, text, stack):
        canonical = filename.resolve()
        if canonical in stack: raise AsmError("recursive include detected", None, str(filename))
        result, next_stack = [], stack + (canonical,)
        for number, raw in enumerate(text.splitlines(), 1):
            clean = no_comment(raw).strip()
            match = re.fullmatch(r'\.include\s+"([^"]+)"', clean, re.I)
            if not match:
                result.append(Source(str(filename), number, raw)); continue
            name = Path(match.group(1))
            candidates = [filename.parent / name] + [directory / name for directory in self.include_dirs]
            selected = next((candidate for candidate in candidates if candidate.is_file()), None)
            if selected is None:
                raise AsmError(f"include file not found '{match.group(1)}'", number, str(filename))
            try: included = selected.read_text()
            except OSError as exc: raise AsmError(str(exc), number, str(filename))
            result.extend(self.expand_includes(selected, included, next_stack))
        return result

    def reg(self, token, n):
        key = token.strip().lower()
        if key not in REGS: self.err(f"{token} is not a valid data register", n)
        return REGS[key]

    def areg(self, token, n):
        key = token.strip().lower()
        if key not in AREGS: self.err(f"{token} is not a valid address register", n)
        return AREGS[key]

    def resolve(self, text, n, current=None):
        if current is None: current = getattr(self, 'current', self.loc)
        symbols = dict(self.symbols); symbols['.'] = current
        return value(text, symbols, n, self.local_labels, current)

    def imm(self, token, n, bits, signed=False):
        token = token.strip()
        if token.startswith('#'): token = token[1:].strip()
        val = self.resolve(token, n)
        low, high = ((-(1 << (bits - 1)), (1 << (bits - 1)) - 1) if signed else (0, (1 << bits) - 1))
        if not low <= val <= high:
            self.err(f"immediate {val} does not fit {'signed' if signed else 'unsigned'} {bits}-bit operand", n)
        return val & ((1 << bits) - 1)

    def labels(self, text, n, define=True):
        while True:
            local = re.match(r'^([0-9]+):', text)
            if local:
                if define:
                    self.local_labels.setdefault(int(local.group(1)), []).append(self.loc)
                text = text[local.end():].strip(); continue
            label = re.match(r'^([A-Za-z_.$][\w.$]*):', text)
            if not label: break
            name = label.group(1).lower()
            if define:
                if name in self.symbols: self.err(f"duplicate label '{label.group(1)}'", n)
                self.symbols[name], self.symbol_kinds[name] = self.loc, 'label'
            text = text[label.end():].strip()
        return text

    def define(self, name, tree, kind, n):
        name = name.lower()
        if name == '.': self.err("cannot define the current-location symbol '.'", n)
        if kind == 'equ' and name in self.symbols: self.err(f"duplicate .equ symbol '{name}'", n)
        if kind == 'set' and name in self.symbols and self.symbol_kinds.get(name) != 'set':
            self.err(f"cannot redefine non-.set symbol '{name}'", n)
        if kind == 'set' and name in self.symbols:
            tree = replace_symbol(tree, name, self.symbols[name])
        self.symbols[name], self.symbol_kinds[name] = freeze_dot(tree, self.loc), kind

    def directive_parts(self, text, n, define=True):
        parts = text.split(None, 1); op = parts[0].lower()
        args = split_args(parts[1]) if len(parts) > 1 else []
        if op in ('.equ', '.set'):
            if len(args) != 2: self.err(f"{op} requires NAME, expression", n)
            if not re.fullmatch(r'[A-Za-z_.$][\w.$]*', args[0].strip()): self.err("invalid constant name", n)
            if define:
                self.define(args[0].strip(), Expr(args[1], n).tree, 'equ' if op == '.equ' else 'set', n)
        return op, args

    def handle_visibility(self, op, args, n):
        if op == '.section':
            if len(args) != 1: self.err(".section requires a name", n)
            return
        if len(args) != 1: self.err(f"{op} requires one symbol", n)
        name = args[0].strip().lower()
        if not re.fullmatch(r'[A-Za-z_.$][\w.$]*', name): self.err("invalid symbol name", n)
        (self.exports if op in ('.global', '.globl') else self.locals).add(name)

    def pass1(self):
        self.loc = 0
        for src in self.sources:
            text = self.labels(no_comment(src.text).strip(), src.line)
            if not text: continue
            old = re.match(r'^([A-Za-z_.$][\w.$]*)\s*=\s*(.+)$', text)
            if old:
                self.define(old.group(1), Expr(old.group(2), src.line).tree, 'set', src.line); continue
            op, args = self.directive_parts(text, src.line)
            if op in ('.equ', '.set'): continue
            if op == '.org':
                if len(args) != 1: self.err(".org requires one expression", src.line)
                self.loc = self.resolve(args[0], src.line); self.check_loc(src.line); src.address = self.loc; continue
            if op in ('.section', '.text', '.data', '.rodata', '.bss', '.global', '.globl', '.local'):
                self.handle_visibility(op, args, src.line); continue
            src.address = self.loc; self.loc += self.size(op, args, src.line); self.check_loc(src.line)
        for name in self.exports | self.locals:
            if name not in self.symbols: self.err(f"declared symbol '{name}' was not defined", 0)

    def check_loc(self, n):
        if not 0 <= self.loc <= 0x10000: self.err("address is outside 16-bit memory", n)

    def align_size(self, args, n, location=None):
        if len(args) != 1: self.err(".align requires one boundary", n)
        boundary = self.resolve(args[0], n)
        if boundary <= 0: self.err("alignment must be positive", n)
        location = getattr(self, 'current', self.loc) if location is None else location
        return (boundary - location % boundary) % boundary

    def size(self, op, args, n):
        if op == '.byte': return len(args)
        if op == '.word': return 2 * len(args)
        if op in ('.ascii', '.asciz'):
            if len(args) != 1: self.err(f"{op} takes one string", n)
            return len(parse_string(args[0], n)) + (op == '.asciz')
        if op == '.space':
            if not 1 <= len(args) <= 2: self.err(".space requires count[, fill]", n)
            count = self.resolve(args[0], n)
            if count < 0: self.err(".space count cannot be negative", n)
            if len(args) == 2 and not 0 <= self.resolve(args[1], n) <= 255: self.err(".space fill must be 0..255", n)
            return count
        if op == '.fill':
            if len(args) != 3: self.err(".fill requires count, size, value", n)
            count, width, val = self.resolve(args[0], n), self.resolve(args[1], n), self.resolve(args[2], n)
            if count < 0 or width <= 0: self.err("invalid .fill count or size", n)
            if not 0 <= val < (1 << (8 * width)): self.err(".fill value does not fit its size", n)
            return count * width
        if op in ('.align', '.balign'): return self.align_size(args, n)
        if op == 'la':
            if len(args) != 4: self.err("la requires address, value, high scratch, low scratch", n)
            return 6
        return 2

    def encode(self, op, args, n):
        def need(count):
            if len(args) != count: self.err(f"{op.upper()} expects {count} operands", n)
        if op == '.byte':
            result = []
            for arg in args:
                val = self.resolve(arg, n)
                if not 0 <= val <= 255: self.err(f"byte value {val} is outside 0..255", n)
                result.append(val)
            return bytes(result)
        if op == '.word':
            out = bytearray()
            for arg in args:
                val = self.resolve(arg, n)
                if not 0 <= val <= 0xffff: self.err(f"value {val} does not fit unsigned 16-bit operand", n)
                out += bytes((val & 255, val >> 8))
            return bytes(out)
        if op in ('.ascii', '.asciz'):
            need(1); data = parse_string(args[0], n); return data + (b'\0' if op == '.asciz' else b'')
        if op == '.space':
            count = self.resolve(args[0], n); fill = self.resolve(args[1], n) if len(args) == 2 else 0
            return bytes([fill]) * count
        if op == '.fill':
            count, width, val = (self.resolve(x, n) for x in args)
            return val.to_bytes(width, 'little') * count
        if op in ('.align', '.balign'): return bytes(self.align_size(args, n))
        if op == 'la':
            need(4); dst = args[0]
            if dst.lower() not in AREGS: self.err("la destination must be A0-A3", n)
            if not args[1].strip().startswith('#'): self.err("la value must be an immediate or symbol prefixed with #", n)
            high = self.encode('li', [args[2], '#hi(' + args[1].strip()[1:] + ')'], n)
            low = self.encode('li', [args[3], '#lo(' + args[1].strip()[1:] + ')'], n)
            return high + low + self.encode('lda', [dst, args[2], args[3]], n)
        if op == 'li': need(2); return w(0x1000 | (self.reg(args[0], n) << 10) | self.imm(args[1], n, 8))
        if op in ALU_OPS:
            if len(args) == 2: return w(0x7700 | (ALU_OPS[op] << 4) | (self.reg(args[0], n) << 2) | self.reg(args[1], n))
            need(3); base = {"add": 0x3000, "sub": 0x4000, "and": 0x9000, "or": 0xa000, "xor": 0xb000, "shl": 0xc000, "shr": 0xd000}[op]
            return w(base | (self.reg(args[0], n) << 10) | (self.reg(args[1], n) << 8) | self.imm(args[2], n, 8))
        if op in ('ld', 'st'):
            need(2); rd = self.reg(args[0], n)
            match = re.match(r'^\[\s*([^+\]-]+)\s*(?:([+-])\s*(.+))?\s*\]$', args[1])
            if not match: self.err("memory operand must be [An] or [An+/-displacement]", n)
            an = self.areg(match.group(1), n); disp = 0 if not match.group(3) else self.imm(('-' if match.group(2) == '-' else '') + match.group(3), n, 8, True)
            return w((0 if op == 'ld' else 0x2000) | (rd << 10) | (an << 8) | disp)
        if op == 'cmp': need(2); return w(0x8000 | (self.reg(args[0], n) << 10) | (self.reg(args[1], n) << 8))
        if op == 'jal': need(1); return w(0x5000 | self.branch_disp(args[0], n))
        if op in BRANCHES: need(1); return w(0x6000 | (BRANCHES[op] << 8) | self.branch_disp(args[0], n))
        if op in ('lda', 'gta'):
            need(3); an = self.areg(args[0] if op == 'lda' else args[2], n); rx = self.reg(args[1] if op == 'lda' else args[0], n); ry = self.reg(args[2] if op == 'lda' else args[1], n)
            return w((0x7100 if op == 'lda' else 0x7200) | (an << 6) | (rx << 4) | (ry << 2))
        if op == 'ada': need(2); return w(0x7000 | (self.areg(args[0], n) << 10) | self.imm(args[1], n, 8, True))
        if op in ('gf', 'sf'): need(1); return w(0x7600 | (self.reg(args[0], n) << 2) | (op == 'sf'))
        if op == 'mva':
            need(2); dst, src = args[0].lower(), args[1].lower()
            if dst not in MVA_REGS or src not in MVA_REGS: self.err("MVA operands must be A0-A3, LR, or SP", n)
            return w(0x7300 | (MVA_REGS[dst] << 3) | MVA_REGS[src])
        if op in ('push', 'pop'):
            need(1); text = args[0].strip()
            if not (text.startswith('{') and text.endswith('}')): self.err("PUSH/POP requires a register list in braces", n)
            mask = 0
            for token in (x.strip().lower() for x in text[1:-1].split(',') if x.strip()):
                if token not in REGS and token != 'lr': self.err(f"{token} is not a valid PUSH/POP register", n)
                bit = 4 if token == 'lr' else REGS[token]
                if mask & (1 << bit): self.err(f"duplicate register '{token}'", n)
                mask |= 1 << bit
            return w((0xe000 if op == 'push' else 0xf000) | mask)
        fixed = {'ret': 0xf040, 'rti': 0xf060, 'halt': 0xf080}
        if op in fixed: need(0); return w(fixed[op])
        self.err(f"unknown instruction or directive '{op}'", n)

    def branch_disp(self, token, n):
        target = self.resolve(token, n)
        if target & 1: self.err("branch target is not instruction-aligned", n)
        displacement = (target - (self.current + 2)) // 2
        if not -128 <= displacement <= 127: self.err(f"branch target '{token}' is out of range", n)
        return displacement & 255

    def pass2(self):
        self.current, self.bytes = 0, {}
        for src in self.sources:
            text = self.labels(no_comment(src.text).strip(), src.line, define=False)
            if not text: continue
            if re.match(r'^[A-Za-z_.$][\w.$]*\s*=\s*.+$', text): continue
            op, args = self.directive_parts(text, src.line, define=False)
            if op in ('.equ', '.set'): continue
            if op == '.org': self.current = self.resolve(args[0], src.line); continue
            if op in ('.section', '.text', '.data', '.rodata', '.bss', '.global', '.globl', '.local'): continue
            src.address = self.current; data = self.encode(op, args, src.line); src.data = data
            for offset, byte in enumerate(data):
                address = self.current + offset
                if address in self.bytes: self.err(f"overlapping output at address 0x{address:04x}", src.line)
                self.bytes[address] = byte
            self.current += len(data)

    def assemble(self): self.pass1(); self.pass2(); return self


def w(value_): return bytes((value_ & 255, (value_ >> 8) & 255))


def resolved_symbols(assembler):
    result = {}
    for name, tree in assembler.symbols.items():
        if name == '.': continue
        try: result[name] = eval_tree(tree, assembler.symbols, assembler.local_labels, 0) if isinstance(tree, tuple) else tree
        except (KeyError, ValueError): pass
    return result


def write_debug_map(path, assembler, source, image_type="ram"):
    locations = []
    for src in assembler.sources:
        if src.address is None or not src.data: continue
        for offset in range(0, len(src.data), 2):
            locations.append({"address": src.address + offset, "source": src.filename,
                              "line": src.line, "text": src.text.rstrip()})
    symbols = resolved_symbols(assembler)
    symbol_info = {name: {"kind": assembler.symbol_kinds.get(name, "label"),
                          "global": name in assembler.exports, "local": name in assembler.locals}
                   for name in symbols}
    data = {"format": "myemulator-debug-v1", "source": str(source), "image_type": image_type,
            "symbols": symbols, "symbol_info": symbol_info, "locations": locations}
    Path(path).write_text(json.dumps(data, indent=2) + "\n")


def main(argv=None):
    parser = argparse.ArgumentParser(prog='myasm')
    parser.add_argument('source'); parser.add_argument('-o', '--output', required=True)
    parser.add_argument('-I', dest='include_dirs', action='append', default=[], metavar='DIR')
    parser.add_argument('--listing'); parser.add_argument('--symbols', action='store_true')
    parser.add_argument('--symbol-file'); parser.add_argument('--debug-map')
    parser.add_argument('--flat-64k', action='store_true')
    parser.add_argument('--firmware', '--rom', dest='firmware', action='store_true')
    parser.add_argument('--fill')
    ns = parser.parse_args(argv)
    try:
        source_path = Path(ns.source)
        assembler = Assembler(ns.source, source_path.read_text(), ns.include_dirs).assemble()
        fill = int(ns.fill, 0) if ns.fill is not None else (0xff if ns.firmware else 0)
        if not 0 <= fill <= 255: raise AsmError("fill byte must be 0..255")
        if assembler.bytes:
            if ns.firmware:
                start, end = 0xf100, 0x10000
                if any(not start <= address < end for address in assembler.bytes):
                    raise AsmError("firmware output requires all data in 0xf100-0xffff")
                blob = bytes(assembler.bytes.get(address, fill) for address in range(start, end))
            else:
                end = 0x10000 if ns.flat_64k else max(assembler.bytes) + 1
                blob = bytes(assembler.bytes.get(address, fill) for address in range(end))
        else: blob = b''
        Path(ns.output).write_bytes(blob)
        if ns.listing:
            with open(ns.listing, 'w') as listing:
                for src in assembler.sources:
                    if src.address is not None and (src.data or no_comment(src.text).strip()):
                        listing.write(f"{src.address:04x}  {' '.join(f'{x:02X}' for x in src.data):<20} {src.text.rstrip()}\n")
        if ns.symbols or ns.symbol_file:
            lines = [f"{name:<12} 0x{address:04x}" for name, address in sorted(resolved_symbols(assembler).items())]
            if ns.symbols: print('\n'.join(lines))
            if ns.symbol_file: Path(ns.symbol_file).write_text('\n'.join(lines) + '\n')
        if ns.debug_map: write_debug_map(ns.debug_map, assembler, ns.source, "firmware" if ns.firmware else "ram")
        return 0
    except (AsmError, OSError) as exc:
        if isinstance(exc, AsmError):
            filename = exc.filename or ns.source
            location = f"{filename}:{exc.line}" if exc.line else filename
            print(f"{location}: {exc.message}", file=sys.stderr)
        else: print(f"{ns.source}: {exc}", file=sys.stderr)
        return 1


if __name__ == '__main__': sys.exit(main())
