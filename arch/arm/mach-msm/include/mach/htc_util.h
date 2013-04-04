#ifndef __ARCH_ARM_MACH_MSM_HTC_UTIL_H
#define __ARCH_ARM_MACH_MSM_HTC_UTIL_H
extern void htc_show_interrupts(void);
extern void htc_timer_stats_onoff(char onoff);
extern void htc_timer_stats_show(u16 water_mark);
extern void htc_print_active_wake_locks(int type);

void htc_pm_monitor_init(void);
void htc_monitor_init(void);
void htc_idle_stat_add(int sleep_mode, u32 time);
void htc_xo_block_clks_count(void);
#endif
