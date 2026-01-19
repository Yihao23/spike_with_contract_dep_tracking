      .option norvc
      .section .text
      .globl _start
  _start:
      la   t0, a_mem
      ld   a0, 0(t0)        # rs1 value from memory (a_mem)

      la   t1, b_mem
      ld   a1, 0(t1)        # rs2 value from memory (b_mem)

      divu a2, a0, a1       # opcode 0110011 (M extension)

      # exit via tohost
      la   t2, tohost
      li   t3, 1            # (code<<1)|1, code=0 => 1
      sd   t3, 0(t2)
  1:  j    1b

      .section .data
      .align 3
  a_mem:
      .dword 100
  b_mem:
      .dword 7
  tohost:
      .dword 0
  fromhost:
      .dword 0
