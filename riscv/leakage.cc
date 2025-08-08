#include "leakage.h"
#include "config.h"
// #include "processor.h"
#include "mmu.h"
#include "disasm.h"
#include "decode_macros.h"
#include <cassert>
#include "platform.h"
// #include "decode.h"



void add_leakage(struct Leakage *l, reg_t npc, insn_t insn, insn_func_t func, processor_t *p)
{
    add_leak(l,"PC", npc);
    
    if (insn.opcode()==0b0000011 ) //load addr
        add_leak(l,"LOAD",insn.i_imm()+RS1);

    else if(insn.opcode()==0b0100011) //store addr
        add_leak(l,"STORE",insn.i_imm()+RS1);

    if(insn.opcode()==0b0000011 && (contract==SEQ_ARCH || contract==TOP)) 
        add_leak(l,"L-Val",RD); //load value

    if(insn.opcode()==0b1100011 && (contract==SEQ_CT_B || contract==TOP)) {
        uint8_t taken=0;
        if (insn.funct3() == 0b000){ //beq
            if(RS1 == RS2) 
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b001){ //bne
            if(RS1 != RS2) 
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b100){ //blt
            if(sreg_t(RS1) < sreg_t(RS2))
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b101){ //bge
            if(sreg_t(RS1) >= sreg_t(RS2)) 
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b110){ //bltu
            if(RS1 < RS2)
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b111){ //bgeu
            if(RS1 >= RS2) 
                taken=1;
            add_leak(l, "taken", taken);
        }
    }

    if (insn.opcode()==0b0110011 && (contract==SEQ_BM || contract==TOP)) {
        add_leak(l,"M rs1", RS1);
        add_leak(l,"M rs2", RS2);
    }

}

void add_leak(struct Leakage *f, const char *location, u_int64_t value)
{
   struct Leak *l = (struct Leak*)malloc(sizeof *l);
   strcpy(l->loc,location); 
   l->leak = value;
   SLIST_INSERT_HEAD(&f->head, l, leaks);
}

void print_leaks(FILE *dest, struct Leakage *f)
{
    struct Leak *l;
    fprintf(dest, "Leakage:\n");
    SLIST_FOREACH(l, &f->head, leaks) {
        fprintf(dest, "Leak: %s 0x%lx\n", l->loc, l->leak);
    }
}
void delete_leak(struct Leakage *f)
{
    struct Leak *b = SLIST_FIRST(&f->head);
    SLIST_REMOVE_HEAD(&f->head, leaks);
    free(b);
}

void delete_all_leaks(struct Leakage *f)
{
    struct Leak *b;
    while((b = SLIST_FIRST(&f->head))) {
        SLIST_REMOVE_HEAD(&f->head, leaks);
        free(b);
    }
}

void init_leaks(struct Leakage *f)
{
  SLIST_INIT(&f->head);
}

