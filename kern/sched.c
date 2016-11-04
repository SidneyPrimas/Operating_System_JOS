#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// LAB 4: Your code here.
	
	// Set initial to beginning of envs array or just after previously running env. 
	uint32_t initial = !curenv ? 0: ENVX(curenv->env_id);

	// Keep track of the highest priority environment. 
	struct Env * next_env = NULL;
	size_t i; 
	size_t index; 
	
	// Identify the highest priority task (other than the currently running task (or task 0). 
	for (i = 1; i < NENV; i++) {
		index = (i + initial)%NENV; 
	
		// If we found a runnable environemnt, call env_run (which will set the previously running environment to runnable and then run the new environment). 
		if (envs[index].env_status == ENV_RUNNABLE) {
			// If next_env is null or has a lower/equal priority, update the next env. 
			if ((!next_env) || (envs[index].env_priority >= next_env->env_priority)) {
				next_env = &envs[index];
			}
		}
	}
	
	
	// Check if we have found a suitable next environment. 
	if ((next_env) && (next_env->env_status == ENV_RUNNABLE)) {
		env_run(next_env);
	} 
	// Check if we can running the previously running environment or initial environment. 
	else {
		// Check the previously running environment (in previous position). Run it when:  1) check if we still can run it, and 2) make sure it's actually the same environment. 
		if ((curenv != NULL) && (curenv->env_status == ENV_RUNNING) && (envs[initial].env_id == curenv->env_id)) {
			env_run(curenv);
		}
		
		// Handle the initialization case (where curenv doesn't exist). 
		if ((curenv == NULL) && (envs[0].env_status == ENV_RUNNABLE)) {
			env_run(&envs[0]);
		}
	
	}
	
	

	// sched_halt never returns
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;
	
	cprintf("sched_halt debug: Running sched_halt() \n");

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}

