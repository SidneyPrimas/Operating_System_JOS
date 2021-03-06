File Descriptions: 
+ memlayout.h: A description of the layout of the virtual address space. 
+ kclock.c and klcock.h: Manipulate PCs battery-backed clock and CMOS RAM hardware (where BIOS records the amount of physical memory  the PC contains). 
+ mmu.h (memory management unit): Includes many lower-level definitions to help use the mmu hardware. 


Implementation Notes: 
+ Panic and Warn: These functions take-in a string and print out the file and line number of the function call. These values are obtained through some kernel trickery that I don't fully understand yet. 
+ Page Size: All pages are multiples of 4096bytes, starting at 0. Since they have 4096bytes they have 4096 addresses since each address points to a single byte in memory. 
+ Usually use a char since it represents a single byte in c (allows us to work with a single byte). 
+ boot_alloc allocates contigious physical memory. 
+ For a virtual pointer, we use uintptr_t to represent the integer representation of a virtual pointer. We use an actual pointer (char *) to creata a pointer representation of a virtual pointer. 
+ Present Bit: The present bit indicates if the table entry is there. If it isn't there, the bit will be 0 and it's as if nothing has been allocated to that entry. 
+ Navigating through memory: Adding 1 to an pointer increments the pointer by 1 address, which represents 1byte in memory. 
+ In mem_init, we initialize the mapping through page tables. During this phase of mapping, we manually setup page tables for the appropriate behavior. Because this mapping won't change, we don't actually use maps (and just don't allocate these portions of memory to the pages array). 
+ Memory Mapping Convention: When we increment through the physical memory, we use size. Size is the amount of memory allocated and when added to the beginning of any page points to the beginning of another page (which will not be allocted). So, (*beginning_of_page)+PGSIZE = (*beginning_of_next_page). 

To Do: 
+ In boot_alloc, allocate initially chunk large enough to hold n bytes. 

Questions/ToDo: 
+ Kernel memory mapping: Is the Kernel memory mapping direct, with just a built in offset. 
+ How does panic and warn get file and line number? 
+ How do we have virtual addresses mapping to physical addresses without page tables (when we just subtract KERNBASE or before page tables are setup). When we then setup page tables, do we add these into the page tables or do we continue just subtracting kernbase? When does this switch happen. 
+ If the kernel starts at 0x8..., why do we subtract 0xf instead? 
+ ***** During boot_map_region, we don't allocate pages? Can you talk more about this? 
+ boot_alloc: my boot_alloc works differently! The way other people do it is returns the first address and updates the freepointer. 

Possible Bugs: 
+ Do I extract page numbers correctly?  
+ Check my out of memory definition in boot_alloc
+ When we create the directory table, we give it the max possible permissions. 

Challenge Note: 
+ I can only set permissions for values that have the present flag set. Should be able to do it for any PTE. 
+ For mapping VA, it should show the page directory as well. Also, should I show information for pages that don't exist. 
+ Should I return -1 when wrong values are input. 
+ For mappings, allow to only put in a single virtual address (instead of forcing us to put in ranges). 
+ Implement a debug variable. 
+ Important: Draw out a table of physical and virtual memory!
+ Take notes on question 6. Very important!
+ Challenge: I am pretty sure that we cannot dumpe a physical address range. We cannot necesarily addres the
+ For getting memory, I call walk_pgdir for each address. This is not the most efficient implementation. 
+ Write notes on why we don't have any pages allocated for kernel static pages. No pages added to pages array. 
+ Loop through physical addresses. 
