#!/usr/bin/env python3
"""Two-pass assembler for the current MyEmulator ISA."""
from __future__ import annotations
import argparse, json, re, sys
from dataclasses import dataclass
from pathlib import Path

REGS = {f"r{i}": i for i in range(4)}
AREGS = {f"a{i}": i for i in range(4)}
SPECIAL = {"lr": 4, "sp": 5, "pc": 6}
MVA_REGS = {**AREGS, "lr": 4, "sp": 5}
BRANCHES = {"beq": 0, "bne": 1, "blt": 2, "bge": 3, "bltu": 4, "bgeu": 5, "br": 6}
ALU_OPS = {"add": 0, "sub": 1, "and": 2, "or": 3, "xor": 4,
           "shl": 5, "shr": 6}
TOKEN = re.compile(r"\s*(0[xX][0-9a-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|[0-9][0-9_]*|'(?:\\.|[^'])'|[A-Za-z_.$][\w.$]*|[()+-])")

class AsmError(Exception):
    def __init__(self, message, line=None): self.message, self.line = message, line

def no_comment(s):
    quoted = False; esc = False
    for i, c in enumerate(s):
        if quoted:
            if esc: esc = False
            elif c == "\\": esc = True
            elif c == '"': quoted = False
        elif c == '"': quoted = True
        elif c == ';': return s[:i]
    return s

def split_args(s):
    out=[]; start=0; depth=0; quoted=False; esc=False
    for i,c in enumerate(s):
        if quoted:
            if esc: esc=False
            elif c=='\\': esc=True
            elif c=='"': quoted=False
        elif c=='"': quoted=True
        elif c in '[{(': depth += 1
        elif c in ']})': depth -= 1
        elif c==',' and depth==0:
            out.append(s[start:i].strip()); start=i+1
    tail=s[start:].strip()
    if tail or out: out.append(tail)
    return out

def parse_string(s, line):
    if len(s)<2 or s[0] != '"' or s[-1] != '"': raise AsmError("expected quoted string", line)
    out=bytearray(); i=1
    escapes={'n':10,'r':13,'t':9,'\\':92,'"':34,'0':0}
    while i < len(s)-1:
        c=s[i]
        if c != '\\': out.extend(c.encode('utf-8')); i+=1; continue
        i+=1
        if i >= len(s)-1: raise AsmError("unterminated string escape", line)
        c=s[i]
        if c not in escapes: raise AsmError(f"unsupported string escape \\{c}", line)
        out.append(escapes[c]); i+=1
    return bytes(out)

class Expr:
    def __init__(self, text, line):
        self.tokens=[]; pos=0
        while pos < len(text):
            m=TOKEN.match(text,pos)
            if not m: raise AsmError(f"malformed expression near '{text[pos:]}'", line)
            self.tokens.append(m.group(1)); pos=m.end()
        self.i=0; self.line=line; self.tree=self.add()
        if self.i != len(self.tokens): raise AsmError("malformed expression", line)
    def peek(self): return self.tokens[self.i] if self.i<len(self.tokens) else None
    def take(self): t=self.peek(); self.i+=1; return t
    def add(self):
        n=self.atom()
        while self.peek() in ('+','-'):
            op=self.take(); n=(op,n,self.atom())
        return n
    def atom(self):
        t=self.take()
        if t is None: raise AsmError("missing expression", self.line)
        if t == '(':
            n=self.add()
            if self.take()!=')': raise AsmError("missing ')' in expression", self.line)
            return n
        if t in ('+','-'):
            return ('u',t,self.atom())
        if self.peek()=='(' and t.lower() in ('hi','lo'):
            self.take(); n=self.add()
            if self.take()!=')': raise AsmError("missing ')' in function", self.line)
            return (t.lower(),n)
        if t.startswith("'"):
            body=t[1:-1]
            if body.startswith('\\'):
                vals={'n':10,'r':13,'t':9,'\\':92,"'":39,'0':0}
                if len(body)!=2 or body[1] not in vals: raise AsmError("invalid character literal", self.line)
                return ('num',vals[body[1]])
            if len(body)!=1: raise AsmError("character literal must contain one character", self.line)
            return ('num',ord(body))
        if t[0].isdigit():
            try: return ('num',int(t.replace('_',''),0))
            except ValueError: raise AsmError(f"invalid number '{t}'", self.line)
        return ('sym',t.lower())

