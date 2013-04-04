/*
 *  Copyright (c) 2003 ARM Limited
 *  All Rights Reserved
 *  Copyright (c) 2010, 2012 Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <linux/init.h>

__CPUINIT

/*
PHY define in msm_iomap-8930.h, VIRT define in msm_iomap.h
The counters to check kernel exit for both cpu's
kernel foot print for cpu0  		: phy 0x8F9F1000 : virt 0xFA705000
kernel foot print for cpu1  		: phy 0x8F9F1004 : virt 0xFA705004
kernel exit counter from cpu0		: phy 0x8F9F1008 : virt 0xFA705008
kernel exit counter from cpu1		: phy 0x8F9F100C : virt 0xFA70500C
msm_pm_boot_entry			: phy 0x8F9F1010 : virt 0xFA705010
msm_pm_boot_vector			: phy 0x8F9F1014 : virt 0xFA705014
reset vector for cpu0			: phy 0x8F9F1018 : virt 0xFA705018
reset vector for cpu1			: phy 0x8F9F101C : virt 0xFA70501C
cpu0 reset vector address		: phy 0x8F9F1020 : virt 0xFA705020
cpu1 reset vector address		: phy 0x8F9F1024 : virt 0xFA705024
cpu0 reset vector address value		: phy 0x8F9F1028 : virt 0xFA705028
cpu1 reset vector address value		: phy 0x8F9F102C : virt 0xFA70502C
cpu0 frequency				: phy 0x8F9F1030 : virt 0xFA705030
cpu1 frequency				: phy 0x8F9F1034 : virt 0xFA705034
L2 frequency				: phy 0x8F9F1038 : virt 0xFA705038
acpuclk_set_rate footprint cpu0		: phy 0x8F9F103C : virt 0xFA70503C
acpuclk_set_rate footprint cpu1		: phy 0x8F9F1040 : virt 0xFA705040
*/

#define MSM_KERNEL_FOOTPRINT_PHYS	0x8F9F1000
#define MSM_KERNEL_FOOTPRINT_VIRT	0xFA705000
#define CPU1_EXIT_COUNT_PHYS	(MSM_KERNEL_FOOTPRINT_PHYS + 0xc)
#define CPU_FOOT_PRINT_MAGIC			0xACBDFE00
#define CPU_FOOT_PRINT_BASE_CPU1_PHY	(MSM_KERNEL_FOOTPRINT_PHYS + 0x4)

/*
 * MSM specific entry point for secondary CPUs.  This provides
 * a "holding pen" into which all secondary cores are held until we're
 * ready for them to initialise.
 *
 * This is executing in physical space with cache's off.
 */
ENTRY(msm_secondary_startup)
/*cpu_resume_exit_count*/
	ldr     r0, =CPU1_EXIT_COUNT_PHYS
	ldr     r4, [r0]
	add     r4, r4, #1
	str     r4, [r0]
	dsb
/*set_cpu1_reset_vector_foot_print_phy 4*/
	ldr     r0, =CPU_FOOT_PRINT_BASE_CPU1_PHY
	ldr     r4, =0xACBDFE04 /* make status number. */
	str     r4, [r0]
	dsb

	mrc	p15, 0, r0, c0, c0, 5 	@ MPIDR
	and	r0, r0, #15		@ What CPU am I
	adr	r4, 1f			@ address of
	ldmia	r4, {r5, r6}		@ load curr addr and pen_rel addr
	sub	r4, r4, r5		@ determine virtual/phys offsets
	add	r6, r6, r4		@ apply
pen:
	ldr	r7, [r6]		@ pen_rel has cpu to remove from reset
	cmp	r7, r0			@ are we lucky?
	bne	pen

	/*
	 * we've been released from the holding pen: secondary_stack
	 * should now contain the SVC stack for this core
	 */
	mvn	r7, #0			@ -1 to registers
	str r7,[r6]			@ back to the pen for ack
	b	secondary_startup

1:	.long	.
	.long	pen_release
