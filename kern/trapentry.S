/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 * Important Note: At this point, the processor has already taken care of the following
 * 1) Switches to the trap stack, 2) the exception paramters have been pushed onto this trap stack
 * 3) We use the interrupt vector and IDT to find the handler function, 4) we call the handler function. 
 * Moving forward: The handler function referenced in IDT is defined in trapentry.S. 
 * In trapentry.S, we setup the stack accordingly, and then jump to trap in trap.c to actually execute the handler. 
 */
 
 	/* TRAPHANDLER macros generate assembly code that setup name as a function (with global access).
 	 * Each name is a handler fuction that pushes us to _alltraps and traps after setting up the stack. 
 	 * Also, pushes number (wich is the interrupt vetor) onto the stack, which allows us to retreive it later (to jump into IDT). 
 	 */
	TRAPHANDLER_NOEC(thandler0, T_DIVIDE);
	TRAPHANDLER_NOEC(thandler1, T_DEBUG);
	TRAPHANDLER_NOEC(thandler2, T_NMI);
	TRAPHANDLER_NOEC(thandler3, T_BRKPT);
	TRAPHANDLER_NOEC(thandler4, T_OFLOW);
	TRAPHANDLER_NOEC(thandler5, T_BOUND);
	TRAPHANDLER_NOEC(thandler6, T_ILLOP);
	TRAPHANDLER_NOEC(thandler7, T_DEVICE);
	TRAPHANDLER(thandler8, T_DBLFLT);
	TRAPHANDLER(thandler10, T_TSS);
	TRAPHANDLER(thandler11, T_SEGNP);
	TRAPHANDLER(thandler12, T_STACK);
	TRAPHANDLER(thandler13, T_GPFLT);
	TRAPHANDLER(thandler14, T_PGFLT);
	TRAPHANDLER_NOEC(thandler16, T_FPERR);
	TRAPHANDLER(thandler17, T_ALIGN);
	TRAPHANDLER_NOEC(thandler18, T_MCHK);
	TRAPHANDLER_NOEC(thandler19, T_SIMDERR);
	
	// Interrupt handler (for 16 hardware interrupts from 32 to 47.) 
	// None of these have error codes. 
	TRAPHANDLER_NOEC(thandler32, IRQ_OFFSET + IRQ_TIMER);
	TRAPHANDLER_NOEC(thandler33, IRQ_OFFSET + IRQ_KBD);
	TRAPHANDLER_NOEC(thandler34, IRQ_OFFSET + 2);
	TRAPHANDLER_NOEC(thandler35, IRQ_OFFSET + 3);
	TRAPHANDLER_NOEC(thandler36, IRQ_OFFSET + IRQ_SERIAL);
	TRAPHANDLER_NOEC(thandler37, IRQ_OFFSET + 5);
	TRAPHANDLER_NOEC(thandler38, IRQ_OFFSET + 6);
	TRAPHANDLER_NOEC(thandler39, IRQ_OFFSET + IRQ_SPURIOUS);
	TRAPHANDLER_NOEC(thandler40, IRQ_OFFSET + 8);
	TRAPHANDLER_NOEC(thandler41, IRQ_OFFSET + 9);
	TRAPHANDLER_NOEC(thandler42, IRQ_OFFSET + 10);
	TRAPHANDLER_NOEC(thandler43, IRQ_OFFSET + 11);
	TRAPHANDLER_NOEC(thandler44, IRQ_OFFSET + 12);
	TRAPHANDLER_NOEC(thandler45, IRQ_OFFSET + 13);
	TRAPHANDLER_NOEC(thandler46, IRQ_OFFSET + IRQ_IDE);
	TRAPHANDLER_NOEC(thandler47, IRQ_OFFSET + 15);
	
	//System call trap. 
	TRAPHANDLER_NOEC(thandler48, T_SYSCALL);


/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps: 
	/* Push values to make stack look like struct Trapframe 
	 * Looking at TapFrame, all items have been pushed on stack expect tf_es, tf_ds and then then tf_regs. 
	 * Push ds and es onto stack so that we correctly make tf structure on stack. 
	 */
	pushl %ds;
	pushl %es;
	pushal
	/* load GD_KD into %ds and %es: Need to access to give correct access to the crrent environment.  */
	movw $(GD_KD), %ax; /* We cannot load immediate-value directly into segment registers */
	movw %ax, %ds; 
	movw %ax, %es;  
	/* Push the %esp onto the stack. This will allow us to get access to the trapframe variables currently on the stack
	 * Essentialy, esp will be a pointer to the first byte of the tf that we setup on the stack. 
	 */
	pushl %esp; 
	/* Call Trap. Now, we execute the exception/interrupt handling in trap.c */
	call trap
