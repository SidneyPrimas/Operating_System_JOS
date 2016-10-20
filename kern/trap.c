#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < ARRAY_SIZE(excnames))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
	
	// Initialize the functions referred to in trapentry.S. 
	// These functions have been set to global variables, and refer to the same addresses as in trapentry.S
	void thandler0();
	void thandler1();
	void thandler2();
	void thandler3();
	void thandler4();
	void thandler5();
	void thandler6();
	void thandler7();
	void thandler8();
	void thandler10();
	void thandler11();
	void thandler12();
	void thandler13();
	void thandler14();
	void thandler16();
	void thandler17();
	void thandler18();
	void thandler19();
	void thandler48();
	
	//Setup the IDT (Inrerrupt/Trap Gate Descriptor Table))using the SETGATE macro.
	// SETGATE Macro: SETGATE(gate, istrap, sel, off, dpl) 
	// A single entry in the IDT is called a gate. We use gates to route interrupts to their handlers.
	// Here we are initializing the gates int the IDT. 
	// istrap = 0: Sets to interrupt gate, and thus turn-off other interrupts.
	// sel = GD_KT: Sets the code segment selector. Allows us to access the .text in kernel. If we are routing to a user functin, use GD_UT. 
	// off = function address: The offset should be the function address (a 32-bit address put in offset(31-16) and offset(15-0))
	// dpl = 0: Since we are handling built-in exceptions, we require the maximum privilege level
	SETGATE(idt[0], 0, GD_KT, &thandler0, 0);
	SETGATE(idt[1], 0, GD_KT, &thandler1, 3);
	SETGATE(idt[2], 0, GD_KT, &thandler2, 0);
	// Vector 3 is the breakpoint interrupt. Most other interrupts will be called by instructions in kernel space. We essentially don't want to allow a user to call these interrupts directly. 
	// However, the breakpoint interrupt is callable within the user space. Thus, we must indicate this in the gate, or the hardware will through a general protection fault. 
	SETGATE(idt[3], 0, GD_KT, &thandler3, 3);
	SETGATE(idt[4], 0, GD_KT, &thandler4, 0);
	SETGATE(idt[5], 0, GD_KT, &thandler5, 0);
	SETGATE(idt[6], 0, GD_KT, &thandler6, 0);
	SETGATE(idt[7], 0, GD_KT, &thandler7, 0);
	SETGATE(idt[8], 0, GD_KT, &thandler8, 0);
	SETGATE(idt[10], 0, GD_KT, &thandler10, 0);
	SETGATE(idt[11], 0, GD_KT, &thandler11, 0);
	SETGATE(idt[12], 0, GD_KT, &thandler12, 0);
	SETGATE(idt[13], 0, GD_KT, &thandler13, 0);
	SETGATE(idt[14], 0, GD_KT, &thandler14, 0);
	SETGATE(idt[16], 0, GD_KT, &thandler16, 0);
	SETGATE(idt[17], 0, GD_KT, &thandler17, 0);
	SETGATE(idt[18], 0, GD_KT, &thandler18, 0);
	SETGATE(idt[19], 0, GD_KT, &thandler19, 0);
	// Initialize the interrupt diescriptor to use vector 48 to handle system calls. Since system calls can be produced by user programs, set the DPL to 3. 
	// Essentially, this is a software interrupt. 
	SETGATE(idt[48], 0, GD_KT, &thandler48, 3);

	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;
	ts.ts_iomb = sizeof(struct Taskstate);

	// Initialize the TSS slot of the gdt.
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	
	// Use trapnumber from tf to figure out which exception to handle. 
	// For PGFL, route to page_fault_handler function. 
	// For most other exceptions, just kill the user environment. 
	if (tf->tf_trapno == T_PGFLT) {
		cprintf("Page Fault Exception \n"); 
		page_fault_handler(tf);
		return; 
	
	}
	
	if (tf->tf_trapno == T_DEBUG | tf->tf_trapno == T_BRKPT) {
		cprintf("Breakpoint or Debug Exception \n");
		monitor(tf);
		return;
	}
	
	if (tf->tf_trapno == T_SYSCALL) {
		cprintf("System Call Interrupt \n");
		// Extract the arguments from the registers. 
		// Right before the software interrupt was called, these registers were filled with the correct arguments by inline assembly. 
		// When the int assembly instruction was called, we transferred these registers through the stack and into the tf variable. 
		uint32_t syscallno = tf->tf_regs.reg_eax;
		uint32_t a1 = tf->tf_regs.reg_edx;
		uint32_t a2 = tf->tf_regs.reg_ecx;
		uint32_t a3 = tf->tf_regs.reg_ebx; 
		uint32_t a4 = tf->tf_regs.reg_edi; 
		uint32_t a5 = tf->tf_regs.reg_esi; 
		
		// Put the return data into the eax register. 
		tf->tf_regs.reg_eax = syscall(syscallno, a1, a2, a3, a4, a5); 		
		return;
	
	}

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	// LAB 4: Your code here.

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}

}

// We essentially manually setup the stack so that *tf is the tf structure (even though no argument has been directly passed). 
void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));


	cprintf("Incoming TRAP frame at %p\n", tf);
	
	// Check if we are coming from user mode. If it's coming from user mode, update the tf variable in curenv->env_tf. 
	// If not, then (???) we are coming from Kernel mode and the curenv is already updated (???)
	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		assert(curenv);

		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point. This is used in env_run defined in env.c. 
		// This actually copies the data on the stack into curenv->env_tf. 
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		// Essentially, we update tf to no longer point to the stack, but instead pont to the data held in curenv->env_tf
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// LAB 3: Your code here.
	// Handle kernel-mode page faults.
	if ((tf->tf_cs & 3) == 0) {
		print_trapframe(tf);
		panic("page_fault_handler: The previous code segment came from a kernel CPL. We panicked from the kernel. \n");
	}
	
	// ToDo: Figure out if this check is redundant or checks different setup. 
	if (tf->tf_cs == GD_KT) {
		print_trapframe(tf);
		panic("page_fault_handler: The previous code segment has a kernel DPL. \n");
	}

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

