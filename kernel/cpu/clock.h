/*
 * kernel/cpu/clock.h
 *
 * JARVIS OS — virtual clock.
 *
 * The clock drives the whole system: the bridge advances it one tick at
 * a time and each tick lets the CPU run one time-slice quantum. The
 * timer "fires" (quantum expired while the CPU is still running), which
 * is the preemption point where a real OS would switch processes.
 *
 * speed_hz is a virtual figure used by the UI; quantum is the number of
 * instructions executed per tick.
 */

#ifndef JARVIS_CLOCK_H
#define JARVIS_CLOCK_H

typedef struct {
    int speed_hz;     /* virtual clock speed (instructions/second) */
    int quantum;      /* instructions per time slice */
    int ticks;        /* ticks elapsed since boot */
    int timer_fired;  /* set by clock_tick, cleared by clock_clear_timer */
} jvk_clock_t;

void clock_init(jvk_clock_t* clk, int speed_hz, int quantum);
void clock_tick(jvk_clock_t* clk);
int  clock_timer_fired(const jvk_clock_t* clk);
void clock_clear_timer(jvk_clock_t* clk);

#endif /* JARVIS_CLOCK_H */