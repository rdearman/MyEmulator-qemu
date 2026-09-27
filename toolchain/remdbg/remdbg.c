#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef NT_PRSTATUS
#define NT_PRSTATUS 1
#endif

/* Matches linux/arch/myemulator2/include/uapi/asm/ptrace.h. */
struct rem_user_regs {
    uint32_t r[16];
    uint32_t pc, sr, cause, info;
};

struct symbol { uint32_t value, size; char *name; };
struct breakpoint { uint32_t addr, saved; int enabled; };
static struct symbol symbols[16384];
static struct breakpoint breakpoints[64];
static size_t breakpoint_count;
static size_t symbol_count;
static pid_t child = -1;
static int child_stopped;
static int stopped_signal;
static int initial_exec_stop = 1;
static int own_breakpoint_stop;
static const char *program;
static int parse_u32(const char *s, uint32_t *v);

static int get_regs(struct rem_user_regs *regs)
{
    struct iovec iov = { regs, sizeof(*regs) };
    memset(regs, 0, sizeof(*regs));
    if (ptrace(PTRACE_GETREGSET, child, (void *)(uintptr_t)NT_PRSTATUS, &iov) < 0)
        return -1;
    if (iov.iov_len < offsetof(struct rem_user_regs, info) + sizeof(regs->info)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static const char *symbol_at(uint32_t pc, uint32_t *offset)
{
    const struct symbol *best = NULL;
    for (size_t i = 0; i < symbol_count; i++) {
        if (symbols[i].value <= pc && (!best || symbols[i].value > best->value))
            best = &symbols[i];
    }
    if (!best) return NULL;
    if (offset) *offset = pc - best->value;
    return best->name;
}

static uint32_t find_symbol(const char *name)
{
    for (size_t i = 0; i < symbol_count; i++)
        if (!strcmp(symbols[i].name, name)) return symbols[i].value;
    return 0;
}

static void load_symbols(const char *path)
{
    FILE *f = fopen(path, "rb");
    Elf32_Ehdr eh;
    if (!f) return;
    if (fread(&eh, 1, sizeof(eh), f) != sizeof(eh) ||
        memcmp(eh.e_ident, ELFMAG, SELFMAG) || eh.e_ident[EI_CLASS] != ELFCLASS32 ||
        eh.e_ident[EI_DATA] != ELFDATA2LSB || eh.e_shentsize != sizeof(Elf32_Shdr) ||
        !eh.e_shnum || eh.e_shnum > 4096) { fclose(f); return; }
    Elf32_Shdr *sh = calloc(eh.e_shnum, sizeof(*sh));
    if (!sh) { fclose(f); return; }
    if (fseek(f, eh.e_shoff, SEEK_SET) || fread(sh, sizeof(*sh), eh.e_shnum, f) != eh.e_shnum) {
        free(sh); fclose(f); return;
    }
    for (unsigned si = 0; si < eh.e_shnum && symbol_count < 16384; si++) {
        if (sh[si].sh_type != SHT_SYMTAB && sh[si].sh_type != SHT_DYNSYM) continue;
        if (sh[si].sh_entsize != sizeof(Elf32_Sym) || sh[si].sh_link >= eh.e_shnum ||
            sh[si].sh_size / sizeof(Elf32_Sym) > 16384) continue;
        Elf32_Shdr str = sh[sh[si].sh_link];
        if (str.sh_size > 16 * 1024 * 1024) continue;
        char *names = malloc(str.sh_size ? str.sh_size : 1);
        Elf32_Sym *syms = malloc(sh[si].sh_size ? sh[si].sh_size : 1);
        if (!names || !syms) { free(names); free(syms); continue; }
        if (fseek(f, str.sh_offset, SEEK_SET) || fread(names, 1, str.sh_size, f) != str.sh_size ||
            fseek(f, sh[si].sh_offset, SEEK_SET) ||
            fread(syms, 1, sh[si].sh_size, f) != sh[si].sh_size) {
            free(names); free(syms); continue;
        }
        size_t n = sh[si].sh_size / sizeof(*syms);
        for (size_t j = 0; j < n && symbol_count < 16384; j++) {
            unsigned type = ELF32_ST_TYPE(syms[j].st_info);
            if (!syms[j].st_value || syms[j].st_name >= str.sh_size ||
                (type != STT_FUNC && type != STT_NOTYPE)) continue;
            const char *name = names + syms[j].st_name;
            if (!*name || !memchr(name, 0, str.sh_size - syms[j].st_name)) continue;
            symbols[symbol_count].name = strdup(name);
            if (!symbols[symbol_count].name) continue;
            symbols[symbol_count].value = syms[j].st_value;
            symbols[symbol_count].size = syms[j].st_size;
            symbol_count++;
        }
        free(names); free(syms);
    }
    free(sh);
    fclose(f);
}

static int peek_word(uint32_t addr, uint32_t *value)
{
    errno = 0;
    long v = ptrace(PTRACE_PEEKDATA, child, (void *)(uintptr_t)addr, 0);
    if (v == -1 && errno) return -1;
    *value = (uint32_t)v;
    return 0;
}

static int poke_word(uint32_t addr, uint32_t value)
{
    if (ptrace(PTRACE_POKEDATA, child, (void *)(uintptr_t)addr,
               (void *)(uintptr_t)value) < 0) return -1;
    return 0;
}

static struct breakpoint *breakpoint_at(uint32_t addr)
{
    for (size_t i = 0; i < breakpoint_count; i++)
        if (breakpoints[i].addr == addr) return &breakpoints[i];
    return NULL;
}

static void add_breakpoint(const char *arg)
{
    uint32_t addr, saved;
    if (parse_u32(arg, &addr) != 0) addr = find_symbol(arg);
    if (!addr) { printf("no symbol/address found: %s\n", arg); return; }
    if (addr & 3) { puts("breakpoint address must be 4-byte aligned"); return; }
    if (breakpoint_at(addr)) { printf("breakpoint already set at 0x%08" PRIx32 "\n", addr); return; }
    if (breakpoint_count == sizeof(breakpoints) / sizeof(breakpoints[0])) { puts("breakpoint table full"); return; }
    if (peek_word(addr, &saved) < 0) { perror("remdbg: read breakpoint instruction"); return; }
    if (poke_word(addr, 0x25000000u) < 0) { perror("remdbg: insert breakpoint"); return; }
    breakpoints[breakpoint_count++] = (struct breakpoint){ addr, saved, 1 };
    printf("breakpoint %zu at 0x%08" PRIx32 "\n", breakpoint_count, addr);
}

static void print_registers(void)
{
    struct rem_user_regs r;
    if (get_regs(&r) < 0) { perror("remdbg: get registers"); return; }
    for (unsigned i = 0; i < 16; i++) printf("r%-2u=%08" PRIx32 "%c", i, r.r[i], i % 4 == 3 ? '\n' : ' ');
    printf("pc=%08" PRIx32 " sr=%08" PRIx32 " cause=%" PRIu32 " info=%08" PRIx32 "\n",
           r.pc, r.sr, r.cause, r.info);
    printf("sp=r13=%08" PRIx32 " lr=r14=%08" PRIx32 " fp=r15=%08" PRIx32 "\n",
           r.r[13], r.r[14], r.r[15]);
    uint32_t off;
    const char *name = symbol_at(r.pc, &off);
    if (name) printf("pc symbol: %s+0x%x\n", name, off);
}

static const char *const rnames[] = {"add","adc","sub","sbc","mul","mulh","mulhu","div","divu","rem","remu","and","or","xor","not","sll","srl","sra","rol","ror","seq","sne","slt","sge","sltu","sgeu"};
static const char *const imnames[] = {"addi","subi","andi","ori","xori","slli","srli","srai"};
static const char *const brnames[] = {"beq","bne","blt","bge","bltu","bgeu"};
static const char *const sysnames[] = {"sr","usp","ssp","vbr","ptbr","mmcr","time_lo","time_hi","timecmp_lo","timecmp_hi","tp","dfsp"};
static int32_t sx(uint32_t x, unsigned n) { uint32_t b = 1u << (n - 1); return (int32_t)((x ^ b) - b); }

static void decode(uint32_t pc, uint32_t w, char *out, size_t cap)
{
    unsigned op = w >> 26, rd = (w >> 21) & 31, ra = (w >> 16) & 31, rb = (w >> 11) & 31;
    if (op == 0 && !(w & 63) && ((w >> 6) & 31) < 26)
        snprintf(out, cap, "%s r%u,r%u,r%u", rnames[(w >> 6) & 31], rd, ra, rb);
    else if (op == 1 && ((w >> 12) & 15) < 8) {
        unsigned sub = (w >> 12) & 15; int32_t imm = sub <= 1 ? sx(w & 0xfff, 12) : (int32_t)(w & 0xfff);
        snprintf(out, cap, "%s r%u,r%u,%d", imnames[sub], rd, ra, imm);
    } else if (op == 2 || op == 3) {
        unsigned sz = (w >> 13) & 7; const char *nm = op == 2 ? (const char *[]) {"lb","lbu","lh","lhu","lw",NULL,NULL,NULL}[sz] : (const char *[]) {"sb","sh","sw",NULL,NULL,NULL,NULL,NULL}[sz];
        if (nm) snprintf(out, cap, "%s r%u,%d(r%u)", nm, rd, sx(w & 0x1fff, 13), ra);
        else snprintf(out, cap, ".word 0x%08" PRIx32, w);
    } else if (op == 4 && ((w >> 23) & 7) < 6)
        snprintf(out, cap, "%s r%u,r%u,0x%08" PRIx32, brnames[(w >> 23) & 7], (w >> 18) & 31, (w >> 13) & 31, pc + 4 + sx(w & 0x1fff, 13) * 4);
    else if (op == 5 || op == 6)
        snprintf(out, cap, "%s 0x%08" PRIx32, op == 5 ? "j" : "jal", pc + 4 + sx(w & 0x3ffffff, 26) * 4);
    else if ((op == 7 && !(w & 0xfffff)) || (op == 8 && !(w & 1)))
        snprintf(out, cap, op == 8 ? "lui r%u,0x%x" : (w >> 25) & 1 ? "jalr r%u" : "jr r%u", op == 8 ? rd : (w >> 20) & 31, (w >> 1) & 0xfffff);
    else if (op == 9 && !(w & 0x7ff) && ((w >> 22) & 15) < 5) {
        unsigned sub = (w >> 22) & 15;
        if (sub < 2) snprintf(out, cap, "%s r%u,%s", sub ? "mtsr" : "mfsr", (w >> 17) & 31, ((w >> 11) & 31) < 12 ? sysnames[(w >> 11) & 31] : "?");
        else snprintf(out, cap, "%s", sub == 2 ? "rfe" : sub == 3 ? "halt" : "break");
    } else if (op == 10) snprintf(out, cap, "syscall 0x%x", w & 0x3ffffff);
    else if (op == 11 && !(w & 0xfffff)) snprintf(out, cap, "tlbflush");
    else if (op == 12 && !(w & 0x7ff)) snprintf(out, cap, "cas r%u,r%u,0(r%u)", rd, (w >> 16) & 31, (w >> 11) & 31);
    else snprintf(out, cap, ".word 0x%08" PRIx32, w);
}

static void disassemble(uint32_t addr, unsigned count)
{
    if (count > 32) count = 32;
    for (unsigned i = 0; i < count; i++, addr += 4) {
        uint32_t w; char insn[128];
        if (peek_word(addr, &w) < 0) { printf("0x%08" PRIx32 ": <unreadable: %s>\n", addr, strerror(errno)); return; }
        decode(addr, w, insn, sizeof(insn));
        printf("0x%08" PRIx32 ": %08" PRIx32 "  %-32s", addr, w, insn);
        uint32_t off; const char *name = symbol_at(addr, &off);
        if (name && off == 0) printf(" <%s>", name);
        putchar('\n');
    }
}

static void memory_dump(uint32_t addr, unsigned len)
{
    if (len > 256) len = 256;
    for (unsigned i = 0; i < len; i += 16) {
        printf("%08" PRIx32 ":", addr + i);
        unsigned n = len - i < 16 ? len - i : 16;
        for (unsigned j = 0; j < n; j++) {
            uint32_t w; uint32_t a = addr + i + j, aligned = a & ~3u;
            if (peek_word(aligned, &w) < 0) { printf(" ??"); continue; }
            printf(" %02x", (w >> ((a & 3) * 8)) & 255);
        }
        putchar('\n');
    }
}

static void backtrace(void)
{
    struct rem_user_regs r;
    if (get_regs(&r) < 0) { perror("remdbg: backtrace registers"); return; }
    uint32_t off;
    const char *name = symbol_at(r.pc, &off);
    printf("#0 0x%08" PRIx32 " %s", r.pc, name ? name : "<unknown>");
    if (name) printf("+0x%x", off);
    putchar('\n');
    if (r.r[14]) {
        name = symbol_at(r.r[14] - 4, &off);
        printf("#1 0x%08" PRIx32 " %s", r.r[14] - 4,
               name ? name : "<return address>");
        if (name) printf("+0x%x", off);
        putchar('\n');
    }
    /* The REM GCC ABI stores LR/FP in a size-dependent frame header. Without
       DWARF unwinding, show possible code pointers in a bounded stack window
       as explicitly heuristic additional frames. */
    unsigned shown = r.r[14] ? 2 : 1;
    for (uint32_t a = r.r[13]; shown < 10 && a < r.r[13] + 512; a += 4) {
        uint32_t value;
        if (peek_word(a, &value) < 0 || value < 4) continue;
        name = symbol_at(value - 4, &off);
        if (!name || off >= 256) continue;
        printf("#%u 0x%08" PRIx32 " %s+0x%x [stack 0x%08" PRIx32 "]?\n",
               shown++, value - 4, name, off, a);
    }
    puts("(frames after #1 are stack-scan guesses; DWARF unwinding is not implemented)");
}

static int parse_u32(const char *s, uint32_t *v)
{
    char *end; errno = 0; unsigned long n = strtoul(s, &end, 0);
    if (errno || end == s || *end || n > UINT32_MAX) return -1;
    *v = (uint32_t)n; return 0;
}

static void report_stop(int status)
{
    if (WIFEXITED(status)) { printf("program exited with status %d\n", WEXITSTATUS(status)); child = -1; child_stopped = 0; return; }
    if (WIFSIGNALED(status)) { printf("program terminated by signal %d\n", WTERMSIG(status)); child = -1; child_stopped = 0; return; }
    if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status); child_stopped = 1; stopped_signal = sig;
        printf("stopped: signal %d (%s)\n", sig, strsignal(sig));
        if (get_regs(&(struct rem_user_regs){0}) == 0) print_registers();
    }
}

