#include "dep.h"
#include "leakage.h"

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
  // if (A->name != Target::ACCU && A->name != Target::ACCU_PC && A->name != target){
  //   return false; // two different targets in same accumulation
  //   std::cout<<"target prob\n";
  // }
  A->name = target;

  // build position
  prog_position p = cur_pos(source_pos);

  if (target == Target::NONE) {
    return true;
  }
  //store
  // if (target == Target::MEM) {
  //   A->
  // }

  if (source == Target::IMM) {
    A->dep_pos[IMM_INDEX] = p;
    return true;
  }

  if (source == Target::MEM) {
   // addr should be added by track mem for now
    return false;
  }


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
  auto* A = accu();
  A->name = target;

  // ---------- LOAD ----------
  if (source == Target::MEM) {
    for (uint8_t i = 0; i < width; i++) {
      auto& me = get_mem(addr + i);
      A->local_bytes[i] = me.addr;
      add_initials_from(me.cur.get(), A);                
      A->dep_pos[BYTE1_INDEX + i] = cur_pos(BYTE1_INDEX + i);
    }
    return true;
  }

  // ---------- STORE ----------
  if (target == Target::MEM) {
    for (uint8_t i = 0; i < width && i < 8; i++) {
      last_bytes_[i] = &get_mem(addr + i);              
    }
    mem_used_ = true;

    if (!A->dep_pos[PC_INDEX].has_value()) {
      A->current_deps[PC_INDEX] = Target::PC;
      A->dep_pos[PC_INDEX]      = cur_pos(PC_DEP);       
      add_initials_from(vault_[to_i(Target::PC)].get(), A); 
    }
    if (!A->dep_pos[IMM_INDEX].has_value())
      A->dep_pos[IMM_INDEX] = cur_pos(IMM_INDEX);

    auto place = [&](Target s, uint16_t slot_idx) {
      if (A->current_deps[slot_idx] == Target::NONE) {
        A->current_deps[slot_idx] = s;
        A->dep_pos[slot_idx] = cur_pos(slot_idx);   
      }
      add_initials_from(vault_[to_i(s)].get(), A);       
    };

    if (source != Target::MEM && source != Target::NONE) {
      if (A->current_deps[RS1_INDEX] == Target::NONE) place(source, RS1_INDEX);
      else if (A->current_deps[RS2_INDEX] == Target::NONE) place(source, RS2_INDEX);
      else place(source, RS3_INDEX);
    }

    // weak_dependency w{};
    // if (source != Target::MEM) { w.is_memory = false; w.reg = source; }
    // else { w.is_memory = true; for (uint8_t i=0;i<width && i<8;i++) w.mem_addrs[i] = addr+i; }

    // for (auto& kv : mem_) {
    //   if (kv.first < addr || kv.first >= addr + width)    
    //     kv.second.cur->weak_deps.push_back(w);
    // }
    // remaining_mem_->weak_deps.push_back(w);
    return true;
  }
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

