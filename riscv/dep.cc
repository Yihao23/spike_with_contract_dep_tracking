#include "dep.h"
#include "leakage.h"

// TODO: populate and propagate weak dependencies 


bool Dep_tracker::track_dependency(Target target, Target source,
                        uint16_t source_pos,
                        uint8_t int_float_relation,
                        bool csr_implicit, INIT_STATE state){

  csr_impl_ = csr_implicit;

  // Choose the active accumulator: PC has its own, everything else uses ACCU.
  auto* A = (target==Target::PC) ? accu_pc() : accu();
  // if (A->name != Target::ACCU && A->name != Target::ACCU_PC && A->name != target){
  //   return false; // two different targets in same accumulation
  //   std::cout<<"target prob\n";
  // }
  A->name = target;

  // build position
  prog_position p = cur_pos(source_pos);

  // Trivial cases
  if (target == Target::NONE) {
    return true;
  }

  if (source == Target::IMM) {
    A->dep_pos[IMM_INDEX] = p;
    return true;
  }

  if (source == Target::MEM) {
   // addr should be added by track mem for now
    return false;
  }

  // Place the source into the next free slot among RS1/RS2/RS3/PC.
  auto slot = (source == Target::PC) ? PC_INDEX
             : (A->current_deps[RS1_INDEX] == Target::NONE ? RS1_INDEX
             : (A->current_deps[RS2_INDEX] == Target::NONE ? RS2_INDEX
             : (A->current_deps[RS3_INDEX] == Target::NONE ? RS3_INDEX : 0xFFFF)));

  if (slot == 0xFFFF) return false;

  A->current_deps[slot] = source;
  A->dep_pos[slot] = p;
  printf("track_dependency**********************************************\n");
  printf("Tracking dep: target %d depends on source %d at pos %d\n", to_i(target), to_i(source), source_pos);
  printf("Accumulator: %d\n", A->name);
  printf("PC: 0x%llx instr#: %llu\n",
       (unsigned long long)p.pc,
       (unsigned long long)p.instr);
  // propagate initial deps
  add_initials_from(vault_[to_i(source)].get(), A, state); //save current deps of sources
  printf("**********************************************track_dependency\n");
  return true;
}

