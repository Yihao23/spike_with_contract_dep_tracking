#ifndef RISCV_DEP_H
#define RISCV_DEP_H

#include <array>
#include <bitset>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <limits.h>
#include <ostream>
#include <algorithm>
#include <memory>
#include "platform.h"
#include "config.h"
#include "processor.h"
#include "mmu.h"
#include "disasm.h"
#include "decode_macros.h"
// #include "leakage.h"

struct Leak;


#define NBR_OF_ACTUAL_DEPENDENCIES 94

#define NUMBER_OF_DEPENDENCIES (NBR_OF_ACTUAL_DEPENDENCIES + 2)

#define NUMBER_OF_GENERAL_PURPOSE_REGISTERS 32

#define CURRENT_DEPENDENCIES_SIZE 4

#define CURRENT_DEPENDENCY_POSITION_SIZE 13

#define OFFSET_TO_FREGS NUMBER_OF_GENERAL_PURPOSE_REGISTERS

#define NUMBER_OF_FLOATING_POINT_REGISTERS 32

#define OFFSET_TO_CSRS (OFFSET_TO_FREGS + NUMBER_OF_FLOATING_POINT_REGISTERS)

#define NUMBER_OF_TRACKED_CSRS 29

#define END_CSRS (OFFSET_TO_CSRS + NUMBER_OF_TRACKED_CSRS)

enum META_MAPPING {
    PC_DEP = 4, PC_CTRL_FLOW = 5, CSR_POS1 = 6, CSR_POS_TARGET = 7
};

/* The following block's mapping needs to be numerically contiguous. */
enum CURRENT_DEPENDENCIES_INDEX {
    RS1_INDEX =  0, 
    RS2_INDEX =  1, 
    RS3_INDEX =  2, 
    PC_INDEX  =  3,
};

enum CURRENT_POSITION_INDEX {
    IMM_INDEX =    4, /* Immediate or shiftamount, if applicable. */
    /* The following block's mapping needs to be numerically contiguous! */
    BYTE1_INDEX =  5, /* Byte index for the BYTE, HALF, WORD & DOUBLE case. */
    BYTE2_INDEX =  6, /* Byte index for the HALF, WORD & DOUBLE case. */
    BYTE3_INDEX =  7, /* Byte index for the WORD & DOUBLE case. */
    BYTE4_INDEX =  8, /* Byte index for the WORD & DOUBLE case. */
    BYTE5_INDEX =  9, /* Byte index for the DOUBLE case. */
    BYTE6_INDEX =  10, /* Byte index for the DOUBLE case. */
    BYTE7_INDEX = 11, /* Byte index for the DOUBLE case. */
    BYTE8_INDEX = 12  /* Byte index for the DOUBLE case. */
    /* The previous block's mapping needs to be numerically contiguous! */
};

enum WORD_WIDTH {
    BYTE = 1, HALF = 2, WORD = 4, DOUBLE = 8
};

enum INIT_STATE {
    OVERWRITE = 0,
    ADD = 1
};

// enum Target {
enum class Target : uint16_t {
    PC = 93, ACCU = 94, ACCU_PC = 95, MEM = 96, NONE = 97, IMM = 98, REMAINING_MEM = 99,
    PMPADDR6 = 91, PMPADDR7 = 92,
    PMPADDR1 = 86, PMPADDR2 = 87, PMPADDR3 = 88, PMPADDR4 = 89, PMPADDR5 = 90,
    MCAUSE = 81, MTVAL = 82, MIP = 83, PMPCFG0 = 84, PMPADDR0 = 85,
    MSTATUS = 76, MEDELEG = 77, MIE = 78, MSCRATCH = 79, MEPC = 80,
    SEPC = 70, SCAUSE = 71, STVAL = 72, SIP = 73, SATP = 74, MHARTID = 75,
    FFLAGS = 64, FRM = 65, FCSR = 66, SSTATUS = 67, SIE = 68, SSCRATCH = 69, 
    F31 = 63, F30 = 62, F29 = 61,
    F28 = 60, F27 = 59, F26 = 58, F25 = 57, F24 = 56, F23 = 55, F22 = 54,
    F21 = 53, F20 = 52, F19 = 51, F18 = 50, F17 = 49, F16 = 48, F15 = 47,
    F14 = 46, F13 = 45, F12 = 44, F11 = 43, F10 = 42, F9 = 41, F8 = 40,
    F7 = 39, F6 = 38, F5 = 37, F4 = 36, F3 = 35, F2 = 34, F1 = 33, F0 = 32,
    X31 = 31, X30 = 30,
    X29 = 29, X28 = 28, X27 = 27, X26 = 26, X25 = 25, X24 = 24, X23 = 23,
    X22 = 22, X21 = 21, X20 = 20, X19 = 19, X18 = 18, X17 = 17 , X16 = 16,
    X15 = 15, X14 = 14, X13 = 13, X12 = 12, X11 = 11, X10 = 10, X9 = 9,
    X8 = 8, X7 = 7, X6 = 6, X5 = 5, X4 = 4, X3 = 3, X2 = 2, X1 = 1, X0 = 0
};


