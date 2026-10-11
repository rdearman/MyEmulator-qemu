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
#include <sys/wait.h>
#include <unistd.h>

/* Matches linux/arch/myemulator2/include/uapi/asm/ptrace.h. */
struct rem_user_regs {
    uint32_t r[16];
    uint32_t pc, sr, cause, info;
};

struct symbol { uint32_t value, size; char *name; };
struct breakpoint { uint32_t addr, saved; int enabled; int num, user_en, temp, ignore; unsigned hits; };
static struct symbol symbols[16384];
static struct breakpoint breakpoints[64];
static size_t breakpoint_count;
static int next_bp_num = 1;
static struct breakpoint *hit_bp;
static size_t symbol_count;
static pid_t child = -1;
static int child_stopped;
static int stopped_signal;
static int initial_exec_stop = 1;
static int own_breakpoint_stop;
static const char *program;
static uint32_t current_pc(void);
static void backtrace(void);

static int get_regs(struct rem_user_regs *regs)
{
    memset(regs, 0, sizeof(*regs));
    return ptrace(PTRACE_GETREGS, child, 0, regs) < 0 ? -1 : 0;
}

static const char *symbol_at(uint32_t pc, uint32_t *offset)
{
    const struct symbol *best = NULL;
    for (size_t i = 0; i < symbol_count; i++) {
        if (symbols[i].value <= pc && (!best || symbols[i].value > best->value))
            best = &symbols[i];
    }
    if (!best) return NULL;
    if (best->size ? pc - best->value >= best->size : pc - best->value > 0x400) return NULL;
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

/* With no live process, read static contents from the program's PT_LOAD segments, as GDB does. */
static int file_peek(uint32_t addr, uint32_t *value)
{
    FILE *f = program ? fopen(program, "rb") : NULL;
    Elf32_Ehdr eh; Elf32_Phdr ph;
    int rc = -1;
    if (!f) return -1;
    if (fread(&eh, 1, sizeof(eh), f) == sizeof(eh) && !memcmp(eh.e_ident, ELFMAG, SELFMAG) && eh.e_phentsize == sizeof(ph)) {
        for (unsigned i = 0; i < eh.e_phnum && rc < 0; i++) {
            if (fseek(f, (long)(eh.e_phoff + i * sizeof(ph)), SEEK_SET) || fread(&ph, 1, sizeof(ph), f) != sizeof(ph)) break;
            if (ph.p_type != PT_LOAD || addr < ph.p_vaddr || addr - ph.p_vaddr + 4 > ph.p_memsz) continue;
            uint32_t off = addr - ph.p_vaddr, w = 0;
            if (off + 4 <= ph.p_filesz) {
                if (fseek(f, (long)(ph.p_offset + off), SEEK_SET) || fread(&w, 1, 4, f) != 4) break;
            } else if (off < ph.p_filesz) break;
            *value = w; rc = 0;
        }
    }
    fclose(f);
    return rc;
}

static int peek_word(uint32_t addr, uint32_t *value)
{
    if (child < 0) return file_peek(addr, value);
    errno = 0;
    long v = ptrace(PTRACE_PEEKDATA, child, (void *)(uintptr_t)addr, 0);
    if (v == -1 && errno) return -1;
    *value = (uint32_t)v;
    for (size_t i = 0; i < breakpoint_count; i++)
        if (breakpoints[i].enabled && breakpoints[i].addr == addr) *value = breakpoints[i].saved;
    return 0;
}

static int peek_text_word(uint32_t addr, uint32_t *value)
{
    errno = 0;
    long v = ptrace(PTRACE_PEEKTEXT, child, (void *)(uintptr_t)addr, 0);
    if (v == -1 && errno) return -1;
    *value = (uint32_t)v;
    return 0;
}

static int poke_text_word(uint32_t addr, uint32_t value)
{
    return ptrace(PTRACE_POKETEXT, child, (void *)(uintptr_t)addr,
                  (void *)(uintptr_t)value) < 0 ? -1 : 0;
}

static struct breakpoint *breakpoint_at(uint32_t addr)
{
    for (size_t i = 0; i < breakpoint_count; i++)
        if (breakpoints[i].addr == addr) return &breakpoints[i];
    return NULL;
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

static void backtrace(void)
{
    struct rem_user_regs r;
    if (get_regs(&r) < 0) { puts("No stack."); return; }
    uint32_t off;
    const char *name = symbol_at(r.pc, &off);
    printf("#0  0x%08" PRIx32 " in %s ()\n", r.pc, name ? name : "??");
    if (r.r[14]) {
        name = symbol_at(r.r[14] - 4, &off);
        printf("#1  0x%08" PRIx32 " in %s ()\n", r.r[14], name ? name : "??");
    }
    /* No CFI/DWARF unwinder: deeper frames are stack-scan guesses. */
    unsigned shown = r.r[14] ? 2 : 1;
    for (uint32_t a = r.r[13]; shown < 10 && a < r.r[13] + 512; a += 4) {
        uint32_t value;
        if (peek_word(a, &value) < 0 || value < 4) continue;
        name = symbol_at(value - 4, &off);
        if (!name || off >= 256) continue;
        printf("#%u  0x%08" PRIx32 " in %s () [guess, stack 0x%08" PRIx32 "]\n",
               shown++, value, name, a);
    }
    puts("(frames after #1 are heuristic: no DWARF/CFI unwinder)");
}

static FILE *open_readelf_dump(const char *dump, pid_t *reader)
{
    int fds[2];
    if (pipe(fds) < 0) return NULL;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return NULL; }
    if (pid == 0) {
        char option[64];
        close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0) _exit(126);
        close(fds[1]);
        snprintf(option, sizeof(option), "--debug-dump=%s", dump);
        execlp("readelf", "readelf", option, program, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    FILE *stream = fdopen(fds[0], "r");
    if (!stream) {
        close(fds[0]);
        (void)waitpid(pid, NULL, 0);
        return NULL;
    }
    *reader = pid;
    return stream;
}

static void close_readelf_dump(FILE *stream, pid_t reader)
{
    int status;
    fclose(stream);
    while (waitpid(reader, &status, 0) < 0 && errno == EINTR) {}
}

static char *attribute_text(char *line, const char *attribute)
{
    char *p = strstr(line, attribute);
    if (!p) return NULL;
    p = strchr(p, ':');
    if (!p) return NULL;
    char *indirect = strstr(p, "):");
    if (indirect) p = indirect + 2;
    else p++;
    while (*p == ' ' || *p == '\t') p++;
    char *end = p + strlen(p);
    while (end > p && (end[-1] == '\n' || end[-1] == '\r' ||
                       end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = 0;
    return p;
}

/* List the frame-relative location expressions emitted for function-scoped
 * GCC variables.  Do not claim values until the REM compiler's DWARF offsets
 * agree with its actual frame stores; current GCC output fails that check. */
static void show_locals(void)
{
    struct rem_user_regs regs;
    const char *function;
    FILE *pipe;
    pid_t reader;
    char line[2048];
    char sub_name[128] = "", local_name[128] = "";
    long local_fbreg = 0;
    int local_has_fbreg = 0, local_is_formal = 0;
    int selected = 0, sub_pending = 0;
    unsigned sub_depth = 0, selected_depth = 0, local_depth = 0;
    unsigned local_arg_index = 0, next_arg_index = 0;
    uint32_t selected_entry = 0;
    uint32_t low_pc = 0, high_pc = 0;
    unsigned shown = 0;

    if (get_regs(&regs) < 0) { perror("remdbg: locals registers"); return; }
    function = symbol_at(regs.pc, NULL);
    if (!function) { puts("locals: current PC has no function symbol"); return; }
    pipe = open_readelf_dump("info", &reader);
    if (!pipe) { perror("remdbg: readelf info"); return; }

    while (fgets(line, sizeof(line), pipe)) {
        char tag[80];
        unsigned depth;
        char *abbrev = strstr(line, "Abbrev Number:");
        if (abbrev && sscanf(line, " <%u>", &depth) == 1) {
            char *lp = strchr(abbrev, '('), *rp = lp ? strchr(lp, ')') : NULL;
            if (lp && rp && (size_t)(rp - lp - 1) < sizeof(tag)) {
                memcpy(tag, lp + 1, rp - lp - 1);
                tag[rp - lp - 1] = 0;

                if (local_name[0] && selected && local_depth == selected_depth + 1 &&
                    (local_has_fbreg || (local_is_formal &&
                     regs.pc == selected_entry && local_arg_index < 4))) {
                    if (local_is_formal && regs.pc == selected_entry && local_arg_index < 4) {
                        unsigned reg = local_arg_index + 1;
                        printf("%s = 0x%08" PRIx32 " (entry argument r%u)\n",
                               local_name, regs.r[reg], reg);
                    } else {
                        printf("%s: DW_OP_fbreg %ld (value unavailable: target location not validated)\n",
                               local_name, local_fbreg);
                    }
                    shown++;
                }
                local_name[0] = 0;
                local_has_fbreg = 0;
                local_is_formal = 0;

                if (sub_pending && depth > sub_depth) {
                    uint32_t high = high_pc < low_pc ? low_pc + high_pc : high_pc;
                    if (!strcmp(sub_name, function) && regs.pc >= low_pc && regs.pc < high) {
                        selected = 1;
                        selected_depth = sub_depth;
                        selected_entry = low_pc;
                        next_arg_index = 0;
                    }
                    sub_pending = 0;
                }
                if (selected && depth <= selected_depth) selected = 0;
                if (!strcmp(tag, "DW_TAG_subprogram")) {
                    selected = 0;
                    sub_pending = 1;
                    sub_depth = depth;
                    sub_name[0] = 0;
                    low_pc = high_pc = 0;
                } else if (selected && depth == selected_depth + 1 &&
                           (!strcmp(tag, "DW_TAG_variable") ||
                            !strcmp(tag, "DW_TAG_formal_parameter"))) {
                    local_depth = depth;
                    local_is_formal = !strcmp(tag, "DW_TAG_formal_parameter");
                    if (local_is_formal) local_arg_index = next_arg_index++;
                } else {
                    local_depth = 0;
                }
                continue;
            }
        }

        if (sub_pending) {
            char *v;
            if ((v = attribute_text(line, "DW_AT_name")))
                snprintf(sub_name, sizeof(sub_name), "%s", v);
            else if ((v = attribute_text(line, "DW_AT_low_pc")))
                low_pc = (uint32_t)strtoul(v, NULL, 0);
            else if ((v = attribute_text(line, "DW_AT_high_pc")))
                high_pc = (uint32_t)strtoul(v, NULL, 0);
        } else if (local_depth) {
            char *v;
            if ((v = attribute_text(line, "DW_AT_name")))
                snprintf(local_name, sizeof(local_name), "%s", v);
            else if (strstr(line, "DW_AT_location") && strstr(line, "DW_OP_fbreg:")) {
                char *op = strstr(line, "DW_OP_fbreg:");
                local_fbreg = strtol(op + strlen("DW_OP_fbreg:"), NULL, 10);
                local_has_fbreg = 1;
            }
        }
    }
    if (local_name[0] && selected && local_depth == selected_depth + 1 &&
        (local_has_fbreg || (local_is_formal &&
         regs.pc == selected_entry && local_arg_index < 4))) {
        if (local_is_formal && regs.pc == selected_entry && local_arg_index < 4) {
            unsigned reg = local_arg_index + 1;
            printf("%s = 0x%08" PRIx32 " (entry argument r%u)\n",
                   local_name, regs.r[reg], reg);
        } else {
            printf("%s: DW_OP_fbreg %ld (value unavailable: target location not validated)\n",
                   local_name, local_fbreg);
        }
        shown++;
    }
    close_readelf_dump(pipe, reader);
    if (!shown) puts("locals: no supported DWARF frame-relative locals at this PC");
}

static void show_source_location(void)
{
    struct rem_user_regs regs;
    FILE *pipe;
    pid_t reader;
    char line[1024], best_file[256] = "";
    unsigned best_line = 0;
    uint32_t best_addr = 0;
    if (get_regs(&regs) < 0) { perror("remdbg: source registers"); return; }
    pipe = open_readelf_dump("decodedline", &reader);
    if (!pipe) { perror("remdbg: readelf line table"); return; }
    while (fgets(line, sizeof(line), pipe)) {
        char file[256], number[32], address[32];
        unsigned source_line;
        unsigned long source_addr;
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (sscanf(p, "%255s %31s %31s", file, number, address) != 3 ||
            !strncmp(number, "-", 1) || strncmp(address, "0x", 2)) continue;
        source_line = (unsigned)strtoul(number, NULL, 10);
        source_addr = strtoul(address, NULL, 0);
        if (source_line && source_addr <= regs.pc && source_addr >= best_addr) {
            snprintf(best_file, sizeof(best_file), "%s", file);
            best_line = source_line;
            best_addr = (uint32_t)source_addr;
        }
    }
    close_readelf_dump(pipe, reader);
    if (best_line)
        printf("0x%08" PRIx32 " = %s:%u (line row at 0x%08" PRIx32 ")\n",
               regs.pc, best_file, best_line, best_addr);
    else puts("source: no DWARF line row at or before this PC");
}

static void print_global_word(const char *name, uint32_t addr)
{
    uint32_t value;
    if (peek_word(addr, &value) == 0)
        printf("%s = 0x%08" PRIx32 " @0x%08" PRIx32 " (DW_OP_addr, 32-bit word)\n",
               name, value, addr);
    else
        printf("%s @0x%08" PRIx32 " (unreadable: %s)\n",
               name, addr, strerror(errno));
}

/* Read simple CU-scope DW_OP_addr variables.  Frame-relative locals need
 * compiler-consistent CFI/location expressions and remain separately gated. */
static void show_globals(void)
{
    FILE *pipe;
    pid_t reader;
    char line[2048], name[128] = "";
    uint32_t addr = 0;
    int active = 0, has_addr = 0;
    unsigned shown = 0;

    pipe = open_readelf_dump("info", &reader);
    if (!pipe) { perror("remdbg: readelf globals"); return; }
    while (fgets(line, sizeof(line), pipe)) {
        unsigned depth;
        if (strstr(line, "Abbrev Number:") && sscanf(line, " <%u>", &depth) == 1) {
            if (active && name[0] && has_addr) {
                print_global_word(name, addr);
                shown++;
            }
            active = depth == 1 && strstr(line, "DW_TAG_variable") != NULL;
            name[0] = 0;
            addr = 0;
            has_addr = 0;
            continue;
        }
        if (!active)
            continue;
        char *value = attribute_text(line, "DW_AT_name");
        if (value)
            snprintf(name, sizeof(name), "%s", value);
        char *location = strstr(line, "DW_OP_addr:");
        if (location) {
            char *end;
            unsigned long parsed = strtoul(location + strlen("DW_OP_addr:"), &end, 16);
            if (end != location + strlen("DW_OP_addr:") && parsed <= UINT32_MAX) {
                addr = (uint32_t)parsed;
                has_addr = 1;
            }
        }
    }
    if (active && name[0] && has_addr) {
        print_global_word(name, addr);
        shown++;
    }
    close_readelf_dump(pipe, reader);
    if (!shown) puts("globals: no CU-scope DW_OP_addr variables found");
}

static const struct { int num; const char *name, *desc; } signal_names[] = {
    {SIGHUP,"SIGHUP","Hangup"},{SIGINT,"SIGINT","Interrupt"},{SIGQUIT,"SIGQUIT","Quit"},
    {SIGILL,"SIGILL","Illegal instruction"},{SIGTRAP,"SIGTRAP","Trace/breakpoint trap"},
    {SIGABRT,"SIGABRT","Aborted"},{SIGBUS,"SIGBUS","Bus error"},
    {SIGFPE,"SIGFPE","Arithmetic exception"},{SIGKILL,"SIGKILL","Killed"},
    {SIGUSR1,"SIGUSR1","User defined signal 1"},{SIGSEGV,"SIGSEGV","Segmentation fault"},
    {SIGUSR2,"SIGUSR2","User defined signal 2"},{SIGPIPE,"SIGPIPE","Broken pipe"},
    {SIGALRM,"SIGALRM","Alarm clock"},{SIGTERM,"SIGTERM","Terminated"},
};

static void signal_text(int sig, const char **name, const char **desc)
{
    for (size_t i = 0; i < sizeof(signal_names) / sizeof(signal_names[0]); i++)
        if (signal_names[i].num == sig) { *name = signal_names[i].name; *desc = signal_names[i].desc; return; }
    *name = "SIG?"; *desc = strsignal(sig);
}

static void frame_line(uint32_t pc)
{
    uint32_t off;
    const char *name = symbol_at(pc, &off);
    printf("0x%08" PRIx32 " in %s ()\n", pc, name ? name : "??");
}

static void forget_patches(void)
{
    for (size_t i = 0; i < breakpoint_count; i++) breakpoints[i].enabled = 0;
}

static void report_stop(int status)
{
    pid_t pid = child;
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status)) printf("[Inferior 1 (process %d) exited with code %02o]\n", (int)pid, WEXITSTATUS(status));
        else printf("[Inferior 1 (process %d) exited normally]\n", (int)pid);
        child = -1; child_stopped = 0; forget_patches(); return;
    }
    if (WIFSIGNALED(status)) {
        const char *n, *d; signal_text(WTERMSIG(status), &n, &d);
        printf("\nProgram terminated with signal %s, %s.\nThe program no longer exists.\n", n, d);
        child = -1; child_stopped = 0; forget_patches(); return;
    }
    if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status); child_stopped = 1; stopped_signal = sig;
        if (sig == SIGTRAP) return;
        struct rem_user_regs regs;
        const char *n, *d; signal_text(sig, &n, &d);
        printf("\nProgram received signal %s, %s.\n", n, d);
        if (get_regs(&regs) == 0) {
            if ((sig == SIGSEGV || sig == SIGBUS) && regs.cause >= 2 && regs.cause <= 9 && regs.info)
                printf("(fault address 0x%08" PRIx32 ", trap cause %" PRIu32 ")\n", regs.info, regs.cause);
            frame_line(regs.pc);
        }
    }
}