// Handle LOADs and STOREs.
bool Dep_tracker::track_memory(Target target, Target source,
                               uint64_t addr, uint8_t width,
                               uint8_t ir,INIT_STATE state)
{
  auto* A = accu();
  A->name = target; // for STORE this becomes Target::MEM; for LOAD it's the RD.

  // ---------- LOAD ----------
  // LOAD: target <= MEM[addr..addr+width)
  if (source == Target::MEM) {
    printf("track_memory load +_+_+_+_+_+_+_+_+\n");
    for (uint8_t i = 0; i < width; i++) {
      auto& me = get_mem(addr + i);
      A->local_bytes[i] = me.addr;
      add_initials_from(me.cur.get(), A, state); //add this addr itself and its deps if this addr used before              
      A->dep_pos[BYTE1_INDEX + i] = cur_pos(BYTE1_INDEX + i);
    }
    printf("+_+_+_+_+_+_+_+_+track_memory load\n");
    return true;
  }

   printf("track_memory store +_+_+_+_+_+_+_+_+\n");
  // ---------- STORE ----------
  // STORE: MEM[addr..addr+width) <= source
  if (target == Target::MEM) {
    if (INIT_STATE::OVERWRITE == state) {
    for (uint8_t i = 0; i < width && i < 8; i++) {
      last_bytes_[i] = &get_mem(addr + i);       // commit_target will push A         
    }
  }
    mem_used_ = true;

    if (!A->dep_pos[IMM_INDEX].has_value())
      A->dep_pos[IMM_INDEX] = cur_pos(IMM_INDEX);

    auto place = [&](Target s, uint16_t slot_idx) {
      if (A->current_deps[slot_idx] == Target::NONE) {
        A->current_deps[slot_idx] = s;
        A->dep_pos[slot_idx] = cur_pos(slot_idx);   
      }
      add_initials_from(vault_[to_i(s)].get(), A, state);       
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
  printf("+_+_+_+_+_+_+_+_+track_memory store \n");
  return true;
}

// Finalize the instruction: push both accumulators.
bool Dep_tracker::commit_target(){
  printf("{{{((((((((((commit_target function begin))))))))))))))}}}\n");
  auto dump = [&](snapshot* a, const char* tag) {
    if (a->name == Target::ACCU || a->name == Target::ACCU_PC) return;

    printf("[%s] instr#=%llu pc=0x%llx target=%llu prev=%s\n",
          tag,
          (unsigned long long)instr_,
          (unsigned long long)pc_,
          (unsigned long long)to_i(a->name),
          a->prev ? "yes" : "no");

    // dep_pos
    for (size_t i = 0; i < a->dep_pos.size(); i++) {
      if (!a->dep_pos[i].has_value()) continue;
      const auto& pp = *a->dep_pos[i];
      printf("  dep_pos[%zu]: instr#=%llu pc=0x%llx pos=%u\n",
            i,
            (unsigned long long)pp.instr,
            (unsigned long long)pp.pc,
            (unsigned)pp.pos);
    }

    // initial_regs
    printf("  initial_regs:");
    bool any = false;
    for (size_t i = 0; i < NBR_OF_ACTUAL_DEPENDENCIES; i++) {
      if (a->initial_regs.test(i)) {
        printf(" %zu", i);
        any = true;
      }
    }
    if (!any) printf(" <none>");
    printf("\n");

    // initial_mem
    for (auto addr : a->initial_mem)
      printf("  initial_mem: 0x%llx\n", (unsigned long long)addr);

    // local_bytes
    for (size_t i = 0; i < a->local_bytes.size(); i++) {
      if (!a->local_bytes[i].has_value()) continue;
      printf("  local_bytes[%zu]: 0x%llx\n",
            i, (unsigned long long)*a->local_bytes[i]);
    }

    // weak deps summary
    printf("  weak_deps: %zu weak_dep_positions: %zu\n",
          a->weak_deps.size(), a->weak_dep_positions.size());
  };
  dump(accu(), "ACCU");
  dump(accu_pc(), "ACCU_PC");
  commit_one_accu(accu());
  commit_one_accu(accu_pc());
  last_bytes_.fill(nullptr);
  mem_used_ = false;
  csr_impl_ = false;
  printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
  printf("pc=0x%llx comit\n ", (unsigned long long)pc_);
  return true;
}

bool Dep_tracker::next_instruction(reg_t new_pc){
  pc_ = new_pc;
  ++instr_;
  return true;
}

// Walk the dependency graph for the leak and write unique addresses.
void Dep_tracker::save_req_dependencies_on_file(const Leak &cur_leak, std::ostream& dep_file){

  for (auto start_idx: {cur_leak.dep_reg1, cur_leak.dep_reg2}) {
    //bc most of the times dep reg2 is null.
    // TODO(dep-tracking): Using `NULL`/`0` as "no dep reg" conflicts with the
    // real RISC-V register number `x0==0`. Also, `break` here prevents
    // processing `dep_reg2` when `dep_reg1==0` but `dep_reg2` is set (e.g.
    // DIVU leaks). Prefer a sentinel (e.g. `UINT64_MAX`) or `std::optional`,
    // and use `continue` instead of `break` for missing entries.
    if (start_idx == NULL) continue; //use continue instead of break

    snapshot* s = vault_[start_idx].get();
    
    // for (uint16_t i = 0; i < OFFSET_TO_FREGS; ++i)
    //   if (s->initial_regs.test(i)) dep_file << "R" << i << "\n";

    //TODO:print dep regs according to dep_pos
    for (size_t i = 0; i < NBR_OF_ACTUAL_DEPENDENCIES; i++) {
      if (s->initial_regs.test(i)) {
        dep_file << "R" << i << "\n";
      }
    }
    for (auto a : s->initial_mem) dep_file << "0x" << std::hex << a << std::dec << "\n";
  }
  //if(getenv("SPIKE_DEP_RAW")){
  {
    // Pretty-print RV32 sign-extended values: Spike stores XLEN values in a 64-bit reg_t.
    // For RV32, many values/PCs are sign-extended to 0xffffffffXXXXXXXX. Printing the
    // low 32 bits makes dep_tracking.txt easier to consume for RV32 targets.
    auto pretty_rv32 = [](uint64_t x) -> uint64_t {
      return ((x >> 32) == 0xffffffffULL) ? (x & 0xffffffffULL) : x;
    };
    dep_file << "# PC: 0x" << std::hex << pretty_rv32(pc_) << std::dec << "\n";
    dep_file << "# Leak value: 0x" << std::hex << pretty_rv32(cur_leak.value) << std::dec << "\n";
    dep_file << "# Leak location: " << cur_leak.loc << "\n";
    dep_file << "# Dep reg1: " << cur_leak.dep_reg1 << "\n";
    dep_file << "# Dep reg2: " << cur_leak.dep_reg2 << "\n";
    dep_file << "# ---------------------Dep_tracker::save_req_dependencies_on_file\n"; 
  }
}

void Dep_tracker::print_orignal_dependencies(std::string name,uint64_t reg, std::ostream& dep_file){
  //dep_file << "## Dep_tracker::print_orignal_dependencies-----------------------\n"; 
  for (auto start_idx: {reg}) {
    snapshot* s = vault_[start_idx].get();
    for (size_t i = 0; i < NBR_OF_ACTUAL_DEPENDENCIES; i++) {
      if (s->initial_regs.test(i)) {
        dep_file << "R" << i << "\n";
      }
    }
    for (auto a : s->initial_mem) dep_file << "0x" << std::hex << a << std::dec << "\n";
  }
  //if(getenv("SPIKE_DEP_RAW")){
  {
    auto pretty_rv32 = [](uint64_t x) -> uint64_t {
      return ((x >> 32) == 0xffffffffULL) ? (x & 0xffffffffULL) : x;
    };
    dep_file << "# PC: 0x" << std::hex << pretty_rv32(pc_) << std::dec << "\n";
    dep_file << "# Orignal reg: " << reg << "\n";
    dep_file << "# Leak location: " << name << "\n";
    dep_file << "# ---------------------Dep_tracker::save_req_dependencies_on_file\n"; 
  }
}

void Dep_tracker::print_branch_dependencies(std::string name,uint64_t reg1,uint64_t reg2, std::ostream& dep_file){
  //dep_file << "## Dep_tracker::print_orignal_dependencies-----------------------\n"; 
  for (auto start_idx: {reg1, reg2}) {
    snapshot* s = vault_[start_idx].get();
    for (size_t i = 0; i < NBR_OF_ACTUAL_DEPENDENCIES; i++) {
      if (s->initial_regs.test(i)) {
        dep_file << "R" << i << "\n";
      }
    }
    for (auto a : s->initial_mem) dep_file << "0x" << std::hex << a << std::dec << "\n";
  }
  //if(getenv("SPIKE_DEP_RAW")){
  {
    auto pretty_rv32 = [](uint64_t x) -> uint64_t {
      return ((x >> 32) == 0xffffffffULL) ? (x & 0xffffffffULL) : x;
    };
    dep_file << "# PC: 0x" << std::hex << pretty_rv32(pc_) << std::dec << "\n";
    dep_file << "# Orignal reg1: " << reg1 << "\n";
    dep_file << "# Orignal reg2: " << reg2 << "\n";
    dep_file << "# Leak location: " << name << "\n";
    dep_file << "# ---------------------Dep_tracker::save_req_dependencies_on_file\n"; 
  }
}


// Maps an instruction to calls into the tracker.
void add_dependency(Dep_tracker &dep_tracker, reg_t pc, insn_t insn, processor_t* p, std::ostream& dep_file)
{
  // for now we don't support fp regs. Later we should lift_to_fp first.
  Target rs1 = to_T(insn.rs1());
  Target rs2 = to_T(insn.rs2());
  Target rd  = to_T(insn.rd());
  switch (insn.opcode()){   
    case 0x03: /*load rd rs1 imm*/
    {
      uint64_t width = 0;
      if (insn.funct3() == 0b000) width = 1; // LB
      if (insn.funct3() == 0b001) width = 2; // LH
      if (insn.funct3() == 0b010) width = 4; // LW
      if (insn.funct3() == 0b100) width = 1; // LBU
      if (insn.funct3() == 0b101) width = 2; // LHU
      if (insn.funct3() == 0b110) width = 4; // LWU
      if (insn.funct3() == 0b111) width = 8; // LD
      dep_tracker.track_dependency(rd, rs1, RS1_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
      dep_tracker.track_memory(rd, Target::MEM, insn.i_imm()+RS1, width,0, INIT_STATE::ADD);
      dep_tracker.track_dependency(rd, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::ADD);
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }  
    case 0x0f:/*fence , fence iorw, iorw*/
    {
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }   
    case 0x13: case 0x1B: /*op-imm / op-imm-32 rd rs1 imm*/
    {
      if (insn.rd() != 0) {
        dep_tracker.track_dependency(rd, rs1, RS1_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
        dep_tracker.track_dependency(rd, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::ADD);
        //dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      }
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }
    case 0x17:/*auipc, auipc rd, imm20*/
    {
      if (insn.rd() != 0) {
        dep_tracker.track_dependency(rd, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
        dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      }
        dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }
    case 0x23:  /*store rs2 imm(rs1)*/
    {
      uint64_t width = 0;
      if (insn.funct3() == 0b000) width = 1; // SB
      if (insn.funct3() == 0b001) width = 2; // SH
      if (insn.funct3() == 0b010) width = 4; // SW
      if (insn.funct3() == 0b011) width = 8; // SD
      dep_tracker.track_memory(Target::MEM, rs1, insn.s_imm()+RS1, width,0, INIT_STATE::OVERWRITE);
      dep_tracker.track_memory(Target::MEM, rs2, insn.s_imm()+RS1, width,0, INIT_STATE::ADD);
      //dep_tracker.track_memory(Target::MEM, Target::PC, PC_INDEX,width,0, INIT_STATE::ADD);
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }
    case 0x33: case 0x3B:   /*op / opw rd rs1 rs2 (RV32M + RV64-M)*/
    {
      if (insn.rd() != 0) {
        if (insn.rs1() != 0){
        dep_tracker.track_dependency(rd, rs1, RS1_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
        }
        if (insn.rs2() != 0){
        dep_tracker.track_dependency(rd, rs2, RS2_INDEX, 0b11, false, INIT_STATE::ADD);
        }
        //dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      }
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }     
    case 0x37:/*lui rd imm*/
    {
      if (insn.rd() != 0) {
        dep_tracker.track_dependency(rd, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
      }
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }
    case 0x63: /*branch,rs1, rs2, off*/
    {
      if (insn.rs2() != insn.rs1()){
        dep_tracker.track_dependency(Target::PC, rs1, RS1_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
        dep_tracker.track_dependency(Target::PC, rs2, RS2_INDEX, 0b11, false, INIT_STATE::ADD);
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
      if (taken){
        dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::ADD);
      }
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }
    case 0x67: /*jalr rd, rs1, imm */  
    {
      if (insn.rd() != 0)
      {
        dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
      }
      //dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      dep_tracker.track_dependency(Target::PC, rs1, RS1_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
      dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }
    case 0x6f:/*jal rd, off*/
    {
      if (insn.rd() != 0)
      {
      dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
      }
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }
    case 0x73: /*system*/
    {
      dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
      break;
    }
    default: /*unsupported instruction*/
    {
      break;
    }
  }

  auto bits = insn.bits();
  if (!((bits & 0xFFFF) == 0xa001 || bits == 0x0000006f)) {
    dep_tracker.commit_target();
  }


}

// // Maps an instruction to calls into the tracker.
// void add_dependency(Dep_tracker &dep_tracker, reg_t pc, insn_t insn, processor_t* p, std::ostream& dep_file)
// {
//   // for now we don't support fp regs. Later we should lift_to_fp first.
//   Target rs1 = to_T(insn.rs1());
//   Target rs2 = to_T(insn.rs2());
//   Target rd  = to_T(insn.rd());

//   switch (insn.opcode()){

//     /*arth- reg*/
//     case 0b0110011:
//     {
//       // dep_file<< "arth-reg\n";
//       if (insn.rd() != 0) {
//         dep_tracker.track_dependency(rd, rs1, RS1_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
//         dep_tracker.track_dependency(rd, rs2, RS2_INDEX, 0b11, false, INIT_STATE::ADD);
//         dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
//       }
//       // dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
//       break;
//     }
  
//     /*arth- imm*/
//     case 0b0010011:
//     {
//         if (insn.rd() != 0) {
//           dep_tracker.track_dependency(rd, rs1, RS1_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
//           dep_tracker.track_dependency(rd, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::ADD);
//           dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
//         }
//         // dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
//       break;
//     }
//     /*load*/
//     case 0b0000011:
//     {
//         uint64_t width = 1 << ((insn.funct3() & 0b11) ); // 0=byte,1=half,2=word,3=double
//         if (insn.funct3() == 0b100) width = 1; // LBU
//         if (insn.funct3() == 0b101) width = 2; // LHU
//         if (insn.funct3() == 0b110) width = 4; // LWU
//         if (insn.funct3() == 0b111) width = 8; // LDWU
//         dep_tracker.track_dependency(rd, rs1, RS1_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
//         dep_tracker.track_memory(rd, Target::MEM, insn.i_imm()+RS1, width,0, INIT_STATE::ADD);
//         dep_tracker.track_dependency(rd, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::ADD);
//         dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
//       break;
//     }
  
//     /*store*/
//     case 0b0100011:
//     {
//         uint64_t width = 1 << ((insn.funct3() & 0b11) ); // 0=byte,1=half,2=word,3=double
//         if (insn.funct3() == 0b100) width = 1; // SB
//         if (insn.funct3() == 0b101) width = 2; // SH
//         if (insn.funct3() == 0b110) width = 4; // SW
//         if (insn.funct3() == 0b111) width = 8; // SD
//         dep_tracker.track_memory(Target::MEM, rs1, insn.s_imm()+RS1, width,0, INIT_STATE::OVERWRITE);
//         dep_tracker.track_memory(Target::MEM, rs2, insn.s_imm()+RS2, width,0, INIT_STATE::ADD);
//         dep_tracker.track_memory(Target::MEM, Target::PC, PC_INDEX,width,0, INIT_STATE::ADD);
//         // dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
//       break;
//     }
//     /*jal*/
//     case 0b1111111:
//     {
//       // dep_tracker.track_dependency(rd, Target::PC, PC_INDEX, 0b11, false, INIT_STATE::ADD);
//       // dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
//       dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
//       break;
//     }
//     /*jalr*/  
//     case 0b1110111:
//     {
//       // dep_tracker.track_dependency(Target::PC, Target::PC, PC_INDEX, 0b11, false);
//       dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
//       dep_tracker.track_dependency(Target::PC, rs1, RS1_INDEX, 0b11, false, INIT_STATE::ADD);
//       break;
//     }
//     /*branch*/
//     case 0b1100011:
//     {
//         // Target rs1 = to_T(insn.rs1());
//         // Target rs2 = to_T(insn.rs2());
//         dep_tracker.track_dependency(Target::PC, rs1, RS1_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
//         dep_tracker.track_dependency(Target::PC, rs2, RS2_INDEX, 0b11, false, INIT_STATE::ADD);

//         uint8_t taken=0;
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
//         if (taken)
//           dep_tracker.track_dependency(Target::PC, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::ADD);

//       break;
//     }
//       /*lui*/
//     case 0b0110111:
//     {
//         if (insn.rd() != 0) {
//             printf("lui rd=%d\n", insn.rd());
//             dep_tracker.track_dependency(rd, Target::IMM, IMM_INDEX, 0b11, false, INIT_STATE::OVERWRITE);
//         }
//         break;
//     }

//     default:
//     {
//       break;
//     }
//   }

//   auto bits = insn.bits();
//   if (!((bits & 0xFFFF) == 0xa001 || bits == 0x0000006f)) {
//     dep_tracker.commit_target();
//   }


// }