def eval_tree(tree, symbols, stack=()):
    k=tree[0]
    if k=='num': return tree[1]
    if k=='sym':
        n=tree[1]
        if n not in symbols: raise KeyError(n)
        v=symbols[n]
        if isinstance(v, tuple):
            if n in stack: raise ValueError("cyclic constant definition")
            return eval_tree(v,symbols,stack+(n,))
        return v
    if k=='u':
        v=eval_tree(tree[2],symbols,stack); return v if tree[1]=='+' else -v
    if k in ('+','-'):
        a=eval_tree(tree[1],symbols,stack); b=eval_tree(tree[2],symbols,stack)
        return a+b if k=='+' else a-b
    v=eval_tree(tree[1],symbols,stack)
    return (v >> 8) & 0xff if k=='hi' else v & 0xff

def value(text, symbols, line):
    try: return eval_tree(Expr(text,line).tree,symbols)
    except KeyError as e: raise AsmError(f"undefined symbol '{e.args[0]}'",line)
    except ValueError as e: raise AsmError(str(e),line)

@dataclass
class Source:
    line:int; text:str; address:int|None=None; data:bytes=b''

class Assembler:
    def __init__(self, filename, text):
        self.filename=filename; self.lines=text.splitlines(); self.symbols={}; self.sources=[]; self.bytes={}; self.loc=0
    def err(self,msg,n): raise AsmError(msg,n)
    def reg(self,t,n):
        k=t.strip().lower()
        if k not in REGS: self.err(f"{t} is not a valid data register",n)
        return REGS[k]
    def areg(self,t,n):
        k=t.strip().lower()
        if k not in AREGS: self.err(f"{t} is not a valid address register",n)
        return AREGS[k]
    def imm(self,t,n,bits,signed=False):
        t=t.strip();
        if t.startswith('#'): t=t[1:].strip()
        v=value(t,self.symbols,n)
        lo,hi=(-1<<(bits-1), (1<<(bits-1))-1) if signed else (0,(1<<bits)-1)
        if not lo<=v<=hi: self.err(f"immediate {v} does not fit {'signed' if signed else 'unsigned'} {bits}-bit operand",n)
        return v & ((1<<bits)-1)
    def pass1(self):
        for no,raw in enumerate(self.lines,1):
            s=no_comment(raw).strip(); src=Source(no,raw)
            if not s: self.sources.append(src); continue
            while True:
                m=re.match(r'^([A-Za-z_.$][\w.$]*):',s)
                if not m: break
                name=m.group(1).lower()
                if name in self.symbols: self.err(f"duplicate label '{m.group(1)}'",no)
                self.symbols[name]=self.loc; s=s[m.end():].strip()
            if not s: self.sources.append(src); continue
            m=re.match(r'^([A-Za-z_.$][\w.$]*)\s*=\s*(.+)$',s)
            if m:
                name=m.group(1).lower()
                if name in self.symbols: self.err(f"duplicate symbol '{m.group(1)}'",no)
                self.symbols[name]=Expr(m.group(2).strip(),no).tree; self.sources.append(src); continue
            parts=s.split(None,1); op=parts[0].lower(); args=split_args(parts[1]) if len(parts)>1 else []
            if op=='.org':
                self.loc=value(args[0],self.symbols,no); self.check_loc(no); src.address=self.loc; self.sources.append(src); continue
            size=self.size(op,args,no); src.address=self.loc; self.loc+=size; self.check_loc(no); self.sources.append(src)
    def check_loc(self,n):
        if not 0<=self.loc<=0x10000: self.err("address is outside 16-bit memory",n)
    def size(self,op,a,n):
        if op in ('.byte',): return len(a)
        if op in ('.word',): return 2*len(a)
        if op in ('.ascii','.asciz'):
            if len(a)!=1: self.err(f"{op} takes one string",n)
            return len(parse_string(a[0],n))+(op=='.asciz')
        if op in ('la',):
            if len(a)!=4: self.err("la requires address, value, high scratch, low scratch",n)
            return 6
        return 2
    def encode(self,op,a,n):
        def need(k):
            if len(a)!=k: self.err(f"{op.upper()} expects {k} operands",n)
        if op=='.byte': return bytes(self.imm(x,n,8) for x in a)
        if op=='.word':
            out=bytearray()
            for x in a:
                v=value(x,n and self.symbols,n)
                if not 0<=v<=0xffff: self.err(f"value {v} does not fit unsigned 16-bit operand",n)
                out += bytes((v&255,v>>8))
            return bytes(out)
        if op in ('.ascii','.asciz'):
            b=parse_string(a[0],n); return b+(b'\0' if op=='.asciz' else b'')
        if op=='la':
            need(4); dst=a[0];
            if dst.lower() not in AREGS: self.err("la destination must be A0-A3",n)
            if not a[1].strip().startswith('#'): self.err("la value must be an immediate or symbol prefixed with #",n)
            hi=self.encode('li',[a[2], '#hi('+a[1].strip()[1:]+')'],n); lo=self.encode('li',[a[3], '#lo('+a[1].strip()[1:]+')'],n)
            return hi+lo+self.encode('lda',[dst,a[2],a[3]],n)
        if op=='li': need(2); return w(0x1000|(self.reg(a[0],n)<<10)|self.imm(a[1],n,8))
        if op in ALU_OPS:
            if len(a) == 2:
                rd=self.reg(a[0],n); rn=self.reg(a[1],n)
                return w(0x7700|(ALU_OPS[op]<<4)|(rd<<2)|rn)
            need(3)
            base={"add":0x3000,"sub":0x4000,"and":0x9000,
                  "or":0xa000,"xor":0xb000,"shl":0xc000,
                  "shr":0xd000}[op]
            return w(base|(self.reg(a[0],n)<<10)|
                    (self.reg(a[1],n)<<8)|self.imm(a[2],n,8))
        if op in ('ld','st'):
            need(2); rd=self.reg(a[0],n); m=re.match(r'^\[\s*([^+\]-]+)\s*(?:([+-])\s*(.+))?\s*\]$',a[1])
            if not m: self.err("memory operand must be [An] or [An+/-displacement]",n)
            an=self.areg(m.group(1),n); d=0 if not m.group(3) else self.imm(('-' if m.group(2)=='-' else '')+m.group(3),n,8,True)
            return w((0 if op=='ld' else 0x2000)|(rd<<10)|(an<<8)|d)
        if op=='cmp': need(2); return w(0x8000|(self.reg(a[0],n)<<10)|(self.reg(a[1],n)<<8))
        if op=='jal': need(1); return w(0x5000|self.branch_disp(a[0],n))
        if op in BRANCHES: need(1); return w(0x6000|(BRANCHES[op]<<8)|self.branch_disp(a[0],n))
        if op in ('lda','gta'):
            need(3); an=self.areg(a[0] if op=='lda' else a[2],n); rx=self.reg(a[1] if op=='lda' else a[0],n); ry=self.reg(a[2] if op=='lda' else a[1],n); return w((0x7100 if op=='lda' else 0x7200)|(an<<6)|(rx<<4)|(ry<<2))
        if op=='ada': need(2); return w(0x7000|(self.areg(a[0],n)<<10)|self.imm(a[1],n,8,True))
        if op in ('gf','sf'): need(1); return w(0x7600|(self.reg(a[0],n)<<2)|(op=='sf'))
        if op=='mva': need(2); d=a[0].lower(); s=a[1].lower();
        if op=='mva':
            if d not in MVA_REGS or s not in MVA_REGS: self.err("MVA operands must be A0-A3, LR, or SP",n)
            return w(0x7300|(MVA_REGS[d]<<3)|MVA_REGS[s])
        if op in ('push','pop'):
            need(1); z=a[0].strip()
            if not (z.startswith('{') and z.endswith('}')): self.err("PUSH/POP requires a register list in braces",n)
            names=[x.strip().lower() for x in z[1:-1].split(',') if x.strip()]; mask=0
            for x in names:
                if x not in REGS and x!='lr': self.err(f"{x} is not a valid PUSH/POP register",n)
                bit=4 if x=='lr' else REGS[x]
                if mask&(1<<bit): self.err(f"duplicate register '{x}'",n)
                mask|=1<<bit
            return w((0xe000 if op=='push' else 0xf000)|mask)
        fixed={'ret':0xf040,'rti':0xf060,'halt':0xf080}
        if op in fixed:
            need(0); return w(fixed[op])
        self.err(f"unknown instruction or directive '{op}'",n)
    def branch_disp(self,t,n):
        target=value(t,self.symbols,n)
        if target&1: self.err("branch target is not instruction-aligned",n)
        d=(target-(self.current+2))//2
        if not -128<=d<=127: self.err(f"branch target '{t}' is out of range",n)
        return d&255
    def pass2(self):
        self.current=0
        for src in self.sources:
            s=no_comment(src.text).strip()
            while True:
                m=re.match(r'^([A-Za-z_.$][\w.$]*):',s)
                if not m: break
                s=s[m.end():].strip()
            if not s or re.match(r'^[A-Za-z_.$][\w.$]*\s*=',s): continue
            parts=s.split(None,1); op=parts[0].lower(); a=split_args(parts[1]) if len(parts)>1 else []
            if op=='.org': self.current=value(a[0],self.symbols,src.line); continue
            src.address=self.current; data=self.encode(op,a,src.line); src.data=data
            for i,b in enumerate(data):
                p=self.current+i
                if p in self.bytes: self.err(f"overlapping output at address 0x{p:04x}",src.line)
                self.bytes[p]=b
            self.current+=len(data)
    def assemble(self): self.pass1(); self.pass2(); return self