static void inspect_breakpoint_stop(void)
{
    struct rem_user_regs r;
    struct breakpoint *bp;
    if (get_regs(&r) < 0) return;
    bp = breakpoint_at(r.pc);
    if (bp && bp->enabled) {
        if (poke_text_word(bp->addr, bp->saved) < 0) perror("remdbg: restore breakpoint");
        else { bp->enabled = 0; own_breakpoint_stop = 1; hit_bp = bp; }
    }
}

static int resume_child(long request)
{
	if (child < 0 || !child_stopped) { puts("no stopped debuggee"); return -1; }
	/* Breakpoint and single-step SIGTRAPs are debugger events.  Other
	 * stopped signals (especially SIGSEGV) must be delivered on continue so
	 * the program retains normal Linux signal semantics instead of retrying
	 * a fault forever. */
	int deliver = stopped_signal == SIGTRAP ? 0 : stopped_signal;
	if (ptrace(request, child, 0, (void *)(uintptr_t)deliver) < 0) return -1;
    child_stopped = 0;
    own_breakpoint_stop = 0;
    hit_bp = NULL;
    stopped_signal = 0;
    int status;
    if (waitpid(child, &status, 0) < 0) { perror("remdbg: waitpid"); return -1; }
	if (WIFSTOPPED(status) && ((unsigned int)status >> 16) == PTRACE_EVENT_EXIT) {
		/* Restore patched text before the exiting tracee releases its mm, so
		 * another run of the same executable starts with pristine code. */
		for (size_t i = 0; i < breakpoint_count; i++) {
			struct breakpoint *bp = &breakpoints[i];
			if (bp->enabled) {
				if (poke_text_word(bp->addr, bp->saved) < 0)
					perror("remdbg: restore breakpoint at exit");
				else
					bp->enabled = 0;
			}
		}
		if (ptrace(PTRACE_CONT, child, 0, 0) < 0 ||
		    waitpid(child, &status, 0) < 0) {
			perror("remdbg: finish tracee exit");
			return -1;
		}
	}
    report_stop(status);
    if (WIFSTOPPED(status)) inspect_breakpoint_stop();
    return 0;
}