void Dep_tracker::save_req_dependencies_on_file(Leak &cur_leak, std::ostream& dep_file){

  std::bitset<NBR_OF_ACTUAL_DEPENDENCIES> need_reg; // X/F/CSR indices 0..93
  std::vector<uint64_t> need_mem;
  need_mem.reserve(256);

  auto add_mem_addr = [&](uint64_t a) {
    if (std::find(need_mem.begin(), need_mem.end(), a) == need_mem.end())
      need_mem.push_back(a);
  };

  for (auto start_idx: {cur_leak.dep_reg1, cur_leak.dep_reg2}){
    //cause most of the times dep reg2 is null.
    if (start_idx == NULL) break; 

    // only for safety
    // if (start_idx >= NUMBER_OF_DEPENDENCIES || !vault_[start_idx]) {
    //   dep_file << "Xregs:\nFregs:\nCSRs:\nMemory:\n";
    //   return;
    // }

    std::vector<snapshot*> stack;
    stack.reserve(128);
    std::set<snapshot*> seen;

    auto push = [&](snapshot* s) {
      if (!s) return;
      if (seen.insert(s).second)
        stack.push_back(s);
    };

    push(vault_[start_idx].get());

    while (!stack.empty()) {
      snapshot* s = stack.back();
      stack.pop_back();

      //problem is here
      // need_reg |= s->initial_regs;
      // for (auto a : s->initial_mem) add_mem_addr(a);

      for (auto t : s->current_deps) {
        if (t == Target::NONE || t == Target::IMM || t == Target::MEM) continue;
        uint64_t i = to_i(t);
        if (i < vault_.size() && vault_[i]) push(vault_[i].get());
      }
      
      //and here
      //if having mem dependency
      for (const auto& dep_mem : s->local_bytes) {
        if (!dep_mem) continue;                      
        auto it = mem_.find(*dep_mem);
        if (it != mem_.end()){
          if (it->second.cur->name != Target::NONE)
            push(it->second.cur.get());
          else 
            add_mem_addr(it->first);
            for (auto a : it->second.cur->initial_mem) add_mem_addr(a);
            // dep_file<< "here\n";

        }
      }

      //I think not necessary
      // if (s->prev) push(s->prev.get());
    }

    // dep_file << "Xregs:\n";
    // for (uint16_t i = 0; i < OFFSET_TO_FREGS; ++i)
    //   if (need_reg.test(i)) dep_file << "R" << i << "\n";

    // dep_file << "Fregs:\n";
    // for (uint16_t i = OFFSET_TO_FREGS; i < OFFSET_TO_CSRS; ++i)
    //   if (need_reg.test(i)) dep_file << "F" << (i - OFFSET_TO_FREGS) << "\n";

    // dep_file << "CSRs:\n";
    // for (uint16_t i = OFFSET_TO_CSRS; i < NBR_OF_ACTUAL_DEPENDENCIES; ++i)
    //   if (need_reg.test(i)) dep_file << (i - OFFSET_TO_CSRS) << "\n";

    std::sort(need_mem.begin(), need_mem.end());
    // dep_file << "Memory:\n";
    for (auto a : need_mem) dep_file << a << "\n";
  }
}




void add_dependency(Dep_tracker &dep_tracker, reg_t pc, insn_t insn, processor_t* p, std::ostream& dep_file)
{
  // dep_file<< "inst args:"<< "pc="<< pc <<", rs1="<< insn.rs1() <<", rs2="<< insn.rs2() <<", rd="<< insn.rd() <<"\n";
  // for now we don't support fp regs. Later we should lift_to_fp first.
  Target rs1 = to_T(insn.rs1());
  Target rs2 = to_T(insn.rs2());
  Target rd  = to_T(insn.rd());

  switch (insn.opcode()){

    /*arth- reg*/
    case 0b0110011:
    {
      // dep_file<< "arth-reg\n";
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
      // dep_file<< "arth-imm\n";
      // dep_file<<"imm="<< insn.i_imm() <<"\n";
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
      // dep_file<< "load\n";
      // dep_file<<"imm="<< insn.i_imm() <<"\n";
      // dep_file<<"rs1_val="<< RS1 <<"\n";
        // Target rs1 = to_T(insn.rs1());
        // Target rd  = to_T(insn.rd());
        uint64_t width = 1 << ((insn.funct3() & 0b11) ); // 0=byte,1=half,2=word,3=double
        if (insn.funct3() == 0b100) width = 1; // LBU
        if (insn.funct3() == 0b101) width = 2; // LHU
        if (insn.funct3() == 0b110) width = 4; // LWU
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
      // dep_file<< "store\n";
      // dep_file<<"imm="<< insn.s_imm() <<"\n";
      // dep_file<<"rs1_val="<< RS1 <<"\n";
        uint64_t width = 1 << ((insn.funct3() & 0b11) ); // 0=byte,1=half,2=word,3=double
        if (insn.funct3() == 0b100) width = 1; // SB
        if (insn.funct3() == 0b101) width = 2; // SH
        if (insn.funct3() == 0b110) width = 4; // SW
        if (insn.funct3() == 0b111) width = 8; // SD
        dep_tracker.track_memory(Target::MEM, rs1, insn.s_imm()+RS1, width,0);
        dep_tracker.track_memory(Target::MEM, rs2, insn.s_imm()+RS2, width,0);
        dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
      break;
    }
    /*jal*/
    case 0b1111111:
    {
      // dep_file<< "jal\n";
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
      dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false);
      break;
    }
    /*jalr*/  
    case 0b1110111:
    {
      // dep_file<< "jalr\n";
      // Target rs1 = to_T(insn.rs1());
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
      dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false);
      dep_tracker.track_dependency(Target::PC, rs1, RS1_INDEX, 0b11, false);
      break;
    }
    /*branch*/
    case 0b1100011:
    {
      // dep_file<< "branch\n";
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

