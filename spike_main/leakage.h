#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>

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


void add_leak(struct Leakage *f, const char *location, u_int64_t value)
{
   struct Leak *l = malloc(sizeof *l);
   strcpy(l->loc,location); 
   l->leak = value;
   SLIST_INSERT_HEAD(&f->head, l, leaks);
}

void print_leaks(FILE *dest, struct Leakage *f)
{
    struct Leak *l;
    fprintf(dest, "Leakage:\n");
    SLIST_FOREACH(l, &f->head, leaks) {
        fprintf(dest, "Leak: %s 0x%lx\n", l->loc, l->leak);
    }
}
void delete_leak(struct Leakage *f)
{
    struct Leak *b = SLIST_FIRST(&f->head);
    SLIST_REMOVE_HEAD(&f->head, leaks);
    free(b);
}

void delete_all_leaks(struct Leakage *f)
{
    struct Leak *b;
    while((b = SLIST_FIRST(&f->head))) {
        SLIST_REMOVE_HEAD(&f->head, leaks);
        free(b);
    }
}

void init_leaks(struct Leakage *f)
{
  SLIST_INIT(&f->head);
}

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