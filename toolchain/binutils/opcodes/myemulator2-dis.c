#include "sysdep.h"
#include "disassemble.h"
#include "opcode/myemulator2.h"

static const char *rname(unsigned r)
{
  static char names[16][4];
  static bool init;
  if (!init) { for (unsigned i=0;i<16;i++) snprintf(names[i],4,"r%u",i); init=true; }
  return names[r & 15];
}

static const char *const rnames[] = {"add","adc","sub","sbc","mul","mulh","mulhu","div","divu","rem","remu","and","or","xor","not","sll","srl","sra","rol","ror","seq","sne","slt","sge","sltu","sgeu"};
static const char *const imnames[] = {"addi","subi","andi","ori","xori","slli","srli","srai"};
static const char *const brnames[] = {"beq","bne","blt","bge","bltu","bgeu"};
static const char *const sysnames[] = {"sr","usp","ssp","vbr","ptbr","mmcr","time_lo","time_hi","timecmp_lo","timecmp_hi","tp","dfsp"};

static int32_t sx(uint32_t value, unsigned bits)
{
  uint32_t sign = 1u << (bits - 1);
  return (int32_t)((value ^ sign) - sign);
}

int print_insn_myemulator2(bfd_vma addr, struct disassemble_info *info)
{
  bfd_byte b[4];
  if (info->read_memory_func(addr,b,4,info)) return -1;
  uint32_t w = bfd_getl32(b), op=w>>26, rd=(w>>21)&31, ra=(w>>16)&31, rb=(w>>11)&31;
  fprintf_ftype f=info->fprintf_func; void *s=info->stream;
  if (op==0 && (w&0x3f)==0 && (w>>6&31)<26) f(s,"%s %s,%s,%s",rnames[w>>6&31],rname(rd),rname(ra),rname(rb));
  else if (op==1 && (w>>12&15)<8) f(s,"%s %s,%s,%d",imnames[w>>12&15],rname(rd),rname(ra),(int)(w&0xfff));
  else if (op==2 || op==3) f(s,"%s %s,%d(%s)",op==2?(const char*[]){"lb","lbu","lh","lhu","lw"}[(w>>13)&7]:(const char*[]){"sb","sh","sw"}[(w>>13)&7],rname(rd),(int)(w&0x1fff),rname(ra));
  else if (op==4 && ((w>>23)&7)<6) f(s,"%s %s,%s,0x%x",brnames[w>>23&7],rname(w>>18&31),rname(w>>13&31),(unsigned)(addr + 4 + sx(w & 0x1fff, 13) * 4));
  else if (op==5 || op==6) f(s,"%s 0x%x",op==5?"j":"jal",(unsigned)(addr + 4 + sx(w & 0x3ffffff, 26) * 4));
  else if (op==7 && (w&0xfffff)==0) f(s,"%s %s",w>>25&1?"jalr":"jr",rname(w>>20&31));
  else if (op==8 && !(w&1)) f(s,"lui %s,0x%x",rname(rd),(unsigned)((w>>1)&0xfffff));
  else if (op==9 && !(w&0x7ff)) { unsigned so=w>>22&15; if(so==0) f(s,"mfsr %s,%s",rname(w>>17&31),sysnames[w>>11&31]); else if(so==1) f(s,"mtsr %s,%s",sysnames[w>>11&31],rname(w>>17&31)); else f(s,"%s",so==2?"rfe":so==3?"halt":so==4?"break":".word"); }
  else if (op==10) f(s,"syscall 0x%x",(unsigned)(w&0x3ffffff));
  else if (op==11 && !(w&0xfffff)) f(s,"tlbflush");
  else if (op==12 && !(w&0x7ff)) f(s,"cas %s,%s,0(%s)",rname(rd),rname(w>>16&31),rname(w>>11&31));
  else { f(s,".word 0x%08x",(unsigned)w); return 4; }
  return 4;
}
