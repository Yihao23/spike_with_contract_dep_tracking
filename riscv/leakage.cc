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
    {"addi", "REG_RS1"},
    {"slti", "REG_RS1"},
    {"sltiu", "REG_RS1"},
    {"xori", "REG_RS1"},
    {"ori", "REG_RS1"},
    {"andi", "REG_RS1"},
    {"slli", "REG_RS1"},
    {"srli", "REG_RS1"},
    {"srai", "REG_RS1"},

    {"beq", "REG_RS2"},
    {"bne", "REG_RS2"},
    {"blt", "REG_RS2"},
    {"bge", "REG_RS2"},
    {"bltu", "REG_RS2"},
    {"bgeu", "REG_RS2"},

    {"jalr", "REG_RS1"},
    {"jal", "REG_PC"},

    {"lb", "REG_RS1"},
    {"lh", "REG_RS1"},
    {"lw", "REG_RS1"},
    {"lbu", "REG_RS1"},
    {"lhu", "REG_RS1"},

    {"ld", "REG_RS1"},
    {"lwu", "REG_RS1"},

    {"sb", "REG_RS2"},
    {"sh", "REG_RS2"},
    {"sw", "REG_RS2"},

    {"sd", "REG_RS2"},

    {"add", "REG_RS2"},
    {"sub", "REG_RS2"},
    {"sll", "REG_RS2"},
    {"slt", "REG_RS2"},
    {"sltu", "REG_RS2"},
    {"xor", "REG_RS2"},
    {"srl", "REG_RS2"},
    {"sra", "REG_RS2"},
    {"or", "REG_RS2"},
    {"and", "REG_RS2"},

    {"mul", "REG_RS2"},
    {"mulh", "REG_RS2"},
    {"mulhu", "REG_RS2"},
    {"mulhsu", "REG_RS2"},
    {"div", "REG_RS2"},
    {"divu", "REG_RS2"},
    {"rem", "REG_RS2"},
    {"remu", "REG_RS2"},
};


void add_leakage(Leakage &leaks, reg_t npc, insn_t insn, insn_func_t func, processor_t *p, Dep_tracker &dep_tracker)
{

  leaks.add_leak("PC", npc);  
  auto* di = p->get_disassembler()->lookup(insn)->get_name();
  if(!di) return;
  const char* name = p->get_disassembler()->lookup(insn)->get_name();
  printf("mnemonic name: %s\n",name);
  if (contract_templete.find(name) == contract_templete.end()) return;
  
  printf("executing instruction is in the contrct_template::");
  printf("%llx\n",npc);
  switch (insn.opcode()){   
    case 0x03: /*load rd rs1 imm*/
    {
      leaks.add_leak("LOAD",insn.i_imm()+RS1,insn.rs1());
      break;
    }  
    case 0x0f:/*fence , fence iorw, iorw*/
    {
        break;
    }
    case 0x13: /*op-imm rd rs1 imm*/
    {
        leaks.add_leak(name, RS1, insn.rs1());
      
      break;
    }
    case 0x17: /*auipc, auipc rd, imm20*/
    {
      if (insn.rd() != 0)
      {
      }
      break;
    }
    case 0x23:  /*store rs2 imm(rs1)*/
    {
      leaks.add_leak("STORE",insn.i_imm()+RS1,insn.rs1());
      break;
    }
    case 0x33:   /*op rd rs1 rs2*/
    {
      if (insn.rs1() != 0) {
        leaks.add_leak(std::string(name)+"-rs1", RS1, insn.rs1());
      }
      if(insn.rd() == insn.rs1() && insn.rd() !=0){
        dep_tracker.print_orignal_dependencies(std::string(name)+"-rs1", insn.rs1(), dep_out);
      }
      if (insn.rs2() != 0) {
      leaks.add_leak(std::string(name)+"-rs2", RS2, 0,insn.rs2());
      }
      if(insn.rd() == insn.rs2() && insn.rd() !=0){
        dep_tracker.print_orignal_dependencies(std::string(name)+"-rs2", insn.rs2(), dep_out);
      }
      break;
    }     
    case 0x37:/*lui, no leakage for lui*/
    {
        if (insn.rd() != 0) {
        }
        break;
    }
    case 0x63: /*branch,rs1, rs2, off*/
    {
      if (insn.rs2() != insn.rs1()){
      }
      uint8_t taken=0;
      if (insn.funct3() == 0b000){ //beq
          if(RS1 == RS2) 
              taken=1;
      }
      else if (insn.funct3() == 0b001){ //bne
          if(RS1 != RS2) 
              taken=1;
      }
      else if (insn.funct3() == 0b100){ //blt
          if(sreg_t(RS1) < sreg_t(RS2))
              taken=1;
      }
      else if (insn.funct3() == 0b101){ //bge
          if(sreg_t(RS1) >= sreg_t(RS2)) 
              taken=1;
      }
      else if (insn.funct3() == 0b110){ //bltu
          if(RS1 < RS2)
              taken=1;
      }
      else if (insn.funct3() == 0b111){ //bgeu
          if(RS1 >= RS2) 
              taken=1;
      }
      leaks.add_leak(name, taken, insn.rs1(), insn.rs2());
      break;
    }
    case 0x67: /*jalr rd, rs1, imm */  
    {
      if (insn.rd() != 0){
      }
      leaks.add_leak(name, insn.i_imm()+RS1, insn.rs1());
      break;
    }
    case 0x6f:/*jal rd, off*/
    {
        if (insn.rd() != 0)
        {
        }
        leaks.add_leak(name, insn.i_imm());
        break;
    }
    case 0x73: /*system*/
    {
        break;
    }
    default: /*unsupported instruction*/
    {
        break;
    }
  }
  return;
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

