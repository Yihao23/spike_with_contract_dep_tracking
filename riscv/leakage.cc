#include "leakage.h"
#include "config.h"
#include "processor.h"
#include "mmu.h"
#include "disasm.h"
#include "decode_macros.h"
#include <cassert>
#include "platform.h"



void add_leakage(struct Leakage *l, reg_t npc, insn_t insn, insn_func_t func)
{
    add_leak(l,"PC", npc);
    
    if (insn.opcode==0b0000011 ) //load addr
        add_leak(l,"laddr",insn.i_imm()+insn.rs1());

    else if(insn.opcode==0b0100011) //store addr
        add_leak(l,"saddr",insn.i_imm()+insn.rs1());

    else if(insn.opcode==0b0000011 && (contract==SEQ_ARCH || contract==TOP)) 
        add_leak(l,"lval",insn.rd()); //load value

    else if(insn.opcode==0b1100011 && (contract==SEQ_CT_B || contract==TOP)) {
        uint8_t taken=0;
        if (insn.funct3() == 0b000){ //beq
            if(insn.rs1() == insn.rs2()) 
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b001){ //bne
            if(insn.rs1() != insn.rs2()) 
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b100){ //blt
            if(sreg_t(insn.rs1()) < sreg_t(insn.rs2()))
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b101){ //bge
            if(sreg_t(insn.rs1()) >= sreg_t(insn.rs2())) 
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b110){ //bltu
            if(insn.rs1() < insn.rs2())
                taken=1;
            add_leak(l, "taken", taken);
        }
        else if (insn.funct3() == 0b111){ //bgeu
            if(insn.rs1() >= insn.rs2()) 
                taken=1;
            add_leak(l, "taken", taken);
        }
    }

    else if(insn.opcode==0b0110011 && (contract==SEQ_BM || contract==TOP)) {
        add_leak(l,"rs1", insn.rs1());
        add_leak(l,"rs2", insn.rs2());
    }

}

