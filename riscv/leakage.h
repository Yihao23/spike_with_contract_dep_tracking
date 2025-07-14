
#ifndef _RISCV_LEAKAGE_H
#define _RISCV_LEAKAGE_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>
#include "decode.h"
#include "processor.h"

// modified from https://stackoverflow.com/a/16819977

struct Leak {
    char loc[8];
    u_int64_t leak;
    //linked list entry
    SLIST_ENTRY(Leak) leaks;
};
struct Leakage {
    //list of leaks in the "leakage"
    SLIST_HEAD(,Leak) head; 
};

void add_leakage(struct Leakage *l, reg_t npc, insn_t insn, insn_func_t func);

void add_leak(struct Leakage *f, const char *location, u_int64_t value);


void print_leaks(FILE *dest, struct Leakage *f);

void delete_leak(struct Leakage *f);


void delete_all_leaks(struct Leakage *f);


void init_leaks(struct Leakage *f);



/* 
int main(void)
{
    struct Leakage f;
    SLIST_INIT(&f.head);

    add_leak(&f,"one", 5);
    add_leak(&f,"two", 42);
    add_leak(&f,"three", 69);

    print_leaks(&f);

    puts("\nDeleting three");
    delete_leak(&f);
    print_leaks(&f);

    puts("\nAdding 2 leaks");
    add_leak(&f,"three", 420);
    add_leak(&f,"three", 42069);
    print_leaks(&f);

    puts("\nDeleting three");
    delete_leak(&f);
    print_leaks(&f);

    delete_all_leaks(&f);


    return 0;
}
 */

 #endif