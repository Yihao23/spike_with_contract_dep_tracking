#include "leakage.h"
#include "config.h"
// #include "processor.h"
#include "mmu.h"
#include "disasm.h"
#include "decode_macros.h"
#include <cassert>
#include "platform.h"
// #include "decode.h"
#include <unordered_map>
std::unordered_map <std::string, std::string> contract_templete = {
    {"ADDI", "REG_RS1"},
    {"DIVU", "REG_RS2"}
};


void add_leakage(Leakage &leaks, reg_t npc, insn_t insn, insn_func_t func, processor_t *p)
{
    leaks.add_leak("PC", npc);
    
    if (insn.opcode() == 0b0010011 ) //addi
        leaks.add_leak("ADDI",RS1,insn.rs1());
        //also adding memory addr's dependency in dep_tracker

    if(insn.opcode() == 0b0110011 && (insn.funct3() == 0b101) && (insn.funct7() == 0b0000001)) //divu
        leaks.add_leak("DIVU",RS2, 0, insn.rs2());
 
}

// void add_leakage(Leakage &leaks, reg_t npc, insn_t insn, insn_func_t func, processor_t *p)
// {
//     leaks.add_leak("PC", npc);
    
//     if (insn.opcode()==0b0000011 ) //load addr
//         leaks.add_leak("LOAD",insn.i_imm()+RS1,insn.rs1());
//         //also adding memory addr's dependency in dep_tracker

//     else if(insn.opcode()==0b0100011) //store addr
//         leaks.add_leak("STORE",insn.i_imm()+RS1,insn.rs1());

//     if(insn.opcode()==0b0000011 && (contract==SEQ_ARCH || contract==TOP)) 
//         leaks.add_leak("L-Val",RD,insn.rd()); //load value

//     uint8_t taken=0;
//     if(insn.opcode()==0b1100011 && (contract==SEQ_CT_B || contract==TOP)) {
//         if (insn.funct3() == 0b000){ //beq
//             if(RS1 == RS2) 
//                 taken=1;
//         }
//         else if (insn.funct3() == 0b001){ //bne
//             if(RS1 != RS2) 
//                 taken=1;
//         }
//         else if (insn.funct3() == 0b100){ //blt
//             if(sreg_t(RS1) < sreg_t(RS2))
//                 taken=1;
//         }
//         else if (insn.funct3() == 0b101){ //bge
//             if(sreg_t(RS1) >= sreg_t(RS2)) 
//                 taken=1;
//         }
//         else if (insn.funct3() == 0b110){ //bltu
//             if(RS1 < RS2)
//                 taken=1;
//         }
//         else if (insn.funct3() == 0b111){ //bgeu
//             if(RS1 >= RS2) 
//                 taken=1;
//         }
//         leaks.add_leak( "taken", taken, insn.rs1(), insn.rs2());
//     }

//     if (insn.opcode()==0b0110011 && (contract==SEQ_BM || contract==TOP)) {
//         leaks.add_leak("M rs1", RS1, insn.rs1());
//         leaks.add_leak("M rs2", RS2, insn.rs2());
//     }
// }