static void inspect_breakpoint_stop(void)
{
    struct rem_user_regs r;
    struct breakpoint *bp;
    if (get_regs(&r) < 0) return;
    bp = breakpoint_at(r.pc);
    if (bp && bp->enabled) {
        if (poke_word(bp->addr, bp->saved) < 0) perror("remdbg: restore breakpoint");
        else { bp->enabled = 0; own_breakpoint_stop = 1; printf("hit breakpoint at 0x%08" PRIx32 "\n", bp->addr); }
    }
}

static int resume_child(long request)
{
    if (child < 0 || !child_stopped) { puts("no stopped debuggee"); return -1; }
    if (ptrace(request, child, 0, 0) < 0) return -1;
    child_stopped = 0;
    own_breakpoint_stop = 0;
    stopped_signal = 0;
    int status;
    if (waitpid(child, &status, 0) < 0) { perror("remdbg: waitpid"); return -1; }
    report_stop(status);
    if (WIFSTOPPED(status)) inspect_breakpoint_stop();
    return 0;
}

static void reinsert_disabled_breakpoints(void)
{
    for (size_t i = 0; i < breakpoint_count; i++) {
        struct breakpoint *bp = &breakpoints[i];
        uint32_t current_word;
        if (!bp->enabled && peek_word(bp->addr, &current_word) == 0 &&
            current_word == bp->saved && poke_word(bp->addr, 0x25000000u) == 0)
            bp->enabled = 1;
    }
}

