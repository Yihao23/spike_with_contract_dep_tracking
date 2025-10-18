#include "dep.h"

//add each index as dep
//add that index's previous deps
//check imm
// test current
//debug
//refine


bool Dep_tracker::track_dependency(Target target, Target source,
                        uint16_t source_pos,
                        uint8_t int_float_relation,
                        bool csr_implicit){
  //pc_ = pc;
  csr_impl_ = csr_implicit;
  // target = lift_to_fp(int_float_relation, target, true);
  // source = lift_to_fp(int_float_relation, source, false);

  auto* A = (target==Target::PC) ? accu_pc() : accu();
  if (A->name != Target::ACCU && A->name != Target::ACCU_PC && A->name != target){
    return false; // two different targets in same accumulation
    std::cout<<"target prob\n";
  }
  A->name = target;

  // build position
  prog_position p = cur_pos(source_pos);

  if (target == Target::NONE) {
    return true;
  }

  if (source == Target::IMM) {
    A->dep_pos[IMM_INDEX] = p;
    return true;
  }

  if (source == Target::MEM) {
   // addr should be added by track mem
    return false;
  }

  // place into RS1/RS2/RS3/PC
  auto slot = (source == Target::PC) ? PC_INDEX
             : (A->current_deps[RS1_INDEX] == Target::NONE ? RS1_INDEX
             : (A->current_deps[RS2_INDEX] == Target::NONE ? RS2_INDEX
             : (A->current_deps[RS3_INDEX] == Target::NONE ? RS3_INDEX : 0xFFFF)));

  if (slot == 0xFFFF) return false;

  A->current_deps[slot] = source;
  A->dep_pos[slot] = p;

  // propagate initial deps
  add_initials_from(vault_[to_i(source)].get(), A);
  return true;
}

bool Dep_tracker::track_memory(Target target, Target source,
                              uint64_t addr, uint8_t width,
                              uint8_t ir)
{
  // target = lift_to_fp(ir, target, true);
  // source = lift_to_fp(ir, source, false);

  auto* A = accu();
  if (A->name!=Target::ACCU && A->name!=target) return false;
  A->name = target;

  // LOAD
  if (source == Target::MEM) {
    for (uint8_t i=0;i<width && i<8;i++){
      auto& me = get_mem(addr+i);
      A->local_bytes[i] = me.addr;
      // byte contributes its deps
      add_initials_from(me.cur.get(), A);
      A->dep_pos[BYTE1_INDEX + i] = cur_pos(BYTE1_INDEX + i);
    }
    return true;
  }

  // STORE
  if (target == Target::MEM) {
    // remember which bytes for commit_target
    for (uint8_t i=0;i<width && i<8;i++)
      last_bytes_[i] = &get_mem(addr+i);

    // add weak deps to other memory byte and remaining memory
    weak_dependency w{};
    if (source != Target::MEM) {
      w.is_memory = false;
      w.reg = source;
    } else {
      w.is_memory = true;
      for (uint8_t i=0;i<width && i<8;i++)
        w.mem_addrs[i] = addr+i;
    }
    for (auto& kv : mem_) {
      if (kv.first < addr || kv.first >= addr+width)
        kv.second.cur->weak_deps.push_back(w);
    }
    remaining_mem_->weak_deps.push_back(w);
    mem_used_ = true;
    return true;
  }

  // Other (e.g., REG <- REG with memory positions previously set by loads)
  return true;
}

//
bool Dep_tracker::commit_target(){
  commit_one_accu(accu());
  commit_one_accu(accu_pc());
  last_bytes_.fill(nullptr);
  mem_used_ = false;
  csr_impl_ = false;
  return true;
}

bool Dep_tracker::next_instruction(reg_t new_pc){
  pc_ = new_pc;
  ++instr_;
  return true;
}

