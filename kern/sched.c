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


	/*
	Comentario en discord:

	Hay varios problemas en sched_yield(), por ejemplo, si no hay un proceso
	corriendo el valor de i no lo pueden utilizar para ver si el estado de
	ese proceso (no existente) es ENV_RUNNING. Además, j se inicializa
	incorrectamente si el proceso actual está en la última posición del
	arreglo envs[].

	Mi consejo es que la reimplementen siguiendo las instrucciones en el
	código y los siguientes consejos: Con la macro ENVX() pueden obtener el
	índice de un proceso a partir de su env_ind.

	        Pueden usar la operación módulo para obtener un índice que
	siempre se encuentre en el rango [0, NENV-1], así: i = n % NENV, donde n
	puede inicializarse como el índice del proceso actual más 1 si existe, o
	en 0 de lo contrario, e incrementarse más allá de los límites del
	arreglo sin ser un problema.
	 */
	int n = 0;

	if (curenv != NULL) {
		n = ENVX(curenv->env_id) + 1;
	}

	for (int i = 0; i < NENV; i++) {
		idle = &envs[(n + i) % NENV];

		if (idle->env_status == ENV_RUNNABLE) {
			env_run(idle);
		}
	}

	// Si la ejecucion llega hasta aca es porque no hay procesos RUNNABLE
	// En tal caso: veo si el actual esta RUNNING y hago env run
	if (curenv && curenv->env_status == ENV_RUNNING) {
		env_run(curenv);
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
	asm volatile("movl $0, %%ebp\n"
	             "movl %0, %%esp\n"
	             "pushl $0\n"
	             "pushl $0\n"
	             "sti\n"
	             "1:\n"
	             "hlt\n"
	             "jmp 1b\n"
	             :
	             : "a"(thiscpu->cpu_ts.ts_esp0));
}
