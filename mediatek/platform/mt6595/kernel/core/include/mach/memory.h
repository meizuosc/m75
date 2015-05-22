#ifndef __MT_MEMORY_H__
#define __MT_MEMORY_H__

/*
 * Define constants.
 */

#define PHYS_OFFSET 0x40000000

/*
 * Define macros.
 */

/* IO_VIRT = 0xF0000000 | IO_PHYS[27:0] */
#define IO_VIRT_TO_PHYS(v) (0x10000000 | ((v) & 0x0fffffff))
#define IO_PHYS_TO_VIRT(p) (0xf0000000 | ((p) & 0x0fffffff))

#ifndef __ASSEMBLER__ 
#define INFRA_4G_SUPPORT    (*(volatile unsigned int *)(0xF0003208) & (1 << 15))
#define PERISYS_4G_SUPPORT  (*(volatile unsigned int *)(0xF0001f00) & (1 << 13)) 

static inline unsigned int enable_4G(void)
{
    return (INFRA_4G_SUPPORT && PERISYS_4G_SUPPORT);
}
#endif 

#endif  /* !__MT_MEMORY_H__ */