inline uint64_t to_i(Target t){ return static_cast<uint64_t>(t); }
inline Target   to_T(uint64_t v){ return static_cast<Target>(v); }

struct prog_position {
  uint64_t instr{};
  uint64_t pc{};
  uint16_t pos{}; // RS1/2/3/PC/IMM/BYTE1..8/CSR_POS_TARGET/etc.
  bool operator<(const prog_position& o) const {
    if (instr != o.instr) return instr < o.instr;
    return pos < o.pos;
  }
};

struct weak_dependency {
  bool is_memory = false;
  std::array<uint64_t,8> mem_addrs{};
  Target reg = Target::NONE;

  //no *next; use vector in snapshot
};

struct snapshot {
  Target name = Target::NONE;

  uint64_t pc = 0;

  // current deps: RS1, RS2, RS3, PC
  std::array<Target,CURRENT_DEPENDENCIES_SIZE> current_deps {Target::NONE, Target::NONE, Target::NONE, Target::NONE};

  // dep positions: RS1..RS3, PC, IMM, BYTE1..8
  std::array<std::optional<prog_position>,CURRENT_DEPENDENCY_POSITION_SIZE> dep_pos{};

  // initial deps (registers 0..93)
  std::bitset<NBR_OF_ACTUAL_DEPENDENCIES> initial_regs{};
  std::vector<uint64_t> initial_mem;

  // load-byte addresses
  std::array<std::optional<uint64_t>,8> local_bytes{};

  // weak deps
  std::vector<weak_dependency> weak_deps;
  std::vector<prog_position> weak_dep_positions;

  std::shared_ptr<snapshot> prev;
};

struct mem_entry {
  uint64_t addr{};
  std::shared_ptr<snapshot> cur; // current dependency snapshot for this byte

  //no *next; done in Dep_tracker
};

class Dep_tracker {
public:
  explicit Dep_tracker(reg_t initial_pc)
    : instr_(0), pc_(initial_pc) {
    vault_.resize(NUMBER_OF_DEPENDENCIES);
    for (uint16_t i = 0; i < NUMBER_OF_DEPENDENCIES; ++i) {
      vault_[i] = std::make_unique<snapshot>();
      vault_[i]->name = to_T(i);
      // initial target position exists for non-accu kinds in original code, but
      // we don’t need it to understand deps;
      if (i != to_i(Target::ACCU) && i != to_i(Target::ACCU_PC)) {
        // mark self-initial dependency except for PC
        if (i != to_i(Target::PC) && i < NBR_OF_ACTUAL_DEPENDENCIES)
          vault_[i]->initial_regs.set(i); //each reg depends on itself
      }
    }
    // Remaining memory weak deps “baseline”
    remaining_mem_ = std::make_unique<snapshot>();
    remaining_mem_->name = Target::MEM;
  }

  // Add a dep edge “target depends on source at source_pos”.
  // int_float_relation: bit0 for source, bit1 for target (0=int,1=float)
  bool track_dependency(Target target, Target source,
                        uint16_t source_pos,
                        uint8_t int_float_relation,
                        bool csr_implicit, INIT_STATE state);

  // Memory interaction: LOAD (source==MEM) or STORE (target==MEM)
  bool track_memory(Target target, Target source,
                    uint64_t addr, uint8_t width,
                    uint8_t int_float_relation, INIT_STATE state);

  // Commit current target (ACCU & ACCU_PC) into vault
  bool commit_target();

  bool next_instruction(reg_t new_pc);

  void save_req_dependencies_on_file(Leak &cur_leak, std::ostream& dep_file);

private:

