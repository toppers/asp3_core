    .section .text, "ax"
    .global start
start:
    la      sp, _stack_top
    la      t0, _bss_start
    la      t1, _bss_end
1:
    bge     t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:
    call    c_main
3:
    j       3b