void Dep_tracker::finish(std::ostream& out){
  // minimal report: list each reg with its latest direct deps
  // out << "=== Dependencies ===\n";
  // for (uint16_t i=0;i<NBR_OF_ACTUAL_DEPENDENCIES;i++){
  //   auto* s = vault_[i].get();
  //   if (!s || s->name==Target::ACCU || s->name==Target::ACCU_PC) continue;
  //   out << "T" << i << ": ";
  //   bool first = true;
  //   for (auto t : s->current_deps){
  //     if (t==Target::NONE) continue;
  //     if (!first) out << ", ";
  //     out << "-> T" << to_i(t);
  //     first = false;
  //   }
  //   if (first) out << "(self/initial)";
  //   out << "\n";
  // }
  out << "=== Memory ===\n";
  for (auto& [addr, me] : mem_){
    out << std::dec << addr << std::dec << "\n";
    // auto* s = me.cur.get();
    // bool first = true;
    // for (auto t : s->current_deps){
    //   if (t==Target::NONE) continue;
    //   if (!first) out << ", ";
    //   out << "-> T" << to_i(t);
    //   first = false;
    // }
    // if (first) out << "(initial)";
    // out << "\n";
  }
}

void Dep_tracker::save_req_dependencies_on_file(Leak &cur_leak, std::ostream& dep_file){
  //
  
}


void add_dependency(Dep_tracker &dep_tracker, reg_t pc, insn_t insn, processor_t* p)
{
  // for now we don't support fp regs. Later we should lift_to_fp first.
  Target rs1 = to_T(insn.rs1());
  Target rs2 = to_T(insn.rs2());
  Target rd  = to_T(insn.rd());

  switch (insn.opcode()){

    /*arth- reg*/
    case 0b0110011:
    {
      if (insn.rd() != 0) {
        dep_tracker.track_dependency(rd, rs1, RS1_INDEX, 0b11, false);
        dep_tracker.track_dependency(rd, rs2, RS2_INDEX, 0b11, false);
        dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false);
      }
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
      break;
    }
  
    /*arth- imm*/
    case 0b0010011:
    {
        // Target rs1 = to_T(insn.rs1());
        // Target rd  = to_T(insn.rd());
        if (insn.rd() != 0) {
          dep_tracker.track_dependency(rd, rs1, RS1_INDEX, 0b11, false);
          dep_tracker.track_dependency(rd, Target::IMM, IMM_INDEX, 0b11, false);
          dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false);
        }
        dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
      break;
    }
    /*load*/
    case 0b0000011:
    {
        // Target rs1 = to_T(insn.rs1());
        // Target rd  = to_T(insn.rd());
        uint64_t width = 1 << ((insn.funct3() & 0b11) ); // 0=byte,1=half,2=word,3=double
        if (insn.funct3() == 0b100) width = 4; // LBU
        if (insn.funct3() == 0b101) width = 4; // LHU
        if (insn.funct3() == 0b110) width = 8; // LWU
        if (insn.funct3() == 0b111) width = 8; // LDWU
        dep_tracker.track_dependency(rd, rs1, RS1_INDEX, 0b11, false);
        dep_tracker.track_memory(rd, Target::MEM, insn.i_imm()+RS1, width,0);
        dep_tracker.track_dependency(rd, Target::IMM, IMM_INDEX, 0b11, false);
        dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
      break;
    }
  
    /*store*/
    case 0b0100011:
    {
        // Target rs1 = to_T(insn.rs1());
        // Target rs2 = to_T(insn.rs2());
        uint64_t width = 1 << ((insn.funct3() & 0b11) ); // 0=byte,1=half,2=word,3=double
        if (insn.funct3() == 0b100) width = 4; // SB
        if (insn.funct3() == 0b101) width = 4; // SH
        if (insn.funct3() == 0b110) width = 8; // SW
        if (insn.funct3() == 0b111) width = 8; // SD
        dep_tracker.track_memory(Target::MEM, rs2, insn.s_imm()+RS1, width,0);
        dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
      break;
    }
    /*jal*/
    case 0b1111111:
    {
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
      dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false);
      break;
    }
    /*jalr*/  
    case 0b1110111:
    {
      // Target rs1 = to_T(insn.rs1());
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
      dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false);
      dep_tracker.track_dependency(Target::PC, rs1, RS1_INDEX, 0b11, false);
      break;
    }
    /*branch*/
    case 0b1100011:
    {
        // Target rs1 = to_T(insn.rs1());
        // Target rs2 = to_T(insn.rs2());
        dep_tracker.track_dependency(Target::PC, rs1, RS1_INDEX, 0b11, false);
        dep_tracker.track_dependency(Target::PC, rs2, RS2_INDEX, 0b11, false);

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
        if (taken)
          dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false);

      break;
    }

    default:
    {
      break;
    }
  }
  dep_tracker.commit_target();
}

