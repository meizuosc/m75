#ifndef __ASMARM_TLS_H
#define __ASMARM_TLS_H

#ifdef __ASSEMBLY__
	.macro set_tls_none, tp, tmp1, tmp2
	.endm

	.macro set_tls_v6k, tp, tmp1, tmp2
	mcr	p15, 0, \tp, c13, c0, 3		@ set TLS register
	mov	\tmp1, #0
	mcr	p15, 0, \tmp1, c13, c0, 2	@ clear user r/w TLS register
#ifdef CONFIG_MT_DYNAMIC_COREPATH 
    mrc p15, 0, \tmp1, c0, c0, 0
    lsls \tmp1, \tmp1, #25
    bcs 1f  @ Jump to CA17 favor code
    @ CA7 favor
    mov \tmp1, #1
    mcr	p15, 0, \tmp1, c13, c0, 2	@ clear user r/w TLS register
    b 2f
1:
    @ CA17 favor
    mov \tmp1, #0
    mcr	p15, 0, \tmp1, c13, c0, 2	@ clear user r/w TLS register
2:
#endif
	.endm

	.macro set_tls_v6, tp, tmp1, tmp2
	ldr	\tmp1, =elf_hwcap
	ldr	\tmp1, [\tmp1, #0]
	mov	\tmp2, #0xffff0fff
	tst	\tmp1, #HWCAP_TLS		@ hardware TLS available?
	mcrne	p15, 0, \tp, c13, c0, 3		@ yes, set TLS register
	movne	\tmp1, #0
	mcrne	p15, 0, \tmp1, c13, c0, 2	@ clear user r/w TLS register
	streq	\tp, [\tmp2, #-15]		@ set TLS value at 0xffff0ff0
	.endm

	.macro set_tls_software, tp, tmp1, tmp2
	mov	\tmp1, #0xffff0fff
	str	\tp, [\tmp1, #-15]		@ set TLS value at 0xffff0ff0
	.endm
#endif

#ifdef CONFIG_TLS_REG_EMUL
#define tls_emu		1
#define has_tls_reg		1
#define set_tls		set_tls_none
#elif defined(CONFIG_CPU_V6)
#define tls_emu		0
#define has_tls_reg		(elf_hwcap & HWCAP_TLS)
#define set_tls		set_tls_v6
#elif defined(CONFIG_CPU_32v6K)
#define tls_emu		0
#define has_tls_reg		1
#define set_tls		set_tls_v6k
#else
#define tls_emu		0
#define has_tls_reg		0
#define set_tls		set_tls_software
#endif

#endif	/* __ASMARM_TLS_H */