def w(v): return bytes((v&255,(v>>8)&255))

def resolved_symbols(assembler):
    result = {}
    for name, value_ in assembler.symbols.items():
        try:
            result[name] = eval_tree(value_, assembler.symbols) if isinstance(value_, tuple) else value_
        except (KeyError, ValueError):
            pass
    return result

def write_debug_map(path, assembler, source):
    locations = []
    for src in assembler.sources:
        if src.address is None or not src.data:
            continue
        for offset in range(0, len(src.data), 2):
            locations.append({"address": src.address + offset,
                              "source": str(source), "line": src.line,
                              "text": src.text.rstrip()})
    data = {"format": "myemulator-debug-v1", "source": str(source),
            "symbols": resolved_symbols(assembler), "locations": locations}
    Path(path).write_text(json.dumps(data, indent=2) + "\n")

def main(argv=None):
    p=argparse.ArgumentParser(prog='myasm'); p.add_argument('source'); p.add_argument('-o','--output',required=True); p.add_argument('--listing'); p.add_argument('--symbols',action='store_true'); p.add_argument('--symbol-file'); p.add_argument('--debug-map'); p.add_argument('--flat-64k',action='store_true'); p.add_argument('--fill',default='0')
    ns=p.parse_args(argv)
    try:
        a=Assembler(ns.source,Path(ns.source).read_text()).assemble()
        fill=int(ns.fill,0)
        if not 0<=fill<=255: raise AsmError("fill byte must be 0..255")
        if a.bytes:
            end=0x10000 if ns.flat_64k else max(a.bytes)+1
            blob=bytes(a.bytes.get(i,fill) for i in range(end))
        else: blob=b''
        Path(ns.output).write_bytes(blob)
        if ns.listing:
            with open(ns.listing,'w') as f:
                for s in a.sources:
                    if s.address is not None and (s.data or no_comment(s.text).strip()): f.write(f"{s.address:04x}  {' '.join(f'{x:02X}' for x in s.data):<20} {s.text.rstrip()}\n")
        if ns.symbols or ns.symbol_file:
            lines=[]
            for k,v in sorted(a.symbols.items()):
                try: resolved = eval_tree(v, a.symbols) if isinstance(v, tuple) else v
                except (KeyError, ValueError): continue
                lines.append(f"{k:<12} 0x{resolved:04x}")
            if ns.symbols: print('\n'.join(lines))
            if ns.symbol_file: Path(ns.symbol_file).write_text('\n'.join(lines)+'\n')
        if ns.debug_map:
            write_debug_map(ns.debug_map, a, ns.source)
        return 0
    except (AsmError, OSError) as e:
        if isinstance(e,AsmError): print(f"{ns.source}:{e.line}: {e.message}" if e.line else f"{ns.source}: {e.message}",file=sys.stderr)
        else: print(f"{ns.source}: {e}",file=sys.stderr)
        return 1
if __name__=='__main__': sys.exit(main())