static void reinsert_disabled_breakpoints(void)
{
    uint32_t pc;
    if (child < 0) return;
    pc = current_pc();
    for (size_t i = 0; i < breakpoint_count; i++) {
        struct breakpoint *bp = &breakpoints[i];
        uint32_t word;
        /* Keep a breakpoint removed while stopped on it so continue can step
         * over the original instruction first. */
        if (bp->user_en && !bp->enabled && pc != bp->addr &&
            peek_text_word(bp->addr, &word) == 0 && poke_text_word(bp->addr, 0x25000000u) == 0) {
            bp->saved = word;
            bp->enabled = 1;
        }
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
        peek_text_word(r.pc, &insn) < 0 || insn != 0x25000000u) return;
    r.pc += 4;
    if (ptrace(PTRACE_SETREGS, child, 0, &r) == 0) {
        struct rem_user_regs check;
        stopped_signal = 0;
        if (get_regs(&check) == 0)
            printf("skipping explicit BREAK instruction at 0x%08" PRIx32
                   ", next PC 0x%08" PRIx32 "\n", r.pc - 4, check.pc);
        else
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
        if (child < 0 || !child_stopped || hit_bp || stopped_signal != SIGTRAP) return;
    }
    if (resume_child(PTRACE_CONT) == 0 && child >= 0) reinsert_disabled_breakpoints();
    else if (child >= 0) perror("remdbg: continue");
}

static int start_child(const char *path, char *const child_argv[])
{
    int status;

    for (size_t i = 0; i < symbol_count; i++)
        free(symbols[i].name);
    symbol_count = 0;
    free((void *)program);
    program = strdup(path);
    if (!program) { perror("remdbg: program path"); return -1; }
    load_symbols(path);
    for (size_t i = 0; i < breakpoint_count; i++) breakpoints[i].enabled = 0;
    child = fork();
    if (child < 0) { perror("remdbg: fork"); return -1; }
    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {
            perror("remdbg: PTRACE_TRACEME");
            _exit(126);
        }
        execvp(path, child_argv);
        perror("remdbg: exec");
        _exit(127);
    }
    if (waitpid(child, &status, 0) < 0) {
        perror("remdbg: waitpid");
        (void)ptrace(PTRACE_KILL, child, 0, 0);
        (void)waitpid(child, NULL, 0);
        child = -1;
        return -1;
    }
    if (!WIFSTOPPED(status)) {
        report_stop(status);
        return -1;
    }
    child_stopped = 1;
    initial_exec_stop = 1;
    own_breakpoint_stop = 0;
    stopped_signal = WSTOPSIG(status);
    if (ptrace(PTRACE_SETOPTIONS, child, 0,
               (void *)(uintptr_t)PTRACE_O_TRACEEXIT) < 0)
        perror("remdbg: PTRACE_SETOPTIONS (exit cleanup)");
    hit_bp = NULL;
    reinsert_disabled_breakpoints();
    return 0;
}

static void stop_child_for_reload(void)
{
	int status;
	pid_t old_child;
	pid_t waited;

    if (child < 0)
        return;
    if (!child_stopped) {
        puts("cannot reload while the debuggee is running");
        return;
    }
	old_child = child;
    if (ptrace(PTRACE_KILL, old_child, 0, 0) < 0 && errno != ESRCH) {
        perror("remdbg: kill old debuggee");
	}
	/* TRACEEXIT adds an intermediate ptrace stop to a killed child.  Drain
	 * that event before forgetting the old PID, otherwise it remains stopped
	 * and can leak a task across repeated loads. */
	for (;;) {
		waited = waitpid(old_child, &status, 0);
		if (waited < 0) {
			if (errno == EINTR)
				continue;
			perror("remdbg: wait old debuggee");
			break;
		}
		if (waited != old_child || WIFEXITED(status) || WIFSIGNALED(status))
			break;
		if (WIFSTOPPED(status) && ptrace(PTRACE_CONT, old_child, 0, 0) < 0 &&
		    errno != ESRCH) {
			perror("remdbg: finish old debuggee exit");
			break;
		}
	}
    child = -1;
    child_stopped = 0;
    stopped_signal = 0;
    own_breakpoint_stop = 0;
    hit_bp = NULL;
    for (size_t i = 0; i < breakpoint_count; i++) breakpoints[i].enabled = 0;
}

/* ====================================================================
 * GDB command language layer
 * ==================================================================== */

#include <ctype.h>

static char *run_args[64];
static int run_argc;
static int should_quit;

static const struct { const char *name; int idx; } regtab[] = {
    {"r0",0},{"r1",1},{"r2",2},{"r3",3},{"r4",4},{"r5",5},{"r6",6},{"r7",7},
    {"r8",8},{"r9",9},{"r10",10},{"r11",11},{"r12",12},{"r13",13},{"r14",14},{"r15",15},
    {"sp",13},{"lr",14},{"fp",15},{"pc",16},{"sr",17},{"cause",18},{"info",19},
};
#define NREGTAB (sizeof(regtab) / sizeof(regtab[0]))

static uint32_t *regslot(struct rem_user_regs *r, int idx)
{
    if (idx < 16) return &r->r[idx];
    return idx == 16 ? &r->pc : idx == 17 ? &r->sr : idx == 18 ? &r->cause : &r->info;
}

static int reg_index(const char *name, size_t len)
{
    for (size_t i = 0; i < NREGTAB; i++)
        if (strlen(regtab[i].name) == len && !strncmp(regtab[i].name, name, len)) return regtab[i].idx;
    return -1;
}

static char *skipws(char *s) { while (*s == ' ' || *s == '\t') s++; return s; }

static void trim_end(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
}

static void print_sym(uint32_t a, int zero_off)
{
    uint32_t off;
    const char *n = symbol_at(a, &off);
    if (!n) return;
    if (off || zero_off) printf(" <%s+%u>", n, (unsigned)off);
    else printf(" <%s>", n);
}

/* ---- expressions ---- */

enum { K_INT, K_PC, K_PTR, K_SYM };
struct ev { const char *p; char err[160]; int kind; int ops; };
static uint32_t history[256];
static unsigned history_n;

static void ev_fail(struct ev *e, const char *fmt, const char *arg)
{
    if (!e->err[0]) snprintf(e->err, sizeof(e->err), fmt, arg);
}

static void ev_ws(struct ev *e) { while (*e->p == ' ' || *e->p == '\t') e->p++; }
static uint32_t ev_expr(struct ev *e, int minprec);

static uint32_t ev_primary(struct ev *e)
{
    uint32_t v = 0;
    ev_ws(e);
    char c = *e->p;
    if (c == '(') {
        e->p++;
        v = ev_expr(e, 0);
        ev_ws(e);
        if (*e->p != ')') { ev_fail(e, "A syntax error in expression, near `%s'.", e->p); return 0; }
        e->p++; e->ops++;
        return v;
    }
    if (c == '-' || c == '+' || c == '~' || c == '!' || c == '*') {
        e->p++; e->ops++;
        v = ev_primary(e);
        if (c == '-') return -v;
        if (c == '~') return ~v;
        if (c == '!') return !v;
        if (c == '*') {
            uint32_t w;
            if (e->err[0]) return 0;
            if (peek_word(v & ~3u, &w) < 0) {
                char b[16]; snprintf(b, sizeof(b), "0x%x", (unsigned)v);
                ev_fail(e, "Cannot access memory at address %s", b); return 0;
            }
            return w;
        }
        return v;
    }
    if (isdigit((unsigned char)c)) {
        char *end;
        unsigned long n = strtoul(e->p, &end, 0);
        if (isalnum((unsigned char)*end) || *end == '_') {
            char b[48]; size_t l = 0;
            while (isalnum((unsigned char)e->p[l]) && l < sizeof(b) - 1) { b[l] = e->p[l]; l++; }
            b[l] = 0; ev_fail(e, "Invalid number \"%s\".", b); return 0;
        }
        e->p = end; e->kind = K_INT;
        return (uint32_t)n;
    }
    if (c == '\'' && e->p[1] && e->p[2] == '\'') { v = (unsigned char)e->p[1]; e->p += 3; e->kind = K_INT; return v; }
    if (c == '$') {
        const char *s = ++e->p; size_t l = 0;
        while (isalnum((unsigned char)s[l]) || s[l] == '_' || s[l] == '$') l++;
        e->p += l;
        if (l == 0 || (l == 1 && s[0] == '$') || isdigit((unsigned char)s[0])) {
            unsigned idx;
            if (l == 0) idx = history_n;
            else if (s[0] == '$') idx = history_n - 1;
            else idx = (unsigned)strtoul(s, NULL, 10);
            if (!history_n || idx == 0 || idx > history_n || history_n - idx >= 256) { ev_fail(e, "History has not yet reached $%s.", l ? s : ""); return 0; }
            e->kind = K_INT;
            return history[(idx - 1) & 255];
        }
        int ri = reg_index(s, l);
        if (ri < 0) { char b[48]; snprintf(b, sizeof(b), "%.*s", (int)l, s); ev_fail(e, "Invalid register `%s'", b); return 0; }
        struct rem_user_regs r;
        if (child < 0 || get_regs(&r) < 0) { ev_fail(e, "No registers.%s", ""); return 0; }
        v = *regslot(&r, ri);
        e->kind = ri == 16 ? K_PC : (ri == 13 || ri == 15) ? K_PTR : K_INT;
        return v;
    }
    if (isalpha((unsigned char)c) || c == '_') {
        char b[128]; size_t l = 0;
        while ((isalnum((unsigned char)e->p[l]) || e->p[l] == '_' || e->p[l] == '.') && l < sizeof(b) - 1) { b[l] = e->p[l]; l++; }
        b[l] = 0; e->p += l;
        v = find_symbol(b);
        if (!v) { ev_fail(e, "No symbol \"%s\" in current context.", b); return 0; }
        e->kind = K_SYM;
        return v;
    }
    ev_fail(e, "A syntax error in expression, near `%s'.", e->p);
    return 0;
}

