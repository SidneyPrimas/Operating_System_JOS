Notes: 
+ When we use boot_alloc, we are working with allocating virtual memory in the kernel space. End indicates the lowest virtual address in the kernel that is not mapped. 
+ Segment register provide important environment information so that the code can run (like the data segment and code segment). 
+ Env.c: This file creates functions that allows us to 1) create a new environment/process, 2) load the appropriate data/code into the RAM for that environment, 3) setup the env variables (that keeps track of process variables) and 4) switches to the environment. 
+ Int Instruction: The int instruction forces a processor interrupt. Pass it a specific vector number to give it a specific type of interrupt. Essentially, int allows us to go from user space to kernel space. 
+ Note: A system call is essentially a set of assembly instructions. The assembly instructions store the # representing the specific system call in regsters, as well as all the arguments. Then, it calls the system call interrupts, which handles the function in kernel mode. 
+ Environments/Process: Each process, has it's own global environment variable. An environment includes both user and kernel space. However, we try to keep the memory and interactions seperate (although there is some over-lap). Thus, the kernel has an environment variable called curenv and the user has a replicated env variable called thisenv. However, I think they point to the same actual location in memory!
+ Response to Backtrace Pagefault: The reason we get a pagefault after libmain.c on the backtrace to the breakpoint could be because: 1) it's the end of the user stack, and we jump to random place in memory, 2) it's the end of the user stack, and we jump to the kernel stack (which would give us a page fault since that area isn't mapped for the user)


Inline Assembly Notes: 
+ Inline assembly acts as interface between assembly and C. 
+ General Syntax for extended inline assembly: asm(assembler instruction : output operands starting at %0: input operands : clobbered registers)
+ First Statment: Defines the assmebly line to be called. We essentially move the contents of a register to the output. 
+ Second statement: Defines the output syscallno. The output is always %0 and the input is always %1. 
+ Constraint: The constraint =r indicatates that we can use any register, and that it's an ouput operand. 
+ The double % indicates that we are referring to a register (and not an operand)
+ Volatile ensures that the compiler doesn't change the location of the code. 
+ Example code: uint32_t syscallno; asm volatile("movl %%eax,%0" : "=r" (syscallno));	

Questions: 
+ What is UVPT? Why is this a page table, and not a page directory
+ What does code-segment do. And how do we use it? 
+ For GD_KT, does it allow us to select all .text define din kernel space? 
+ When switching between the user and kernel space in the same environment, are the environment variables always the same? When are they different? 
+ What is the difference in checking GD_KT () vs. the first bits in cs? Checking CPL vs DPL in C? 
+ In sys_cputs, why do we always assume this comes from the current environment (we use envid from curenv)? Same in kdebug.c? Why do we get the environment from the kernel. Or, what is a situation where a kernel would kill a different environment? Doesn't each enviornment have it's own kernelspace? 

Possible Issues: 
+ Make sure we setup the linked list correctly. 
+ I used page_alloc in env_setup_vm
+ Initialize entry point in load_icode. 
+ Make sure e->env_tf is setup correctly. 
+ Going against the directions, i zero the new page in region_alloc. 

To DO: 
+ HIGHLY IMPORTANT: Make the debug statement so I can I can enable and disable printing. 
+ Take notes on regers. 
+ Don't fully understand the difference in checking the CS DPL and the CPL. I do both in the page handler sectin. 
+ We currently don't allow interrupts of sys_calls. 
