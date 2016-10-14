/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



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
	TRAPHANDLER_NOEC(thandler0, 0);
	TRAPHANDLER_NOEC(thandler2, 2);
	TRAPHANDLER_NOEC(thandler3, 3);
	TRAPHANDLER_NOEC(thandler4, 4);
	TRAPHANDLER_NOEC(thandler5, 5);
	TRAPHANDLER_NOEC(thandler6, 6);
	TRAPHANDLER_NOEC(thandler7, 7);
	TRAPHANDLER(thandler8, 8);
	TRAPHANDLER(thandler10, 10);
	TRAPHANDLER(thandler11, 11);
	TRAPHANDLER(thandler12, 12);
	TRAPHANDLER(thandler13, 13);
	TRAPHANDLER(thandler14, 14);
	TRAPHANDLER_NOEC(thandler16, 16);
	TRAPHANDLER(thandler17, 17);
	TRAPHANDLER_NOEC(thandler18, 18);
	TRAPHANDLER_NOEC(thandler19, 19);
	TRAPHANDLER_NOEC(thandler48, 48);


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