  uint64_t instr_;
  uint64_t pc_;
  bool     mem_used_ = false;
  bool     csr_impl_ = false;

  std::vector<std::unique_ptr<snapshot>> vault_;   // index = Target value
  std::unordered_map<uint64_t, mem_entry> mem_;     // per byte
  std::array<mem_entry*,8> last_bytes_{};           // store aggregation
  std::unique_ptr<snapshot> remaining_mem_;        // weak deps baseline

  static bool is_xreg(Target t){ return to_i(t) < 32; }
  static bool is_freg(Target t){ return to_i(t) >= 32 && to_i(t) < 64; }
  static bool is_csr (Target t){ return to_i(t) >= 64 && to_i(t) < 93; }

  Target lift_to_fp(uint8_t ir_bits, Target t, bool is_target){
    if (t == Target::MEM || t == Target::PC || t == Target::IMM || t==Target::NONE) return t;
    bool need_fp = is_target ? (ir_bits & 0b10) : (ir_bits & 0b01);
    if (!need_fp && is_xreg(t)) return to_T(to_i(t)+32); // int->float
    return t;
  }

  prog_position cur_pos(uint16_t pos) const { return prog_position{instr_, pc_, pos}; }

  // push position into vector
  static void push_pos(std::vector<prog_position>& vec, prog_position p){
    auto it = std::lower_bound(vec.begin(), vec.end(), p);
    if (it==vec.end() || it->instr!=p.instr || it->pos!=p.pos) vec.insert(it, p);
  }

  snapshot* accu()    { return vault_[to_i(Target::ACCU)].get(); }
  snapshot* accu_pc() { return vault_[to_i(Target::ACCU_PC)].get(); }

  mem_entry& get_mem(uint64_t addr){ //build a new mem_entry object for the new addr and save its deps.
    auto it = mem_.find(addr);
    if (it != mem_.end()) return it->second;
    else {
      auto [it,ins] = mem_.emplace(addr, mem_entry{addr,{}});
      if (ins) {
        it->second.cur = std::make_unique<snapshot>(*remaining_mem_);
        it->second.cur->initial_mem = {addr};
        it->second.cur->weak_deps = remaining_mem_->weak_deps;
        it->second.cur->name = Target::MEM;
      }
      return it->second;
    }
  }

  void add_initials_from(const snapshot* src, snapshot* dst, INIT_STATE state){
    if (!src) return;
    if (state == INIT_STATE::OVERWRITE) {
      dst->initial_regs = src->initial_regs;
      dst->initial_mem = src->initial_mem;
      return;
    }
    else if( state == INIT_STATE::ADD) {
      dst->initial_regs |= src->initial_regs;
      for (auto a : src->initial_mem) {
        if (std::find(dst->initial_mem.begin(), dst->initial_mem.end(), a) == dst->initial_mem.end())
          dst->initial_mem.push_back(a);
      }
    }
  }

  void commit_one_accu(snapshot* a){
    if (a->name==Target::ACCU || a->name==Target::ACCU_PC) return;

    if (a->name != Target::X0 && a->name != Target::NONE) {
      a->current_deps[PC_INDEX] = Target::PC;
      a->dep_pos[PC_INDEX] = cur_pos(PC_DEP);
      add_initials_from(vault_[to_i(Target::PC)].get(), a, INIT_STATE::ADD);
    }

    if (a->name == Target::MEM) {
      for (auto b : last_bytes_) {
        if (!b) break;
        std::unique_ptr<snapshot> ns = std::make_unique<snapshot>(*a);
        add_initials_from(a,b->cur.get(), INIT_STATE::OVERWRITE);
        // ns->name = Target::MEM;
        // ns->prev = std::move(b->cur);
        // b->cur = std::make_uni/que<snapshot>(*a);
      }
      // weak dep positions for other memory:
    } else {
      // register/PC/CSR target
      auto& tgt = vault_[to_i(a->name)];
      std::unique_ptr<snapshot> ns = std::make_unique<snapshot>(*a);
      ns->prev = std::move(tgt);
      tgt = std::move(ns);
    }

    // reset accumulator
    *a = snapshot{};
    a->name = (a == accu()) ? Target::ACCU : Target::ACCU_PC;
  }
};

void add_dependency(Dep_tracker &dep_tracker, reg_t pc, insn_t insn, processor_t* p, std::ostream& dep_file);

#endif