static int binop(const char *p, int *len, int *prec)
{
    static const struct { const char *t; int prec; } ops[] = {
        {"||",1},{"&&",2},{"==",6},{"!=",6},{"<=",7},{">=",7},{"<<",8},{">>",8},
        {"|",3},{"^",4},{"&",5},{"<",7},{">",7},{"+",9},{"-",9},{"*",10},{"/",10},{"%",10},
    };
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        size_t l = strlen(ops[i].t);
        if (!strncmp(p, ops[i].t, l)) { *len = (int)l; *prec = ops[i].prec; return (int)i; }
    }
    return -1;
}

static uint32_t ev_expr(struct ev *e, int minprec)
{
    uint32_t l = ev_primary(e);
    for (;;) {
        int len, prec, op;
        ev_ws(e);
        if (e->err[0] || (op = binop(e->p, &len, &prec)) < 0 || prec <= minprec) return l;
        const char *t = e->p;
        e->p += len; e->ops++;
        uint32_t r = ev_expr(e, prec);
        if (e->err[0]) return 0;
        if (!strncmp(t, "||", 2)) l = l || r;
        else if (!strncmp(t, "&&", 2)) l = l && r;
        else if (!strncmp(t, "==", 2)) l = l == r;
        else if (!strncmp(t, "!=", 2)) l = l != r;
        else if (!strncmp(t, "<=", 2)) l = (int32_t)l <= (int32_t)r;
        else if (!strncmp(t, ">=", 2)) l = (int32_t)l >= (int32_t)r;
        else if (!strncmp(t, "<<", 2)) l <<= r & 31;
        else if (!strncmp(t, ">>", 2)) l = (uint32_t)((int32_t)l >> (r & 31));
        else switch (*t) {
            case '|': l |= r; break;
            case '^': l ^= r; break;
            case '&': l &= r; break;
            case '<': l = (int32_t)l < (int32_t)r; break;
            case '>': l = (int32_t)l > (int32_t)r; break;
            case '+': l += r; break;
            case '-': l -= r; break;
            case '*': l *= r; break;
            case '/': if (!r) { ev_fail(e, "Division by zero%s", ""); return 0; } l = (uint32_t)((int32_t)l / (int32_t)r); break;
            default: if (!r) { ev_fail(e, "Division by zero%s", ""); return 0; } l = (uint32_t)((int32_t)l % (int32_t)r); break;
        }
        if (e->ops > 0 && e->kind == K_SYM) e->kind = K_INT;
    }
}

static int eval_ex(const char *s, uint32_t *out, int *kind, int *ops)
{
    struct ev e = { s, "", K_INT, 0 };
    uint32_t v = ev_expr(&e, 0);
    ev_ws(&e);
    if (!e.err[0] && *e.p) ev_fail(&e, "A syntax error in expression, near `%s'.", e.p);
    if (e.err[0]) { puts(e.err); return -1; }
    *out = v;
    if (kind) *kind = e.kind;
    if (ops) *ops = e.ops;
    return 0;
}

static int eval(const char *s, uint32_t *out) { return eval_ex(s, out, NULL, NULL); }

/* ---- formats ---- */

struct fmt { int count, size; char f; };

static int parse_fmt(const char *s, struct fmt *o)
{
    if (isdigit((unsigned char)*s)) { o->count = (int)strtol(s, NULL, 10); while (isdigit((unsigned char)*s)) s++; }
    for (; *s; s++) {
        if (*s == 'b') o->size = 1; else if (*s == 'h') o->size = 2;
        else if (*s == 'w') o->size = 4; else if (*s == 'g') o->size = 8;
        else if (strchr("xduotacfszir", *s)) o->f = *s;
        else { printf("Invalid number \"%s\".\n", s); return -1; }
    }
    return 0;
}

static void print_char_lit(unsigned c)
{
    if (c >= 32 && c < 127 && c != '\'' && c != '\\') printf("'%c'", c);
    else if (c == '\n') fputs("'\\n'", stdout);
    else if (c == '\t') fputs("'\\t'", stdout);
    else if (c == 0) fputs("'\\000'", stdout);
    else printf("'\\%03o'", c);
}

static void print_int_fmt(uint64_t v, char f, int size)
{
    int bits = size * 8;
    uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1);
    v &= mask;
    int64_t sv = (int64_t)(v << (64 - bits)) >> (64 - bits);
    switch (f) {
    case 'x': printf("0x%" PRIx64, v); break;
    case 'z': printf("0x%0*" PRIx64, size * 2, v); break;
    case 'd': printf("%" PRId64, sv); break;
    case 'u': printf("%" PRIu64, v); break;
    case 'o': if (v) printf("0%" PRIo64, v); else putchar('0'); break;
    case 't': {
        int started = 0;
        for (int i = bits - 1; i >= 0; i--) { int b = (v >> i) & 1; if (b) started = 1; if (started) putchar('0' + b); }
        if (!started) putchar('0');
        break; }
    case 'c': printf("%" PRId64 " ", (int64_t)(int8_t)v); print_char_lit((unsigned)(v & 255)); break;
    case 'a': printf("0x%" PRIx64, v); print_sym((uint32_t)v, 0); break;
    default: printf("%" PRId64, sv); break;
    }
}

/* ---- command infrastructure ---- */

typedef void (*cmd_fn)(const char *mod, char *args);
struct cmd { const char *name, *aliases; cmd_fn fn; const char *usage, *brief; };

static int need_live(void)
{
    if (child < 0) { puts("The program is not being run."); return 0; }
    return 1;
}

static int read_bytes(uint32_t addr, void *buf, size_t n)
{
    unsigned char *o = buf;
    for (size_t i = 0; i < n; i++) {
        uint32_t a = addr + (uint32_t)i, w;
        if (peek_word(a & ~3u, &w) < 0) {
            printf("Cannot access memory at address 0x%x\n", (unsigned)a);
            return -1;
        }
        o[i] = (unsigned char)(w >> ((a & 3) * 8));
    }
    return 0;
}

static int write_bytes(uint32_t addr, const void *buf, size_t n)
{
    const unsigned char *in = buf;
    for (size_t i = 0; i < n; i++) {
        uint32_t a = addr + (uint32_t)i, w;
        if (child < 0 || peek_word(a & ~3u, &w) < 0) goto bad;
        w = (w & ~(255u << ((a & 3) * 8))) | ((uint32_t)in[i] << ((a & 3) * 8));
        if (ptrace(PTRACE_POKEDATA, child, (void *)(uintptr_t)(a & ~3u), (void *)(uintptr_t)w) < 0) goto bad;
        continue;
    bad:
        printf("Cannot access memory at address 0x%x\n", (unsigned)a);
        return -1;
    }
    return 0;
}

/* ---- breakpoints ---- */

static struct breakpoint *find_bp_num(int num)
{
    for (size_t i = 0; i < breakpoint_count; i++)
        if (breakpoints[i].num == num) return &breakpoints[i];
    return NULL;
}

static void delete_bp(struct breakpoint *bp)
{
    size_t i = (size_t)(bp - breakpoints);
    if (bp->enabled && child >= 0) (void)poke_text_word(bp->addr, bp->saved);
    memmove(&breakpoints[i], &breakpoints[i + 1], (breakpoint_count - i - 1) * sizeof(breakpoints[0]));
    breakpoint_count--;
    hit_bp = NULL;
}

static struct breakpoint *new_bp(uint32_t addr, int temp, int internal)
{
    struct breakpoint *old = breakpoint_at(addr);
    if (old) {
        if (internal) return old;
        printf("Note: breakpoint %d already set at 0x%08" PRIx32 "; duplicate not created.\n", old->num, addr);
        return NULL;
    }
    if (addr & 3) { puts("Breakpoint addresses must be 4-byte aligned on REM32."); return NULL; }
    if (breakpoint_count == sizeof(breakpoints) / sizeof(breakpoints[0])) { puts("Too many breakpoints."); return NULL; }
    struct breakpoint *bp = &breakpoints[breakpoint_count++];
    memset(bp, 0, sizeof(*bp));
    bp->addr = addr; bp->user_en = 1; bp->temp = temp;
    bp->num = internal ? 0 : next_bp_num++;
    reinsert_disabled_breakpoints();
    return bp;
}

static int parse_location(char *a, uint32_t *addr)
{
    a = skipws(a); trim_end(a);
    if (strstr(a, " if ")) { puts("Conditional breakpoints are not implemented."); return -1; }
    if (!*a) {
        struct rem_user_regs r;
        if (child < 0 || get_regs(&r) < 0) { puts("No default breakpoint address now."); return -1; }
        *addr = r.pc; return 0;
    }
    if (*a == '*') return eval(a + 1, addr);
    if (isdigit((unsigned char)*a)) { puts("No symbol table is loaded.  Use the \"file\" command."); return -1; }
    *addr = find_symbol(a);
    if (!*addr) { printf("Function \"%s\" not defined.\n", a); return -1; }
    return 0;
}

static void break_common(char *args, int temp)
{
    uint32_t addr;
    if (parse_location(args, &addr) < 0) return;
    struct breakpoint *bp = new_bp(addr, temp, 0);
    if (bp) printf("%s %d at 0x%08" PRIx32 "\n", temp ? "Temporary breakpoint" : "Breakpoint", bp->num, addr);
}

static void c_break(const char *m, char *a) { (void)m; break_common(a, 0); }
static void c_tbreak(const char *m, char *a) { (void)m; break_common(a, 1); }

