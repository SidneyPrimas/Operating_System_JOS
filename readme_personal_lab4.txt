Notes: 
+ When we initially boot an AP, after the boot processor has been setup, the new processor initially only uses a 16bit processor. That's why we can only load the boot code into a memory location below 640k. In additiona, all processors share commomen RAM memory, and so we must take this into account. 
+ mpentry_start and mpentry_end are global variables that are defined in assembly as locations in memory. Thus, we can refer to them in C, and refer to the code defined in mpentry.S as an array. 
+ PTSIZE: The bytes mapped by a single page table or by a single page directory entry: PGSIZE*(num of page entries). 
+ Each CPU has it's own: 1) kernel stack (so that when CPUS are operating at the same time on the kernel, they don't interfere with each other), 2) environment variable so that we know with what environment each CPU is working with, 3) TSS indicator so that we can ... and 4) registers, so we must set each CPUs registers seperately. 
+ Remember, above kernbase, we mapped the paging so that we always can just subtract KERNBASE and get the physical address. This is only true for the kernbase mapping since all kernel memory that we create is always mapped to the same physical regiion. The reason this is true is that every physical address is replicated in the virtual address space. So, any physical address that the kernel uses can also be found in the virtual address space. 
+ The TSS heps us handle interrupts occuring on different processors. Essentially, when we interrupt from user space into kernel space on a specific processor, we have to make sure that the processor (which can be running arbitrary code) uses the correct kernel stack. So, when switching kernel stacks through a trap, we use TSS to work with the correct, processor specific stack 
+ In a global descriptor table, we hold segment descriptors. One possible segment descriptor is a task state descriptor. In our setup, we have a task state descriptor for each process. To select a TSS, we use the task register. The task register needs to be loaded with the segment selector that points to t he correct TSS in the GDT. 
+ We never officially map the virtual address at percpu_kstacks[] to the physical address through the page table. We only map the addresses from MEMLAYOUT. 
+ Whenever we execute common kernel code we need the kernel lock. And, whenever we go into user mode, we need to unlock. 
+ Important note: curenv and thisenv are different! 
+ Remember: Whenever I put a new system call in, I have to route the system call!!!
+ Important: It seems that if a code line is optimized out, the brreakpoint will be called at the nearest line to the optimized out line. 
+ IF Flag: When entering an interrupt, we can indicate how to change the interrupt flag. By disabling the flag within the IDT routing, we disable interrupts in the handler. When we return to the previous stack (user stack), we push the previous eflag back from the stack, and thus restore the previous state. This is done through the iret instruction that restores the eflags implicitly. 
+ The user references thisenv variable to get it's environment and the kernel reference curenv (through the cpu struct) to get it's current environment. 



Question: 
+ Why do we have two virtual addresses for each kernel stack that is available to the kernel. One virtual address is through the percpu_kstacks[], while the other is the mapping explicitly defined through KSTKSIZE and KSTKGAP.
+ What happens to paging when we allocate a huge array in memory (just without updating the paging)?  
+ When we use our updated fork, do we ever remove the previously mapped page (if both the parent and child remap it to a new page)?
+ Is _pgfault_handler a global variable, or is it different for each enviornment? 

Possible Issue: 
+ Do we release the lock at the correct point when we return to user mode? 
+ Make sure I get the return of 0 correct in the child environment! 
+ In sys_ipc_recv, make sure to still return a 0 after the system call returns. 