static uint32_t current_pc(void)
{
    struct rem_user_regs r;
    return get_regs(&r) == 0 ? r.pc : 0;
}

static void skip_embedded_break(void)
{
    struct rem_user_regs r;
    uint32_t insn;
    if (!child_stopped || initial_exec_stop || own_breakpoint_stop ||
        stopped_signal != SIGTRAP || get_regs(&r) < 0 ||
        peek_word(r.pc, &insn) < 0 || insn != 0x25000000u) return;
    r.pc += 4;
    if (ptrace(PTRACE_SETREGS, child, 0, &r) == 0) {
        stopped_signal = 0;
        puts("skipping explicit BREAK instruction");
    }
}

static void continue_child(void)
{
	skip_embedded_break();
	initial_exec_stop = 0;
    struct breakpoint *bp = breakpoint_at(current_pc());
    if (bp && !bp->enabled) {
        if (resume_child(PTRACE_SINGLESTEP) < 0) { perror("remdbg: step over breakpoint"); return; }
        reinsert_disabled_breakpoints();
        if (child < 0 || !child_stopped) return;
    }
    if (resume_child(PTRACE_CONT) == 0 && child >= 0) reinsert_disabled_breakpoints();
    else if (child >= 0) perror("remdbg: continue");
}

static void help(void)
{
    puts("run | regs | step | continue | break SYMBOL|ADDRESS | x ADDRESS LENGTH");
    puts("mem ADDRESS LENGTH | write ADDRESS BYTE | disasm [ADDRESS [COUNT]] | bt | quit");
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: remdbg PROGRAM [ARG ...]\n"); return 2; }
    program = argv[1]; load_symbols(program);
    child = fork();
    if (child < 0) { perror("remdbg: fork"); return 1; }
    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) { perror("remdbg: PTRACE_TRACEME"); _exit(126); }
        execvp(program, argv + 1); perror("remdbg: exec"); _exit(127);
    }
    int status;
    if (waitpid(child, &status, 0) < 0) { perror("remdbg: waitpid"); return 1; }
    if (!WIFSTOPPED(status)) { report_stop(status); return 1; }
    child_stopped = 1;
    printf("loaded %s (pid %d); initial exec stop\n", program, (int)child);
    print_registers();
    struct rem_user_regs initial;
    if (get_regs(&initial) == 0) disassemble(initial.pc, 1);
    char line[256];
    while (child >= 0) {
        fputs("remdbg> ", stdout); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        char *save = NULL, *cmd = strtok_r(line, " \t\r\n", &save);
        if (!cmd) continue;
        if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) help();
        else if (!strcmp(cmd, "regs")) print_registers();
        else if (!strcmp(cmd, "step") || !strcmp(cmd, "next")) {
		    skip_embedded_break();
		    initial_exec_stop = 0;
            if (resume_child(PTRACE_SINGLESTEP) < 0) fprintf(stderr, "step failed: %s\n", strerror(errno));
            reinsert_disabled_breakpoints();
        } else if (!strcmp(cmd, "continue") || !strcmp(cmd, "c")) {
            continue_child();
        } else if (!strcmp(cmd, "run")) {
            continue_child();
        } else if (!strcmp(cmd, "bt") || !strcmp(cmd, "backtrace")) {
            backtrace();
        } else if (!strcmp(cmd, "break") || !strcmp(cmd, "b")) {
            char *a = strtok_r(NULL, " \t\r\n", &save);
            if (!a) puts("usage: break SYMBOL|ADDRESS"); else add_breakpoint(a);
        } else if (!strcmp(cmd, "mem") || !strcmp(cmd, "x")) {
            char *a = strtok_r(NULL, " \t\r\n", &save), *n = strtok_r(NULL, " \t\r\n", &save); uint32_t addr, len;
            if (!a || !n || parse_u32(a, &addr) || parse_u32(n, &len)) puts("usage: mem ADDRESS LENGTH"); else memory_dump(addr, len);
        } else if (!strcmp(cmd, "write")) {
            char *a = strtok_r(NULL, " \t\r\n", &save), *v = strtok_r(NULL, " \t\r\n", &save); uint32_t addr, byte, word;
            if (!a || !v || parse_u32(a, &addr) || parse_u32(v, &byte) || byte > 255) puts("usage: write ADDRESS BYTE");
            else if (peek_word(addr & ~3u, &word) < 0) perror("remdbg: read for write");
            else { word = (word & ~(255u << ((addr & 3) * 8))) | (byte << ((addr & 3) * 8)); if (ptrace(PTRACE_POKEDATA, child, (void *)(uintptr_t)(addr & ~3u), (void *)(uintptr_t)word) < 0) perror("remdbg: write memory"); else puts("memory updated"); }
        } else if (!strcmp(cmd, "disasm")) {
            struct rem_user_regs r; uint32_t addr, count = 8; char *a = strtok_r(NULL, " \t\r\n", &save), *n = strtok_r(NULL, " \t\r\n", &save);
            if (!a) { if (get_regs(&r) < 0) perror("remdbg: get registers"); else { addr = r.pc; disassemble(addr, count); } }
            else if (parse_u32(a, &addr) || (n && parse_u32(n, &count))) puts("usage: disasm [ADDRESS [COUNT]]"); else disassemble(addr, count);
        } else if (!strcmp(cmd, "symbol")) {
            char *a = strtok_r(NULL, " \t\r\n", &save); uint32_t addr, off;
            if (!a || parse_u32(a, &addr)) puts("usage: symbol ADDRESS");
            else { const char *s = symbol_at(addr, &off); if (s) printf("0x%08" PRIx32 " = %s+0x%x\n", addr, s, off); else puts("no symbol at or below address"); }
        } else if (!strcmp(cmd, "quit") || !strcmp(cmd, "q")) break;
        else puts("unknown command; type help");
    }
    if (child >= 0) { if (child_stopped) (void)ptrace(PTRACE_KILL, child, 0, 0); (void)waitpid(child, NULL, 0); }
    return 0;
}