/* GDB-style list parsing: N, N-M, or empty for "all". Returns -1 on error. */
static int each_bp_arg(char *args, void (*fn)(struct breakpoint *))
{
    args = skipws(args); trim_end(args);
    if (!strncmp(args, "breakpoints", 11) && (!args[11] || isspace((unsigned char)args[11]))) args = skipws(args + 11);
    if (!*args) {
        for (size_t i = 0; i < breakpoint_count; ) {
            if (!breakpoints[i].num) { i++; continue; }
            size_t before = breakpoint_count;
            fn(&breakpoints[i]);
            if (breakpoint_count == before) i++;
        }
        return 0;
    }
    for (char *tok = strtok(args, " \t"); tok; tok = strtok(NULL, " \t")) {
        char *dash = strchr(tok, '-');
        long lo, hi;
        if (!isdigit((unsigned char)*tok)) { printf("Bad breakpoint argument: '%s'\n", tok); return -1; }
        lo = strtol(tok, NULL, 10);
        hi = dash && isdigit((unsigned char)dash[1]) ? strtol(dash + 1, NULL, 10) : lo;
        for (long n = lo; n <= hi; n++) {
            struct breakpoint *bp = n > 0 ? find_bp_num((int)n) : NULL;
            if (!bp) printf("No breakpoint number %ld.\n", n); else fn(bp);
        }
    }
    return 0;
}

static void bp_delete_one(struct breakpoint *bp) { delete_bp(bp); }
static void bp_disable_one(struct breakpoint *bp)
{
    bp->user_en = 0;
    if (bp->enabled) { if (poke_text_word(bp->addr, bp->saved) == 0) bp->enabled = 0; }
}
static void bp_enable_one(struct breakpoint *bp) { bp->user_en = 1; }

static void c_delete(const char *m, char *a) { (void)m; each_bp_arg(a, bp_delete_one); }
static void c_disable(const char *m, char *a) { (void)m; each_bp_arg(a, bp_disable_one); }
static void c_enable(const char *m, char *a) { (void)m; each_bp_arg(a, bp_enable_one); reinsert_disabled_breakpoints(); }

static void c_ignore(const char *m, char *a)
{
    (void)m;
    char *n = strtok(a, " \t"), *c = strtok(NULL, " \t");
    uint32_t cnt;
    if (!n) { puts("Argument required (a breakpoint number)."); return; }
    struct breakpoint *bp = find_bp_num(atoi(n));
    if (!bp) { printf("No breakpoint number %s.\n", n); return; }
    if (!c) { puts("Second argument (specified ignore-count) is missing."); return; }
    if (eval(c, &cnt) < 0) return;
    bp->ignore = (int)cnt;
    if (!cnt) printf("Will stop next time breakpoint %d is reached.\n", bp->num);
    else if (cnt == 1) printf("Will ignore next crossing of breakpoint %d.\n", bp->num);
    else printf("Will ignore next %u crossings of breakpoint %d.\n", (unsigned)cnt, bp->num);
}

static void c_info_breakpoints(const char *m, char *a)
{
    (void)m; (void)a;
    int any = 0;
    for (size_t i = 0; i < breakpoint_count; i++) if (breakpoints[i].num) any = 1;
    if (!any) { puts("No breakpoints or watchpoints."); return; }
    puts("Num     Type           Disp Enb Address    What");
    for (size_t i = 0; i < breakpoint_count; i++) {
        struct breakpoint *b = &breakpoints[i];
        if (!b->num) continue;
        printf("%-7d %-14s %-4s %-3c 0x%08" PRIx32, b->num, "breakpoint", b->temp ? "del" : "keep", b->user_en ? 'y' : 'n', b->addr);
        printf("%*s", 1, "");
        uint32_t off; const char *n = symbol_at(b->addr, &off);
        if (n && off) printf("<%s+%u>", n, (unsigned)off);
        else if (n) printf("<%s>", n);
        putchar('\n');
        if (b->hits) printf("\tbreakpoint already hit %u time%s\n", b->hits, b->hits == 1 ? "" : "s");
        if (b->ignore) printf("\tignore next %d hits\n", b->ignore);
    }
}

/* ---- execution control ---- */

static void announce(void)
{
    struct rem_user_regs r;
    if (child < 0 || !child_stopped || stopped_signal != SIGTRAP || get_regs(&r) < 0) return;
    if (hit_bp && hit_bp->num) {
        struct breakpoint *bp = hit_bp;
        bp->hits++;
        printf("\n%s %d, ", bp->temp ? "Temporary breakpoint" : "Breakpoint", bp->num);
        frame_line(r.pc);
        if (bp->temp) delete_bp(bp);
    } else frame_line(r.pc);
    hit_bp = NULL;
}

static void drop_internal_hit(void)
{
    if (hit_bp && !hit_bp->num) { delete_bp(hit_bp); hit_bp = NULL; }
}

static int live_stopped(void)
{
    if (child < 0) { puts("The program is not being run."); return 0; }
    return child_stopped;
}

/* Continue honouring ignore counts; internal temporary stops are the caller's. */
static void do_continue(void)
{
    for (;;) {
        continue_child();
        if (child < 0 || !child_stopped || stopped_signal != SIGTRAP) return;
        if (hit_bp && hit_bp->num && hit_bp->ignore > 0) {
            hit_bp->ignore--; hit_bp->hits++;
            continue;
        }
        return;
    }
}

static int step_once(void)
{
    skip_embedded_break();
    initial_exec_stop = 0;
    if (resume_child(PTRACE_SINGLESTEP) < 0) return -1;
    reinsert_disabled_breakpoints();
    return 0;
}

static int stop_is_plain(void)
{
    return child >= 0 && child_stopped && stopped_signal == SIGTRAP && !hit_bp;
}

static int launch(void)
{
    if (child >= 0) stop_child_for_reload();
    if (child >= 0) return -1;
    run_args[run_argc] = NULL;
    printf("Starting program: %s", run_args[0]);
    for (int i = 1; i < run_argc; i++) printf(" %s", run_args[i]);
    putchar('\n');
    return start_child(run_args[0], run_args);
}

static void set_run_args(char *args)
{
    while (run_argc > 1) free(run_args[--run_argc]);
    for (char *t = strtok(args, " \t"); t && run_argc < 63; t = strtok(NULL, " \t")) run_args[run_argc++] = strdup(t);
    run_args[run_argc] = NULL;
}

static void c_run(const char *m, char *a)
{
    (void)m;
    a = skipws(a);
    if (*a) set_run_args(a);
    if (child >= 0 && initial_exec_stop && !*a) {
        printf("Starting program: %s\n", run_args[0]);
    } else if (launch() < 0) return;
    do_continue();
    announce();
}

static void c_start(const char *m, char *a)
{
    (void)m;
    uint32_t addr = find_symbol("main");
    a = skipws(a);
    if (*a) set_run_args(a);
    if (!addr) { puts("No symbol table loaded.  Use the \"file\" command."); return; }
    struct breakpoint *bp = new_bp(addr, 1, 0);
    if (bp) printf("Temporary breakpoint %d at 0x%08" PRIx32 "\n", bp->num, addr);
    if (child >= 0 && initial_exec_stop && !*a) printf("Starting program: %s\n", run_args[0]);
    else if (launch() < 0) return;
    do_continue();
    announce();
}

static void c_starti(const char *m, char *a)
{
    (void)m;
    a = skipws(a);
    if (*a) set_run_args(a);
    if (launch() < 0) return;
    struct rem_user_regs r;
    puts("\nProgram stopped.");
    if (get_regs(&r) == 0) frame_line(r.pc);
}

static void c_continue(const char *m, char *a)
{
    (void)m;
    if (!live_stopped()) return;
    a = skipws(a);
    if (*a) {
        uint32_t n;
        if (eval(a, &n) < 0) return;
        if (hit_bp && n > 1) {
            hit_bp->ignore = (int)n - 1;
            printf("Will ignore next %u crossings of breakpoint %d.  Continuing.\n", (unsigned)n - 1, hit_bp->num);
        }
    }
    puts("Continuing.");
    do_continue();
    announce();
}

static int is_call(uint32_t w)
{
    unsigned op = w >> 26;
    return op == 6 || (op == 7 && !(w & 0xfffff) && ((w >> 25) & 1));
}

/* Run to ADDR using a temporary internal breakpoint. */
static void run_to(uint32_t addr)
{
    struct breakpoint *old = breakpoint_at(addr);
    int was_disabled = old && !old->user_en;
    struct breakpoint *bp = new_bp(addr, 0, 1);
    if (!bp) return;
    bp->user_en = 1;
    reinsert_disabled_breakpoints();
    do_continue();
    if (was_disabled && (bp = breakpoint_at(addr))) bp_disable_one(bp);
    if (!old) drop_internal_hit();
    if (!old && (bp = breakpoint_at(addr)) && !bp->num) delete_bp(bp);
}

static void c_stepi(const char *m, char *a)
{
    (void)m;
    uint32_t n = 1;
    if (!live_stopped()) return;
    a = skipws(a);
    if (*a && eval(a, &n) < 0) return;
    for (uint32_t i = 0; i < n; i++) {
        if (step_once() < 0) { perror("remdbg: stepi"); return; }
        if (!stop_is_plain()) break;
    }
    announce();
}

static void c_nexti(const char *m, char *a)
{
    (void)m;
    uint32_t n = 1;
    if (!live_stopped()) return;
    a = skipws(a);
    if (*a && eval(a, &n) < 0) return;
    for (uint32_t i = 0; i < n; i++) {
        struct rem_user_regs r; uint32_t w;
        if (get_regs(&r) < 0 || peek_word(r.pc, &w) < 0) { puts("Cannot access memory at current PC."); return; }
        if (is_call(w)) {
            skip_embedded_break();
            initial_exec_stop = 0;
            uint32_t ret = r.pc + 4;
            struct breakpoint *old = breakpoint_at(ret);
            if (!old) {
                /* Step the call itself first, then run to the return address. */
                if (step_once() < 0) { perror("remdbg: nexti"); return; }
                if (!stop_is_plain()) break;
                run_to(ret);
                if (!stop_is_plain() && !(hit_bp == NULL && child >= 0)) break;
            } else {
                if (step_once() < 0) { perror("remdbg: nexti"); return; }
                if (!stop_is_plain()) break;
                run_to(ret);
                if (hit_bp) break;
            }
        } else {
            if (step_once() < 0) { perror("remdbg: nexti"); return; }
            if (!stop_is_plain()) break;
        }
        if (child < 0 || !child_stopped || stopped_signal != SIGTRAP) break;
    }
    announce();
}

