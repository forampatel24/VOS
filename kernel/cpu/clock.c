/*
 * kernel/cpu/clock.c
 *
 * JARVIS OS — virtual clock implementation.
 */

#include "clock.h"

void clock_init(jvk_clock_t* clk, int speed_hz, int quantum)
{
    clk->speed_hz    = speed_hz > 0 ? speed_hz : 1000;
    clk->quantum     = quantum  > 0 ? quantum  : 10;
    clk->ticks       = 0;
    clk->timer_fired = 0;
}

void clock_tick(jvk_clock_t* clk)
{
    clk->ticks++;
    /* M2: the timer interrupt fires on every tick boundary. The kernel
       decides whether to surface it as an event based on what the CPU
       actually did during the tick. */
    clk->timer_fired = 1;
}

int clock_timer_fired(const jvk_clock_t* clk)
{
    return clk->timer_fired;
}

void clock_clear_timer(jvk_clock_t* clk)
{
    clk->timer_fired = 0;
}