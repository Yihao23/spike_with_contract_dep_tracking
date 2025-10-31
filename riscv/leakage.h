
#ifndef _RISCV_LEAKAGE_H
#define _RISCV_LEAKAGE_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>
#include "decode.h"
#include "processor.h"
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <ostream>
// #include "dep.h"
struct Leak {
    std::string  loc;
    std::uint64_t value{}; 
    std::uint64_t dep_reg1{};
    std::uint64_t dep_reg2{};
    //normally one dep reg except for branches and mul/div
};

class Leakage {
public:

    void add_leak(std::string loc, std::uint64_t value, std::uint64_t dep_reg1= NULL, std::uint64_t dep_reg2 = NULL) {
        leaks_.emplace_back(Leak{std::move(loc), value, dep_reg1,dep_reg2});
    }

    void print_leaks(std::ostream& File) const {
        File << "Leakage:\n";
        for (const auto& l : leaks_) {
            File << "  Leak: " << l.loc << ' '
               << std::showbase << std::hex << std::uppercase << l.value
               << std::dec << '\n';
        //     save_dependency(l.dep_reg);
        }
    }

    void delete_leak() noexcept { leaks_.clear(); }
    std::size_t size() const noexcept { return leaks_.size(); }
    bool delete_all_leaks() const noexcept { return leaks_.empty(); }

    std::deque<Leak> leaks() const noexcept { return leaks_; }
    // const std::deque<Leak>& leaks() const noexcept { return leaks_; }

private:
    std::deque<Leak> leaks_; 
};



// struct Leak {
//         char loc[8];
//         u_int64_t leak;
//         //linked list entry
//         SLIST_ENTRY(Leak) leaks;
//         u_int64_t dep_reg;
//     };
//     struct Leakage {
//             //list of leaks in the "leakage"
//             SLIST_HEAD(,Leak) head; 
// };

void add_leakage(Leakage &leaks, reg_t npc, insn_t insn, insn_func_t func, processor_t *p);

// void add_leak(struct Leakage *f, const char *location, u_int64_t value/*,  u_int64_t DepReg*/);


// void print_leaks(FILE *dest, struct Leakage *f);

// void delete_leak(struct Leakage *f);


// void delete_all_leaks(struct Leakage *f);


// void init_leaks(struct Leakage *f);
#endif