static void no_source_step(const char *cmd)
{
    printf("\"%s\" is source-level stepping; remdbg has no DWARF line-table support, so it is not implemented.\n", cmd);
    puts("Use stepi/nexti (instruction level), or break/tbreak/until ADDRESS and continue.");
}

static void c_step(const char *m, char *a) { (void)m; (void)a; no_source_step("step"); }
static void c_next(const char *m, char *a) { (void)m; (void)a; no_source_step("next"); }

static void c_finish(const char *m, char *a)
{
    (void)m; (void)a;
    struct rem_user_regs r;
    if (!live_stopped()) return;
    if (get_regs(&r) < 0) return;
    if (!r.r[14]) { puts("\"finish\" not meaningful in the outermost frame."); return; }
    printf("Run till exit from #0  ");
    frame_line(r.pc);
    run_to(r.r[14]);
    announce();
}

static void c_until(const char *m, char *a)
{
    (void)m;
    uint32_t addr;
    if (!live_stopped()) return;
    a = skipws(a);
    if (!*a) { no_source_step("until"); return; }
    if (parse_location(a, &addr) < 0) return;
    run_to(addr);
    announce();
}

static void c_jump(const char *m, char *a)
{
    (void)m;
    uint32_t addr;
    struct rem_user_regs r;
    if (!live_stopped()) return;
    if (!*skipws(a)) { puts("Argument required (starting address)."); return; }
    if (parse_location(a, &addr) < 0 || get_regs(&r) < 0) return;
    r.pc = addr;
    if (ptrace(PTRACE_SETREGS, child, 0, &r) < 0) { perror("remdbg: jump"); return; }
    printf("Continuing at 0x%08" PRIx32 ".\n", addr);
    stopped_signal = SIGTRAP; own_breakpoint_stop = 1; initial_exec_stop = 0;
    do_continue();
    announce();
}

static void c_kill(const char *m, char *a)
{
    (void)m; (void)a;
    pid_t p = child;
    if (child < 0) { puts("The program is not being run."); return; }
    stop_child_for_reload();
    if (child < 0) printf("[Inferior 1 (process %d) killed]\n", (int)p);
}

static void c_quit(const char *m, char *a) { (void)m; (void)a; should_quit = 1; }

/* ---- data ---- */

static void show_reg_row(const char *name, uint32_t v, int idx)
{
    printf("%-15s", name);
    char hex[16]; snprintf(hex, sizeof(hex), "0x%x", (unsigned)v);
    printf("%-19s", hex);
    if (idx == 16) { printf("0x%x", (unsigned)v); print_sym(v, 0); }
    else if (idx == 13 || idx == 14 || idx == 15) { printf("0x%x", (unsigned)v); if (idx == 14) print_sym(v, 0); }
    else printf("%d", (int)v);
    putchar('\n');
}

static void c_info_registers(const char *m, char *a)
{
    (void)m;
    struct rem_user_regs r;
    if (child < 0 || get_regs(&r) < 0) { puts("The program has no registers now."); return; }
    a = skipws(a); trim_end(a);
    if (!*a) {
        for (int i = 0; i < 16; i++) { char n[8]; snprintf(n, sizeof(n), "r%d", i); show_reg_row(n, r.r[i], i); }
        show_reg_row("pc", r.pc, 16); show_reg_row("sr", r.sr, 17);
        show_reg_row("cause", r.cause, 18); show_reg_row("info", r.info, 19);
        return;
    }
    for (char *t = strtok(a, " \t"); t; t = strtok(NULL, " \t")) {
        const char *n = *t == '$' ? t + 1 : t;
        int idx = reg_index(n, strlen(n));
        if (idx < 0) { printf("Invalid register `%s'\n", n); return; }
        show_reg_row(n, *regslot(&r, idx), idx);
    }
}

static unsigned hist_add(uint32_t v) { history[history_n & 255] = v; return ++history_n; }

static void print_common(const char *mod, char *a, int record)
{
    struct fmt f = { 1, 4, 0 };
    uint32_t v; int kind, ops;
    if (mod && *mod == '/' && parse_fmt(mod + 1, &f) < 0) return;
    a = skipws(a); trim_end(a);
    if (!*a) {
        if (!history_n) { puts("History is empty."); return; }
        v = history[(history_n - 1) & 255]; kind = K_INT; ops = 1;
    } else if (eval_ex(a, &v, &kind, &ops) < 0) return;
    if (record) printf("$%u = ", hist_add(v));
    if (f.f) {
        print_int_fmt(v, f.f, f.size > 4 ? 4 : f.size);
    } else if (kind == K_PC && !ops) {
        printf("(void (*)()) 0x%x", (unsigned)v); print_sym(v, 0);
    } else if (kind == K_PTR && !ops) printf("(void *) 0x%x", (unsigned)v);
    else if (kind == K_SYM && !ops) { printf("{<text variable, no debug info>} 0x%x", (unsigned)v); print_sym(v, 0); }
    else printf("%d", (int)v);
    putchar('\n');
}

static void c_print(const char *m, char *a) { print_common(m, a, 1); }
static void c_output(const char *m, char *a) { print_common(m, a, 0); }

static void c_echo(const char *m, char *a)
{
    (void)m;
    for (char *p = a; *p; p++) {
        if (*p == '\n' && !p[1]) break;
        if (*p == '\\' && p[1]) {
            p++;
            putchar(*p == 'n' ? '\n' : *p == 't' ? '\t' : *p);
        } else if (*p == '\\') break;
        else putchar(*p);
    }
}

static int last_x_size = 4, last_x_count = 1;
static char last_x_fmt = 'x';
static uint32_t last_x_addr;
static int have_last_x;

static void insn_line(uint32_t addr, int mark, int rel, uint32_t func_start)
{
    uint32_t w; char text[128];
    if (peek_word(addr, &w) < 0) { printf("Cannot access memory at address 0x%x\n", (unsigned)addr); return; }
    decode(addr, w, text, sizeof(text));
    if (rel) printf("%s0x%08" PRIx32 " <+%u>:\t%s\n", mark ? "=> " : "   ", addr, (unsigned)(addr - func_start), text);
    else {
        printf("%s0x%x", mark ? "=> " : "   ", (unsigned)addr);
        print_sym(addr, 0);
        printf(":\t%s\n", text);
    }
}

static void c_x(const char *mod, char *a)
{
    struct fmt f = { last_x_count > 0 ? 1 : 1, last_x_size, last_x_fmt };
    int explicit_size = 0;
    uint32_t addr;
    if (mod && *mod == '/') {
        if (parse_fmt(mod + 1, &f) < 0) return;
        for (const char *s = mod + 1; *s; s++) if (strchr("bhwg", *s)) explicit_size = 1;
    }
    if (f.f == 'f') { puts("x/f (floating point) is not implemented: REM32 has no FPU."); return; }
    if (f.f == 'a') f.size = 4;
    else if ((f.f == 'c' || f.f == 's') && !explicit_size) f.size = 1;
    else if (f.f == 'i') f.size = 4;
    a = skipws(a); trim_end(a);
    if (*a) { if (eval(a, &addr) < 0) return; }
    else if (have_last_x) addr = last_x_addr;
    else { puts("Argument required (starting display address)."); return; }
    if (f.count < 1) f.count = 1;
    struct rem_user_regs r;
    uint32_t pc = (child >= 0 && get_regs(&r) == 0) ? r.pc : 0xffffffffu;
    if (f.f == 'i') {
        for (int i = 0; i < f.count; i++, addr += 4) {
            uint32_t w;
            if (peek_word(addr, &w) < 0) { printf("Cannot access memory at address 0x%x\n", (unsigned)addr); break; }
            insn_line(addr, addr == pc, 0, 0);
        }
    } else if (f.f == 's') {
        for (int i = 0; i < f.count; i++) {
            char buf[201]; size_t n = 0;
            printf("0x%x", (unsigned)addr); print_sym(addr, 0); fputs(":\t\"", stdout);
            for (;;) {
                unsigned char c;
                if (read_bytes(addr + (uint32_t)n, &c, 1) < 0) { putchar('\n'); goto done; }
                n++;
                if (!c) break;
                if (c == '\n') fputs("\\n", stdout); else if (c == '\t') fputs("\\t", stdout);
                else if (c == '"' || c == '\\') printf("\\%c", c);
                else if (c < 32 || c > 126) printf("\\%03o", c);
                else putchar(c);
                if (n >= sizeof(buf) - 1) { fputs("\"...", stdout); goto next_s; }
            }
            putchar('"');
        next_s:
            putchar('\n');
            addr += (uint32_t)n;
        }
    } else {
        int per = f.size == 1 ? 8 : f.size == 2 ? 8 : f.size == 4 ? 4 : 2;
        if (f.f == 'c') per = 8;
        for (int i = 0; i < f.count; ) {
            printf("0x%x", (unsigned)addr); print_sym(addr, 0); putchar(':');
            for (int j = 0; j < per && i < f.count; j++, i++) {
                uint64_t v = 0; unsigned char b[8];
                if (read_bytes(addr, b, (size_t)f.size) < 0) { goto done; }
                for (int k = f.size - 1; k >= 0; k--) v = v << 8 | b[k];
                putchar('\t');
                if (f.f == 'x' || f.f == 'z') printf("0x%0*" PRIx64, f.size * 2, v);
                else if (f.f == 'o' && v) printf("0%0*" PRIo64, (f.size * 8 + 2) / 3, v);
                else if (f.f == 't') { for (int k = f.size * 8 - 1; k >= 0; k--) putchar('0' + (int)((v >> k) & 1)); }
                else print_int_fmt(v, f.f == 'r' ? 'x' : f.f, f.size);
                addr += (uint32_t)f.size;
            }
            putchar('\n');
        }
    }
done:
    last_x_addr = addr; have_last_x = 1;
    last_x_size = f.size; last_x_fmt = f.f; last_x_count = f.count;
}

