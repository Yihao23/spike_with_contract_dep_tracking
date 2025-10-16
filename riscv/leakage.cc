#include "leakage.h"
#include "config.h"
// #include "processor.h"
#include "mmu.h"
#include "disasm.h"
#include "decode_macros.h"
#include <cassert>
#include "platform.h"
// #include "decode.h"



void add_leakage(Leakage &leaks, reg_t npc, insn_t insn, insn_func_t func, processor_t *p)
{
    leaks.add_leak("PC", npc,NULL);
    
    if (insn.opcode()==0b0000011 ) //load addr
        leaks.add_leak("LOAD",insn.i_imm()+RS1,insn.rs1());

    else if(insn.opcode()==0b0100011) //store addr
        leaks.add_leak("STORE",insn.i_imm()+RS1,insn.rs1());

    if(insn.opcode()==0b0000011 && (contract==SEQ_ARCH || contract==TOP)) 
        leaks.add_leak("L-Val",RD,insn.rd()); //load value

    uint8_t taken=0;
    if(insn.opcode()==0b1100011 && (contract==SEQ_CT_B || contract==TOP)) {
        if (insn.funct3() == 0b000){ //beq
            if(RS1 == RS2) 
                taken=1;
            // leaks.add_leak( "taken", taken,insn.rs1());
            // leaks.add_leak( "taken", taken,insn.rs2());
        }
        else if (insn.funct3() == 0b001){ //bne
            if(RS1 != RS2) 
                taken=1;
            // leaks.add_leak( "taken", taken);
        }
        else if (insn.funct3() == 0b100){ //blt
            if(sreg_t(RS1) < sreg_t(RS2))
                taken=1;
            // leaks.add_leak( "taken", taken);
        }
        else if (insn.funct3() == 0b101){ //bge
            if(sreg_t(RS1) >= sreg_t(RS2)) 
                taken=1;
            // leaks.add_leak( "taken", taken);
        }
        else if (insn.funct3() == 0b110){ //bltu
            if(RS1 < RS2)
                taken=1;
            // leaks.add_leak( "taken", taken);
        }
        else if (insn.funct3() == 0b111){ //bgeu
            if(RS1 >= RS2) 
                taken=1;
            // leaks.add_leak( "taken", taken);
        }
        leaks.add_leak( "taken", taken,insn.rs1());
        leaks.add_leak( "taken", taken,insn.rs2());
    }

    if (insn.opcode()==0b0110011 && (contract==SEQ_BM || contract==TOP)) {
        leaks.add_leak("M rs1", RS1, insn.rs1());
        leaks.add_leak("M rs2", RS2, insn.rs2());
    }

}

// void add_leak(struct Leakage *f, const char *location, u_int64_t value/* u_int64_t DepReg*/)
// {
//    struct Leak *l = (struct Leak*)malloc(sizeof *l);
//    strcpy(l->loc,location); 
//    l->leak = value;
// //    l->dep_reg = DepReg;
//    SLIST_INSERT_HEAD(&f->head, l, leaks);
// }

// void print_leaks(FILE *dest, struct Leakage *f)
// {
//     struct Leak *l;
//     fprintf(dest, "Leakage:\n");
//     SLIST_FOREACH(l, &f->head, leaks) {
//         fprintf(dest, "Leak: %s 0x%lx\n", l->loc, l->leak);
//         // save_dependency(l->dep_reg);
//     }
// }
// void delete_leak(struct Leakage *f)
// {
//     struct Leak *b = SLIST_FIRST(&f->head);
//     SLIST_REMOVE_HEAD(&f->head, leaks);
//     free(b);
// }

// void delete_all_leaks(struct Leakage *f)
// {
//     struct Leak *b;
//     while((b = SLIST_FIRST(&f->head))) {
//         SLIST_REMOVE_HEAD(&f->head, leaks);
//         free(b);
//     }
// }

// void init_leaks(struct Leakage *f)
// {
//   SLIST_INIT(&f->head);
// }

