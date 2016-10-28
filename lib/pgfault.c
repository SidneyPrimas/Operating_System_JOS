// User-level page fault handler support.
// Rather than register the C page fault handler directly with the
// kernel as the page fault handler, we register the assembly language
// wrapper in pfentry.S, which in turns calls the registered C
// function.

#include <inc/lib.h>

// Note: On user page fault, we 1) transfer to kernel page_fault_handler, 2) transfer to pfentry.s, 3) transfer to C pagefault hanlders, 4) transfer back to pfentry.S, 5) transfer back to original user-space stack and instruction. 

// Assembly language pgfault entrypoint defined in lib/pfentry.S.
extern void _pgfault_upcall(void);

// Pointer to currently installed C-language pgfault handler.
void (*_pgfault_handler)(struct UTrapframe *utf);

//
// Set the page fault handler function.
// If there isn't one yet, _pgfault_handler will be 0.
// The first time we register a handler, we need to
// allocate an exception stack (one page of memory with its top
// at UXSTACKTOP), and tell the kernel to call the assembly-language
// _pgfault_upcall routine when a page fault occurs.
//

// To redirect a user exception page fault: 1) tell kernel which function to call with _pgfault_upcall (stored in env)
// and 2) tell pfentry.S which function to call with _pgfault_handler, a global variable for the user. 
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;
	
	
	if (_pgfault_handler == 0) {
		// First time through!
		// LAB 4: Your code here.
		r = sys_page_alloc(thisenv->env_id, (void *) (UXSTACKTOP-PGSIZE), PTE_U | PTE_W | PTE_P); 
		if (r < 0 ) {
			panic("set_pgfault_handler: could not allocate user exception stack. \n");
		}
		
		// Tell kernel to call _pgfault_upcall routine (which is a global variable). In that routine, then call the _pgfault_handler. 
		r =  sys_env_set_pgfault_upcall(thisenv->env_id, _pgfault_upcall); 
		if (r < 0 ) {
			panic("set_pgfault_handler: could not set pgfault_upcall. \n");
		}
		
		
	}

	// Save handler pointer for assembly to call.
	// _pgfault_handler is a global variable, and will be called in pfentry.S. 
	_pgfault_handler = handler;
	
}