static int func_bounds(uint32_t pc, uint32_t *start, uint32_t *end, const char **name)
{
    uint32_t off;
    const char *n = symbol_at(pc, &off);
    if (!n) return -1;
    *start = pc - off; *name = n;
    uint32_t size = 0, next = 0xffffffffu;
    for (size_t i = 0; i < symbol_count; i++) {
        if (symbols[i].value == *start && symbols[i].size > size) size = symbols[i].size;
        if (symbols[i].value > *start && symbols[i].value < next) next = symbols[i].value;
    }
    *end = size ? *start + size : (next == 0xffffffffu ? *start + 0x40 : next);
    return 0;
}

static void c_disassemble(const char *mod, char *a)
{
    (void)mod;
    uint32_t start, end; const char *name = NULL; int rel = 0;
    struct rem_user_regs r;
    a = skipws(a); trim_end(a);
    if (*a == '/') { while (*a && !isspace((unsigned char)*a)) a++; a = skipws(a); }
    char *comma = strchr(a, ',');
    uint32_t pc = (child >= 0 && get_regs(&r) == 0) ? r.pc : 0xffffffffu;
    if (!*a) {
        if (child < 0) { puts("No frame selected."); return; }
        if (func_bounds(pc, &start, &end, &name) < 0) { puts("No function contains program counter for selected frame."); return; }
        rel = 1;
    } else if (comma) {
        *comma = 0;
        char *e = skipws(comma + 1);
        if (eval(a, &start) < 0) return;
        int plus = *e == '+';
        if (eval(plus ? e + 1 : e, &end) < 0) return;
        if (plus) end += start;
    } else {
        uint32_t v;
        if (eval(a, &v) < 0) return;
        if (func_bounds(v, &start, &end, &name) < 0) { puts("No function contains specified address."); return; }
        rel = 1;
    }
    if (rel) printf("Dump of assembler code for function %s:\n", name);
    else printf("Dump of assembler code from 0x%x to 0x%x:\n", (unsigned)start, (unsigned)end);
    for (uint32_t p = start; p < end; p += 4) {
        if (rel) insn_line(p, p == pc, 1, start);
        else {
            uint32_t w; char text[128];
            if (peek_word(p, &w) < 0) { printf("Cannot access memory at address 0x%x\n", (unsigned)p); return; }
            decode(p, w, text, sizeof(text));
            uint32_t off; const char *n = symbol_at(p, &off);
            printf("%s0x%08" PRIx32, p == pc ? "=> " : "   ", p);
            if (n) printf(" <%s+%u>", n, (unsigned)off);
            printf(":\t%s\n", text);
        }
    }
    puts("End of assembler dump.");
}

static void c_set(const char *m, char *a)
{
    (void)m;
    static const char *const noop[] = { "confirm", "pagination", "width", "height", "listsize", "verbose",
        "print", "disassembly-flavor", "startup-with-shell", "disable-randomization", "style", "logging", "breakpoint", "step-mode" };
    a = skipws(a); trim_end(a);
    if (!strncmp(a, "args", 4) && (!a[4] || isspace((unsigned char)a[4]))) { set_run_args(skipws(a + 4)); return; }
    for (size_t i = 0; i < sizeof(noop) / sizeof(noop[0]); i++) {
        size_t l = strlen(noop[i]);
        if (!strncmp(a, noop[i], l) && (!a[l] || isspace((unsigned char)a[l]))) return;
    }
    if (!strncmp(a, "variable", 8) && isspace((unsigned char)a[8])) a = skipws(a + 8);
    else if (!strncmp(a, "var", 3) && isspace((unsigned char)a[3])) a = skipws(a + 3);
    char *eq = strchr(a, '=');
    while (eq && (eq[1] == '=' || eq[-1] == '=' || eq[-1] == '!' || eq[-1] == '<' || eq[-1] == '>')) eq = strchr(eq + 2, '=');
    if (!eq) { puts("Argument required (expression to compute)."); return; }
    *eq = 0;
    uint32_t v;
    if (eval(eq + 1, &v) < 0) return;
    char *lhs = skipws(a); trim_end(lhs);
    if (*lhs == '$') {
        int idx = reg_index(lhs + 1, strlen(lhs + 1));
        struct rem_user_regs r;
        if (idx < 0) { printf("Left operand of assignment is not a modifiable lvalue.\n"); return; }
        if (child < 0 || get_regs(&r) < 0) { puts("No registers."); return; }
        *regslot(&r, idx) = v;
        if (ptrace(PTRACE_SETREGS, child, 0, &r) < 0) perror("remdbg: set register");
        return;
    }
    if (*lhs == '{') {
        char *close = strchr(lhs, '}');
        uint32_t addr; size_t sz;
        if (!close) { puts("A syntax error in expression, near `'."); return; }
        *close = 0;
        char *t = skipws(lhs + 1); trim_end(t);
        sz = !strcmp(t, "char") ? 1 : !strcmp(t, "short") ? 2 : !strcmp(t, "int") || !strcmp(t, "long") ? 4 : 0;
        if (!sz) { printf("No symbol \"%s\" in current context.\n", t); return; }
        if (eval(close + 1, &addr) < 0) return;
        write_bytes(addr, &v, sz);
        return;
    }
    puts("Left operand of assignment is not an lvalue.");
}

static void c_frame(const char *m, char *a)
{
    (void)m;
    struct rem_user_regs r;
    a = skipws(a); trim_end(a);
    if (child < 0 || get_regs(&r) < 0) { puts("No stack."); return; }
    if (*a && strcmp(a, "0")) { printf("No frame at level %s (only frame 0 is selectable; no unwinder).\n", a); return; }
    printf("#0  ");
    frame_line(r.pc);
}

static void c_bt(const char *m, char *a)
{
    (void)m; (void)a;
    if (child < 0) { puts("No stack."); return; }
    backtrace();
}

static void c_list(const char *m, char *a)
{
    (void)m; (void)a;
    puts("\"list\" is not implemented: remdbg does not read DWARF line tables or source files.");
}

static void c_info_locals(const char *m, char *a) { (void)m; (void)a; if (need_live()) show_locals(); }
static void c_info_source(const char *m, char *a) { (void)m; (void)a; if (need_live()) show_source_location(); }
static void c_info_variables(const char *m, char *a) { (void)m; (void)a; if (need_live()) show_globals(); }

static void c_info_symbol(const char *m, char *a)
{
    (void)m;
    uint32_t v, off;
    if (!*skipws(a)) { puts("Argument required (address)."); return; }
    if (eval(a, &v) < 0) return;
    const char *n = symbol_at(v, &off);
    if (!n) printf("No symbol matches %s.\n", skipws(a));
    else if (off) printf("%s + %u in section .text\n", n, (unsigned)off);
    else printf("%s in section .text\n", n);
}

/* ---- command tables ---- */

static void c_help(const char *m, char *a);
static void c_info(const char *m, char *a);

/* Names of GDB commands not implemented by remdbg (fn == NULL). They exist
 * so that prefix ambiguity matches GDB and the user gets an honest message. */
#define STUB(n) { n, "", NULL, "", "not implemented" }

static const struct cmd main_cmds[] = {
    { "advance", "", c_until, "advance LOCATION", "Continue to LOCATION (temporary breakpoint)." },
    STUB("attach"), STUB("awatch"),
    { "backtrace", "bt where", c_bt, "backtrace", "Print the call chain (return-address heuristics only)." },
    { "break", "b br bre brea", c_break, "break [*ADDRESS | FUNCTION]", "Set a breakpoint." },
    STUB("call"), STUB("catch"), STUB("cd"), STUB("clear"), STUB("commands"), STUB("condition"),
    { "continue", "c fg cont", c_continue, "continue [N]", "Continue the stopped program." },
    STUB("core-file"), STUB("define"),
    { "delete", "d", c_delete, "delete [breakpoints] [N...]", "Delete breakpoints (all if no argument)." },
    STUB("detach"), STUB("directory"),
    { "disable", "dis disa", c_disable, "disable [breakpoints] [N...]", "Disable breakpoints." },
    { "disassemble", "", c_disassemble, "disassemble [START,END | START,+LENGTH | ADDRESS]", "Disassemble REM32 code." },
    STUB("disconnect"), STUB("display"), STUB("down"),
    { "echo", "", c_echo, "echo TEXT", "Print TEXT (supports \\n \\t \\\\)." },
    STUB("edit"),
    { "enable", "en", c_enable, "enable [breakpoints] [N...]", "Enable breakpoints." },
    STUB("exec-file"), STUB("file"),
    { "finish", "fin", c_finish, "finish", "Run until the current function returns (uses LR)." },
    { "frame", "f", c_frame, "frame [0]", "Select/print frame 0." },
    STUB("handle"), STUB("hbreak"),
    { "help", "h", c_help, "help [COMMAND [SUBCOMMAND]]", "Print help." },
    { "ignore", "", c_ignore, "ignore N COUNT", "Ignore a breakpoint COUNT times." },
    { "info", "i inf", c_info, "info SUBCOMMAND", "Show program state (registers, breakpoints, ...)." },
    { "inspect", "", c_print, "inspect[/FMT] EXPRESSION", "Same as print." },
    { "jump", "j", c_jump, "jump *ADDRESS", "Resume execution at ADDRESS." },
    { "kill", "k", c_kill, "kill", "Kill the program being debugged." },
    { "list", "l", c_list, "list", "List source lines (not implemented)." },
    { "next", "n", c_next, "next [N]", "Source-level step over calls (not implemented: no line tables)." },
    { "nexti", "ni", c_nexti, "nexti [N]", "Step one instruction, stepping over calls." },
    { "output", "", c_output, "output[/FMT] EXPRESSION", "Print a value without history or newline." },
    { "print", "p", c_print, "print[/FMT] EXPRESSION", "Print an expression ($r1, $pc, $sp, symbols, arithmetic)." },
    STUB("ptype"),
    { "quit", "q exit", c_quit, "quit", "Exit remdbg." },
    STUB("return"),
    { "run", "r", c_run, "run [ARGS]", "Start (or restart) the program." },
    STUB("search"),
    { "set", "", c_set, "set var $REG|{char|short|int}ADDR = VALUE | set args ARGS", "Assign a register/memory, set arguments." },
    STUB("shell"), STUB("show"), STUB("signal"), STUB("source"),
    { "start", "", c_start, "start [ARGS]", "Run to main with a temporary breakpoint." },
    { "starti", "", c_starti, "starti [ARGS]", "Start the program and stop at its first instruction." },
    { "step", "s", c_step, "step [N]", "Source-level step (not implemented: no line tables)." },
    { "stepi", "si", c_stepi, "stepi [N]", "Execute exactly one machine instruction." },
    STUB("symbol-file"), STUB("target"),
    { "tbreak", "tb", c_tbreak, "tbreak [*ADDRESS | FUNCTION]", "Set a temporary breakpoint." },
    STUB("thread"), STUB("tty"), STUB("undisplay"),
    { "until", "u", c_until, "until LOCATION", "Run until LOCATION is reached (no-argument form is source-level: not implemented)." },
    STUB("up"), STUB("watch"), STUB("whatis"),
    { "x", "", c_x, "x[/NFU] ADDRESS", "Examine memory: N count, F format (x d u o t a c s i), U size (b h w g)." },
};

