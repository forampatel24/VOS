/*
 * kernel/process/pcb.h
 *
 * JARVIS OS — Process Control Block.
 *
 * Every process owns exactly one PCB. The PCB is the single record of a
 * process's identity, lifecycle state, priority, saved register context
 * and accounting data. Nothing outside the process subsystem stores
 * process data (Process Rules).
 */

#ifndef JARVIS_PCB_H
#define JARVIS_PCB_H

#include <stdint.h>

#include "../cpu/registers.h"

#define JVK_PROC_NAME_LEN 32

typedef enum {
    PROC_STATE_READY = 0,
    PROC_STATE_RUNNING,
    PROC_STATE_WAITING,
    PROC_STATE_SUSPENDED,
    PROC_STATE_TERMINATED,
} jvk_proc_state_t;

typedef struct {
    int              pid;              /* positive; 0 marks a free slot */
    char             name[JVK_PROC_NAME_LEN];
    jvk_proc_state_t state;
    int              priority;         /* higher runs first (future) */
    cpu_regs_t       regs;             /* saved register/PC context */
    unsigned long    created_ticks;
    int              cpu_used;         /* instructions executed so far */
} jvk_pcb_t;

#endif /* JARVIS_PCB_H */
