.extern IRQHandler
.extern FIQHandler

.section .vectors, "ax"
.align 5
.global vector_table
vector_table:
    ldr     pc, =_start
    ldr     pc, =handler_undef
    ldr     pc, =handler_swi
    ldr     pc, =handler_prefetch
    ldr     pc, =handler_data
    ldr     pc, =handler_unused
    ldr     pc, =handler_irq
    ldr     pc, =handler_fiq

.section .text.start, "ax"
.global _start
.type _start, %function

_start:
    cpsid   if
    ldr     sp, =_stack_top

    ldr     r0, =vector_table
    mcr     p15, 0, r0, c12, c0, 0

    /*
     * U-Boot leaves SCTLR.A set; packed LEAP structs then fault on fields
     * at offsets like g_disc_result.message_type (+6).
     */
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #0x2
    mcr     p15, 0, r0, c1, c0, 0

    ldr     r0, =__bss_start__
    ldr     r1, =__bss_end__
    mov     r2, #0
1:
    cmp     r0, r1
    bhs     2f
    str     r2, [r0], #4
    b       1b

2:
    bl      bbb_wdt_disable_all
    bl      main

3:
    b       3b

handler_undef:
    mov     r0, #'U'
    mov     r1, #0
    mov     r2, #0
    bl      bbb_exc_hang
    b       .

handler_swi:
    mov     r0, #'S'
    mov     r1, #0
    mov     r2, #0
    bl      bbb_exc_hang
    b       .

handler_prefetch:
    sub     lr, lr, #4
    stmfd   sp!, {r0-r12, lr}
    mov     r0, #'P'
    mrc     p15, 0, r1, c5, c0, 1
    mrc     p15, 0, r2, c6, c0, 0
    bl      bbb_exc_hang
    b       .

handler_data:
    sub     lr, lr, #8
    stmfd   sp!, {r0-r12, lr}
    mov     r0, #'D'
    mrc     p15, 0, r1, c5, c0, 0
    mrc     p15, 0, r2, c6, c0, 0
    bl      bbb_exc_hang
    b       .

handler_unused:
    mov     r0, #'?'
    mov     r1, #0
    mov     r2, #0
    bl      bbb_exc_hang
    b       .

handler_irq:
    ldr     pc, =IRQHandler

handler_fiq:
    ldr     pc, =FIQHandler
