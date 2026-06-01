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
#include <fstream>

// Defined in spike_main/spike.cc, toggled by CLI flag --id-atoms.
extern bool emit_id_atoms;
// Independent output file for id-based / imm encoding atoms (see spike.cc).
extern std::ofstream id_dep_out;

// Emit id-based atoms (reg-id encoding bits + format-specific immediate) for
// an instruction directly into the dedicated id_dep_out stream. This bypasses
// the Leakage queue (and therefore the "save only last 1-2 leaks" truncation
// in the value-based dep_tracking path) so every id/imm atom survives.
//
// Output format (one atom per line):
//   PC=0x<pc> atom=<name>-id-rs1 value=0x<regid>
//   PC=0x<pc> atom=<name>-imm-I  value=0x<imm>
static void emit_id_atoms_for_insn(const char *name, insn_t insn, reg_t pc, unsigned xlen) {
    if (!id_dep_out.is_open()) return;

    // Strip the sign-extension spike applies to 32-bit PCs stored in 64-bit reg_t,
    // so RV32 traces print 0x80000484 instead of 0xffffffff80000484.
    if (xlen == 32) pc &= 0xFFFFFFFFull;

    uint32_t opc = insn.opcode();
    bool has_rd = false, has_rs1 = false, has_rs2 = false;
    bool has_f3 = false, has_f7 = false;
    enum { NONE, IMM_I, IMM_S, IMM_B, IMM_U, IMM_J } imm_kind = NONE;

    switch (opc) {
        case 0x33: case 0x3B:                       // R-type (ALU, ALUW) — no imm
            has_rd = has_rs1 = has_rs2 = true;
            has_f3 = has_f7 = true;
            break;
        case 0x13: case 0x1B:                       // I-type ALU (addi/slli/...)
        case 0x03:                                  // I-type Load (lw/lb/...)
        case 0x67:                                  // I-type JALR
            has_rd = has_rs1 = true;
            has_f3 = true;
            imm_kind = IMM_I;
            break;
        case 0x23:                                  // S-type (Store)
            has_rs1 = has_rs2 = true;
            has_f3 = true;
            imm_kind = IMM_S;
            break;
        case 0x63:                                  // B-type (Branch)
            has_rs1 = has_rs2 = true;
            has_f3 = true;
            imm_kind = IMM_B;
            break;
        case 0x37: case 0x17:                       // U-type (LUI/AUIPC)
            has_rd = true;
            imm_kind = IMM_U;
            break;
        case 0x6F:                                  // J-type (JAL)
            has_rd = true;
            imm_kind = IMM_J;
            break;
        default:
            return;  // system/fence/unknown — skip
    }

    auto write_atom = [&](const std::string &atom, uint64_t value) {
        id_dep_out << "PC=0x" << std::hex << pc
                   << " atom=" << atom
                   << " value=0x" << std::hex << value
                   << std::dec << '\n';
    };

    // opcode / funct3 / funct7 encoding atoms — used to compare per-insn-type
    // retire timing across cores (e.g. detect fixed-latency vs data-dependent
    // pipelines for the same opcode class).
    write_atom(std::string(name) + "-op", (uint64_t)opc);
    if (has_f3) write_atom(std::string(name) + "-f3", (uint64_t)insn.funct3());
    if (has_f7) write_atom(std::string(name) + "-f7", (uint64_t)insn.funct7());

    // reg-id atoms
    if (has_rs1) write_atom(std::string(name) + "-id-rs1", (uint64_t)insn.rs1());
    if (has_rs2) write_atom(std::string(name) + "-id-rs2", (uint64_t)insn.rs2());
    if (has_rd)  write_atom(std::string(name) + "-id-rd",  (uint64_t)insn.rd());

    // imm atom (format-specific; cellift end picks mask by suffix)
    switch (imm_kind) {
        case IMM_I: write_atom(std::string(name) + "-imm-I", (uint64_t)insn.i_imm());  break;
        case IMM_S: write_atom(std::string(name) + "-imm-S", (uint64_t)insn.s_imm());  break;
        case IMM_B: write_atom(std::string(name) + "-imm-B", (uint64_t)insn.sb_imm()); break;
        case IMM_U: write_atom(std::string(name) + "-imm-U", (uint64_t)insn.u_imm());  break;
        case IMM_J: write_atom(std::string(name) + "-imm-J", (uint64_t)insn.uj_imm()); break;
        case NONE:  break;
    }
}

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

  // Emit id/imm encoding atoms BEFORE the contract_templete early-return,
  // because id-based taints (auipc/lui rd, imm-U, etc.) are independent of
  // the value-based contract and would otherwise be dropped for insns not
  // in the value contract map.
  if (emit_id_atoms) {
    emit_id_atoms_for_insn(name, insn, npc, p->get_xlen());
  }

  if (contract_templete.find(name) == contract_templete.end()) return;
  
      unsigned xlen = p->get_xlen();
      sreg_t smax = (sreg_t)(((reg_t)1 << (xlen - 1)) - 1);
      sreg_t smin_raw = (sreg_t)((reg_t)1 << (xlen - 1));                                         
      int shift = 64 - xlen;                                                                      
      sreg_t smin = (smin_raw << shift) >> shift;
      reg_t umax = ~(reg_t)0 ;
        printf("smax=0x%016llx smin=0x%016llx (xlen=%u) umax=0x%016llx\n",                                         
         (unsigned long long)smax,                                                            
         (unsigned long long)smin,
         xlen,
         (unsigned long long)umax);
  printf("executing instruction is in the contrct_template::");
  printf("%llx\n",npc);
  switch (insn.opcode()){   
    case 0x03: /*load rd rs1 imm*/
    {
      leaks.add_leak(name, insn.i_imm()+RS1, insn.rs1());
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
      leaks.add_leak(name, insn.s_imm()+RS1, insn.rs1());
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
      // if (insn.rs2() == insn.rs1()){
        // break;
      // }

      uint8_t taken=0;
      if (insn.funct3() == 0b000){//beq
          if(RS1 == RS2) 
              taken=1;
      }
      else if (insn.funct3() == 0b001){ //bne
          if(RS1 != RS2) 
              taken=1;
      }
      else if (insn.funct3() == 0b100){ //blt
          if(sreg_t(RS1) == smax || sreg_t(RS2) == smin) {
          dep_tracker.print_branch_dependencies(std::string(name)+"-rs1", insn.rs1(), insn.rs2(), dep_out);
          dep_tracker.print_branch_dependencies(std::string(name)+"-rs2", insn.rs1(), insn.rs2(), dep_out);
          break; 
          }
          if(sreg_t(RS1) < sreg_t(RS2))
              taken=1;
      }
      else if (insn.funct3() == 0b101){ //bge
          if(sreg_t(RS1) == smax || sreg_t(RS2) == smin) {
          dep_tracker.print_branch_dependencies(std::string(name)+"-rs1", insn.rs1(), insn.rs2(), dep_out);
          dep_tracker.print_branch_dependencies(std::string(name)+"-rs2", insn.rs1(), insn.rs2(), dep_out);
          break; 
          }
          if(sreg_t(RS1) >= sreg_t(RS2)) 
              taken=1;
      }
      else if (insn.funct3() == 0b110){ //bltu
          if(reg_t(RS1) == umax || RS2 == 0) {
            dep_tracker.print_branch_dependencies(std::string(name)+"-rs1", insn.rs1(), insn.rs2(), dep_out);
            dep_tracker.print_branch_dependencies(std::string(name)+"-rs2", insn.rs1(), insn.rs2(), dep_out);
            break; 
          }
          if(RS1 < RS2)
              taken=1;
      }
      else if (insn.funct3() == 0b111){ //bgeu
          if(reg_t(RS1) == umax || RS2 == 0) {
            dep_tracker.print_branch_dependencies(std::string(name)+"-rs1", insn.rs1(), insn.rs2(), dep_out);
            dep_tracker.print_branch_dependencies(std::string(name)+"-rs2", insn.rs1(), insn.rs2(), dep_out);
            break; 
          }
          if(RS1 >= RS2) 
              taken=1;
      }
      if (insn.rs1() != 0) {
        leaks.add_leak(std::string(name)+"-rs1", RS1, insn.rs1());
      }
      if (insn.rs2() != 0) {
        leaks.add_leak(std::string(name)+"-rs2", RS2, 0,insn.rs2());
      }
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

