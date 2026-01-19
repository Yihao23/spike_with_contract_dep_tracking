// easy_ctr.c
#include <stdint.h>

volatile uint64_t x = 3;
volatile uint64_t tohost = 0;
volatile uint64_t fromhost = 0;
volatile uint64_t sink = 9;
static inline void spike_exit(int code) {
  tohost = ((uint64_t)code << 1) | 1;
  while (1) { }
}

void _start(void) {
  uint64_t a = x;    // LOAD
  // x = a + 1;         // STORE
  // if (a == 3) {      // 分支
  //   x = 5;
  // }

sink = a;
sink = sink + 1;
  //(void)b;

  spike_exit(0);
}