static const struct cmd info_cmds[] = {
    STUB("address"),
    { "all-registers", "", c_info_registers, "info all-registers", "Same as info registers." },
    STUB("args"),
    { "breakpoints", "b", c_info_breakpoints, "info breakpoints", "Status of breakpoints." },
    STUB("display"), STUB("files"), STUB("frame"), STUB("functions"), STUB("inferiors"), STUB("line"),
    { "locals", "", c_info_locals, "info locals", "Local variables of the current function (uses guest readelf/DWARF)." },
    STUB("program"),
    { "registers", "r", c_info_registers, "info registers [REG...]", "Show REM32 registers r0-r15, pc, sr, cause, info." },
    STUB("sharedlibrary"), STUB("signals"),
    { "source", "", c_info_source, "info source", "Source line of the current PC (uses guest readelf/DWARF)." },
    { "stack", "s", c_bt, "info stack", "Same as backtrace." },
    { "symbol", "", c_info_symbol, "info symbol ADDRESS", "Describe ADDRESS symbolically." },
    STUB("threads"), STUB("types"),
    { "variables", "", c_info_variables, "info variables", "Global variables (uses guest readelf/DWARF)." },
    STUB("watchpoints"),
};

#define NMAIN (sizeof(main_cmds) / sizeof(main_cmds[0]))
#define NINFO (sizeof(info_cmds) / sizeof(info_cmds[0]))

static int alias_match(const char *aliases, const char *w, size_t wl)
{
    const char *p = aliases;
    while (*p) {
        const char *e = p;
        while (*e && *e != ' ') e++;
        if ((size_t)(e - p) == wl && !strncmp(p, w, wl)) return 1;
        p = *e ? e + 1 : e;
    }
    return 0;
}

/* GDB lookup: exact name or predefined alias first, then a unique prefix. */
static const struct cmd *lookup(const struct cmd *tbl, size_t n, const char *w, size_t wl, const char *kind)
{
    const struct cmd *hit = NULL;
    size_t matches = 0;
    for (size_t i = 0; i < n; i++)
        if ((strlen(tbl[i].name) == wl && !strncmp(tbl[i].name, w, wl)) || alias_match(tbl[i].aliases, w, wl)) return &tbl[i];
    for (size_t i = 0; i < n; i++)
        if (!strncmp(tbl[i].name, w, wl)) { hit = &tbl[i]; matches++; }
    if (matches == 1) return hit;
    if (matches > 1) {
        printf("Ambiguous %scommand \"%.*s\": ", kind, (int)wl, w);
        int first = 1;
        for (size_t i = 0; i < n; i++)
            if (!strncmp(tbl[i].name, w, wl)) { printf("%s%s", first ? "" : ", ", tbl[i].name); first = 0; }
        puts(".");
        return NULL;
    }
    if (*kind) printf("Undefined %scommand: \"%.*s\".  Try \"help %s\".\n", kind, (int)wl, w, "info");
    else printf("Undefined command: \"%.*s\".  Try \"help\".\n", (int)wl, w);
    return NULL;
}

static size_t word_len(const char *s)
{
    size_t l = 0;
    while (s[l] && (isalnum((unsigned char)s[l]) || s[l] == '-' || s[l] == '_' || s[l] == '.')) l++;
    return l;
}

static void c_info(const char *m, char *a)
{
    (void)m;
    a = skipws(a);
    size_t l = word_len(a);
    if (!l) {
        puts("List of info subcommands:\n");
        for (size_t i = 0; i < NINFO; i++) if (info_cmds[i].fn) printf("info %s -- %s\n", info_cmds[i].name, info_cmds[i].brief);
        return;
    }
    const struct cmd *c = lookup(info_cmds, NINFO, a, l, "info ");
    if (!c) return;
    if (!c->fn) { printf("\"info %s\" is a GDB command, but it is not implemented in remdbg.\n", c->name); return; }
    c->fn(a + l, skipws(a + l));
}

static void print_cmd_help(const struct cmd *c, const char *prefix)
{
    if (!c->fn) { printf("\"%s%s\" is a GDB command, but it is not implemented in remdbg.\n", prefix, c->name); return; }
    printf("%s\nUsage: %s\n", c->brief, c->usage);
    if (c->aliases[0]) {
        printf("Abbreviations: ");
        for (const char *p = c->aliases; *p; p++) putchar(*p == ' ' ? ',' : *p);
        puts("  (any unambiguous prefix also works, as in GDB)");
    }
}

static void c_help(const char *m, char *a)
{
    (void)m;
    a = skipws(a); trim_end(a);
    if (!*a) {
        puts("remdbg uses the GNU GDB command language (implemented subset). Abbreviations: any unique prefix, plus GDB's\n"
             "predefined ones shown in [brackets]. Type \"help COMMAND\" (e.g. help info registers).\n");
        for (size_t i = 0; i < NMAIN; i++) {
            if (!main_cmds[i].fn) continue;
            printf("%s", main_cmds[i].name);
            if (main_cmds[i].aliases[0]) { printf(" ["); for (const char *p = main_cmds[i].aliases; *p; p++) putchar(*p == ' ' ? ',' : *p); putchar(']'); }
            printf(" -- %s\n", main_cmds[i].brief);
        }
        puts("\nSub-commands of info: registers [r], breakpoints [b], all-registers, locals, source, stack, symbol, variables.\n"
             "x format letters: x d u o t a c s i; sizes b h w g. See docs/REM_DEBUGGER.md for NOT-implemented commands.");
        return;
    }
    size_t l = word_len(a);
    const struct cmd *c = lookup(main_cmds, NMAIN, a, l, "");
    if (!c) return;
    if (c->fn == c_info) {
        char *rest = skipws(a + l);
        size_t l2 = word_len(rest);
        if (l2) {
            const struct cmd *s = lookup(info_cmds, NINFO, rest, l2, "info ");
            if (s) print_cmd_help(s, "info ");
            return;
        }
    }
    print_cmd_help(c, "");
}

static char last_line[512];
static int last_repeatable;

static void execute(char *line)
{
    char *p = skipws(line);
    trim_end(p);
    if (!*p || *p == '#') return;
    size_t l = word_len(p);
    if (!l) { printf("Undefined command: \"%s\".  Try \"help\".\n", p); return; }
    const struct cmd *c = lookup(main_cmds, NMAIN, p, l, "");
    if (!c) return;
    if (!c->fn) { printf("\"%s\" is a GDB command, but it is not implemented in remdbg.\n", c->name); return; }
    char mod[64] = "";
    char *rest = p + l;
    if (*rest == '/') {
        size_t k = 0;
        while (rest[k] && !isspace((unsigned char)rest[k]) && k < sizeof(mod) - 1) { mod[k] = rest[k]; k++; }
        mod[k] = 0; rest += k;
    }
    last_repeatable = !strcmp(c->name, "stepi") || !strcmp(c->name, "nexti") || !strcmp(c->name, "continue") || !strcmp(c->name, "x");
    c->fn(mod, skipws(rest));
}

int main(int argc, char **argv)
{
    char line[512];
    if (argc < 2) { fprintf(stderr, "usage: remdbg PROGRAM [ARG ...]\n"); return 2; }
    run_args[run_argc++] = strdup(argv[1]);
    for (int i = 2; i < argc && run_argc < 63; i++) run_args[run_argc++] = strdup(argv[i]);
    run_args[run_argc] = NULL;
    printf("Reading symbols from %s...\n", argv[1]);
    if (start_child(run_args[0], run_args) < 0) { puts("remdbg: cannot start program"); return 1; }
    puts("(Program is held at its entry point until \"run\"; GDB's \"start\"/\"starti\" semantics are available.)");
    while (!should_quit) {
        fputs("(remdbg) ", stdout); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) { putchar('\n'); break; }
        if (!line[strspn(line, " \t\r\n")] && last_repeatable) strcpy(line, last_line);
        else snprintf(last_line, sizeof(last_line), "%s", line);
        execute(line);
    }
    if (child >= 0) stop_child_for_reload();
    return 0;
}
