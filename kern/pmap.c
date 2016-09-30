/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/kclock.h>

// These variables are set by i386_detect_memory()
size_t npages;			// Amount of physical memory (in pages)
static size_t npages_basemem;	// Amount of base memory (in pages)

// These variables are set in mem_init()
pde_t *kern_pgdir;		// Kernel's initial page directory
struct PageInfo *pages;		// Physical page state array. The page number is the location of page in the pages array. 
static struct PageInfo *page_free_list;	// Free list of physical pages


// --------------------------------------------------------------
// Detect machine's physical memory setup.
// --------------------------------------------------------------

static int
nvram_read(int r)
{
	return mc146818_read(r) | (mc146818_read(r + 1) << 8);
}

static void
i386_detect_memory(void)
{
	size_t basemem, extmem, ext16mem, totalmem;

	// Use CMOS calls to measure available base & extended memory.
	// (CMOS calls return results in kilobytes.)
	basemem = nvram_read(NVRAM_BASELO);
	extmem = nvram_read(NVRAM_EXTLO);
	ext16mem = nvram_read(NVRAM_EXT16LO) * 64;

	// Calculate the number of physical pages available in both base
	// and extended memory.
	if (ext16mem)
		totalmem = 16 * 1024 + ext16mem;
	else if (extmem)
		totalmem = 1 * 1024 + extmem;
	else 
		totalmem = basemem;

	npages = totalmem / (PGSIZE / 1024);
	npages_basemem = basemem / (PGSIZE / 1024);

	cprintf("Physical memory: %uK available, base = %uK, extended = %uK\n",
		totalmem, basemem, totalmem - basemem);
}


// --------------------------------------------------------------
// Set up memory mappings above UTOP.
// --------------------------------------------------------------

static void boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm);
static void check_page_free_list(bool only_low_memory);
static void check_page_alloc(void);
static void check_kern_pgdir(void);
static physaddr_t check_va2pa(pde_t *pgdir, uintptr_t va);
static void check_page(void);
static void check_page_installed_pgdir(void);

// This simple physical memory allocator is used only while JOS is setting
// up its virtual memory system.  page_alloc() is the real allocator.
//
// If n>0, allocates enough pages of contiguous physical memory to hold 'n'
// bytes.  Doesn't initialize the memory.  Returns a kernel virtual address.
//
// If n==0, returns the address of the next free page without allocating
// anything.
//
// If we're out of memory, boot_alloc should panic.
// This function may ONLY be used during initialization,
// before the page_free_list list has been set up.
static void *
boot_alloc(uint32_t n)
{	
	static char *nextfree;	// virtual address of next byte of free memory
	char *result;

	// Initialize nextfree if this is the first time.
	// 'end' is a magic symbol automatically generated by the linker,
	// which points to the end of the kernel's bss segment:
	// the first virtual address that the linker did *not* assign
	// to any kernel code or global variables.
	// BSS includes all uninitialized objects declared at file scope and uninitialized static local variables. 
	if (!nextfree) {
		// End is an array starting at the last address the kernel knows about (including all uninitialized objects). 
		// End is a virtual address in kernel space. 
		extern char end[];
		// Rounds-up to the nearest multiple of PGSize. 
		nextfree = ROUNDUP((char *) end, PGSIZE);
	}

	// Allocate a chunk large enough to hold 'n' bytes, then update
	// nextfree.  Make sure nextfree is kept aligned
	// to a multiple of PGSIZE.
	//
	// Add Code Here
	
	// If we're out of memory, boot_alloc should panic.
	// This function may ONLY be used during initialization,
	// before the page_free_list list has been set up.
	// (npages * PGSIZE) -1: Largest valid address is one below total size of the  physical memory-space (since address starts at 0).
	physaddr_t max_pa = (physaddr_t) (npages * PGSIZE - 1);
	// Since nextfree is in the virtual address space, we need to convert max_pa into the va-space. 
	uintptr_t max_va = (uintptr_t) KADDR(max_pa);
	uintptr_t updated_nextfree = (uintptr_t) (nextfree + n);
	
	if ( updated_nextfree > max_va ) {
		 cprintf("Max_va: %x \nNextfree: %x \n", max_va, updated_nextfree);	
		panic("boot_alloc: No more memory during initialization.\n");
	}
	
	// If n>0, allocates enough pages of contiguous physical memory to hold 'n'
	// bytes.  Doesn't initialize the memory.  Returns a kernel virtual address.
	if (n > 0) {

		// Update nextfree (making sure that nextfree is ketp aligned with a multiple of page size.
		// nextfree+n: Since nextfree is apointer to char, we are moving the forward by singel byte intervals. 
		// ROUNDUP: Command allows us to round to the beginning of the next PAGE. 
		nextfree = ROUNDUP(nextfree + n, PGSIZE);
		
		// Returns a kernel virtual address. 
		// Nextfree is a pointer to a virtual address. 
		return nextfree;
	}
	
	
	// If n==0, returns the address of the next free page without allocating anything.
	if (n == 0) {
		return nextfree;
	}
	
	// All possible conditions should be handled in the above if statments. 
	assert(true);
	return NULL;
	
}

// Set up a two-level page table:
//    kern_pgdir is its linear (virtual) address of the root
//
// This function only sets up the kernel part of the address space
// (ie. addresses >= UTOP).  The user part of the address space
// will be setup later.
//
// From UTOP to ULIM, the user is allowed to read but not write.
// Above ULIM the user cannot read or write.
void
mem_init(void)
{	
	cprintf("***** mem_init\n");
	uint32_t cr0;
	size_t n;

	// Find out how much memory the machine has (npages & npages_basemem).
	i386_detect_memory();

	// Remove this line when you're ready to test this function.

	warn("mem_init: This function is not finished\n");

	//////////////////////////////////////////////////////////////////////
	// create initial page directory.
	// kern_pgdir is placed directly above the end pointer. 
	// kern_pgdir is a virtual address pointer. 
	kern_pgdir = (pde_t *) boot_alloc(PGSIZE);
	// The kernel pgdir consists of a single page, which is zeroed here. 
	memset(kern_pgdir, 0, PGSIZE);

	//////////////////////////////////////////////////////////////////////
	// Recursively insert PD in itself as a page table, to form
	// a virtual page table at virtual address UVPT.
	// (For now, you don't have understand the greater purpose of the
	// following line.)

	// Permissions: kernel R, user R
	// Inserts the physical address of kern_pgdir (with correct configure bits) into the kern_pgdir array at the PDX (page direcotry) of User Virtual Page Table (UVPT). 
	// Essentially, we are adding a pointer to the kernel pgdir physical address within kern_pgdir. 
	kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;
	
	//////////////////////////////////////////////////////////////////////
	// Allocate an array of npages 'struct PageInfo's and store it in 'pages'.
	// The kernel uses this array to keep track of physical pages: for
	// each physical page, there is a corresponding struct PageInfo in this
	// array.  'npages' is the number of physical pages in memory.  Use memset
	// to initialize all fields of each struct PageInfo to 0.
	// Your code goes here:
	
	// Allocate an array PageInfo structs, and update pages with array pointer. 
	// boot_alloc(0) returns a virtual kernel pointer to the current nextfree. We use this to setup the first address of our pages array. 
	pages = (struct PageInfo *) boot_alloc(0);
	// Then, we allocate sufficient memory to store the pages array. 	
	boot_alloc(npages*sizeof(struct PageInfo));
	// Initializes all data (and thus all structures) in pages to 0. 
	memset(pages, 0, npages*sizeof(struct PageInfo));

	//////////////////////////////////////////////////////////////////////
	// Now that we've allocated the initial kernel data structures, we set
	// up the list of free physical pages. Once we've done so, all further
	// memory management will go through the page_* functions. In
	// particular, we can now map memory using boot_map_region
	// or page_insert
	page_init();

	check_page_free_list(1);
	check_page_alloc();
	
	check_page();
	
	return;
	
	//////////////////////////////////////////////////////////////////////
	// Now we set up virtual memory

	//////////////////////////////////////////////////////////////////////
	// Map 'pages' read-only by the user at linear address UPAGES
	// Permissions:
	//    - the new image at UPAGES -- kernel R, user R
	//      (ie. perm = PTE_U | PTE_P)
	//    - pages itself -- kernel RW, user NONE
	// Your code goes here:

	//////////////////////////////////////////////////////////////////////
	// Use the physical memory that 'bootstack' refers to as the kernel
	// stack.  The kernel stack grows down from virtual address KSTACKTOP.
	// We consider the entire range from [KSTACKTOP-PTSIZE, KSTACKTOP)
	// to be the kernel stack, but break this into two pieces:
	//     * [KSTACKTOP-KSTKSIZE, KSTACKTOP) -- backed by physical memory
	//     * [KSTACKTOP-PTSIZE, KSTACKTOP-KSTKSIZE) -- not backed; so if
	//       the kernel overflows its stack, it will fault rather than
	//       overwrite memory.  Known as a "guard page".
	//     Permissions: kernel RW, user NONE
	// Your code goes here:

	//////////////////////////////////////////////////////////////////////
	// Map all of physical memory at KERNBASE.
	// Ie.  the VA range [KERNBASE, 2^32) should map to
	//      the PA range [0, 2^32 - KERNBASE)
	// We might not have 2^32 - KERNBASE bytes of physical memory, but
	// we just set up the mapping anyway.
	// Permissions: kernel RW, user NONE
	// Your code goes here:

	// Check that the initial page directory has been set up correctly.
	check_kern_pgdir();

	// Switch from the minimal entry page directory to the full kern_pgdir
	// page table we just created.	Our instruction pointer should be
	// somewhere between KERNBASE and KERNBASE+4MB right now, which is
	// mapped the same way by both page tables.
	//
	// If the machine reboots at this point, you've probably set up your
	// kern_pgdir wrong.
	lcr3(PADDR(kern_pgdir));

	check_page_free_list(0);

	// entry.S set the really important flags in cr0 (including enabling
	// paging).  Here we configure the rest of the flags that we care about.
	cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_MP;
	cr0 &= ~(CR0_TS|CR0_EM);
	lcr0(cr0);

	// Some more checks, only possible after kern_pgdir is installed.
	check_page_installed_pgdir();
}

// --------------------------------------------------------------
// Tracking of physical pages.
// The 'pages' array has one 'struct PageInfo' entry per physical page.
// Pages are reference counted, and free pages are kept on a linked list.
// --------------------------------------------------------------

//
// Initialize page structure and memory free list.
// After this is done, NEVER use boot_alloc again.  ONLY use the page
// allocator functions below to allocate and deallocate physical
// memory via the page_free_list.
//
void
page_init(void)
{	
	cprintf("***** page_init\n");
	// The example code here marks all physical pages as free.
	// However this is not truly the case.  What memory is free?
	//  1) Mark physical page 0 as in use.
	//     This way we preserve the real-mode IDT and BIOS structures
	//     in case we ever need them.  (Currently we don't, but...)
	//  2) The rest of base memory, [PGSIZE, npages_basemem * PGSIZE)
	//     is free.
	//  3) Then comes the IO hole [IOPHYSMEM, EXTPHYSMEM), which must
	//     never be allocated.
	//  4) Then extended memory [EXTPHYSMEM, ...).
	//     Some of it is in use, some is free. Where is the kernel
	//     in physical memory?  Which pages are already in use for
	//     page tables and other data structures?
	//
	// Change the code to reflect this.
	// NB: DO NOT actually touch the physical memory corresponding to
	// free pages!
	
	// boot_alloc(0) returns the pointer to the first free virtual kernel memory. We cast this to a integer. 
	// We do not skip the page that includes first_free_va. 
	 physaddr_t first_free_va = (physaddr_t) PADDR(boot_alloc(0));

	// Debug
	cprintf("Number of pages %d. \n", npages);
	cprintf("Page at Kernel Page Directory: %d \n", PADDR(kern_pgdir)/PGSIZE);
	cprintf("Page at start of Kernel: %d \n", EXTPHYSMEM/PGSIZE); 
	cprintf("Page at end of kernel + bss + pages: %d \n", first_free_va/PGSIZE - 1);
	
	
	// Initialize all npages into a linked list (with all being free for use). 
	size_t i;
	for (i = 0; i < npages; i++) {
		
		// Count of pointers to this page. Initialized to 0. 
		pages[i].pp_ref = 0;
		
		// Based on information in 1-4, don't allocate certain PageInfo structs in free linked list. 
		//1) Marke physical page 0 as in use.
		if (i == 0) {
			cprintf("Skipped Page 0 \n");
			pages[i].pp_link = NULL;
			continue;
		}
		// Don't allocate pages between [IOPHYSMEM, EXTPHYSMEM)
		else if ( i >= (IOPHYSMEM/PGSIZE) && i < (EXTPHYSMEM/PGSIZE)) {
			cprintf("Skipped I/O. i:%d \n", i);
			pages[i].pp_link = NULL;
			continue;
		}  
		// Skip pages allocated to kernel and other data structures in physical memory (including bss)
		// In physical memory, the kernel begins at EXTPHYSMEM. 
		// The kernel ranges first_free_va/PGSIZE, which is the first free page after the kernel + bss + pages array.  
		else if ( (i >= EXTPHYSMEM/PGSIZE) && i < first_free_va/PGSIZE) {
			cprintf("Skipped pages allocated to kernel + bss + pages array. i:%d \n", i);
			pages[i].pp_link = NULL;
			continue;
		}
		// Other then the exceptions above, include all PageInfo structs in linked list. . 
		else {
			// On first iteration, the link points to nothing or null (thus indicating that it's the end of the linked list). 
			pages[i].pp_link = page_free_list;
			// Saves the location in the PageInfo linked list of the free pages. 
			page_free_list = &pages[i];
		}
	}
	 
	
	
}

//
// Allocates a physical page.  If (alloc_flags & ALLOC_ZERO), fills the entire
// returned physical page with '\0' bytes.  Does NOT increment the reference
// count of the page - the caller must do these if necessary (either explicitly
// or via page_insert).
//
// Be sure to set the pp_link field of the allocated page to NULL so
// page_free can check for double-free bugs.
//
// Returns NULL if out of free memory.
//
// Hint: use page2kva and memset

// page_free_list is a pointer to the first free page in the pages linked list. 
// To allocate this page, we must 1) Mark as used by updating the pp_link of the returned page to null, 2) update page_free_list to the next page in the linked list (removes it from the list), and 3) return the removed paged from the linked list. 
struct PageInfo *
page_alloc(int alloc_flags)
{	
	cprintf("***** page_alloc\n");
	//Return NULL if out of free memory. 
	// When page_free_list is NULL, we have reached the end of the linked list. So, we are out of free memory. 
	if (page_free_list == NULL) {
		assert("Out of free memory.\n");
		return NULL;
	}
	// Fill the entire returned physical page with '\0' bytes. 
	// This is an optional convinience feature for the user. 
	if(alloc_flags & ALLOC_ZERO) {
		// memset operates on kernel virtual addresses. 
		// clear the the physical page that page_free_list points to. 
		memset(page2kva(page_free_list), 0, PGSIZE);
	}
	
	// Save the free page to be returned. 
	struct PageInfo * newPage = page_free_list;
	// Update page_free_list to pop off newPage from the linked list.
	page_free_list = (newPage->pp_link);
	// Mark allocated page as note free.
	newPage->pp_link = NULL;
	//Return the allocated page.
	return newPage;
}

//
// Return a page to the free list.
// (This function should only be called when pp->pp_ref reaches 0.)
// Set the PageInfo at pp to free by adding it to the linked list. 
void
page_free(struct PageInfo *pp)
{		
	cprintf("***** page_free\n");
	// Fill this function in
	// Hint: You may want to panic if pp->pp_ref is nonzero or
	// pp->pp_link is not NULL.
	if (pp->pp_ref != 0) {
		panic("page_free: pp->pp_ref is nonzero. \n");
	}
	
	if (pp->pp_link != NULL) {
		panic("page_fee: pp->pp_link is not NULL. \n");	
	}
	
	// 1) Set pp->pp_link to the current page_free_list pointer
	cprintf("page_free_list: %p \n", page_free_list);
	pp->pp_link = page_free_list;
	// 2) Update page_free_list to point to the pp passed into this fuction.
	page_free_list = pp;
	
}

//
// Decrement the reference count on a page,
// freeing it if there are no more refs.
//
void
page_decref(struct PageInfo* pp)
{	
	cprintf("***** page_decref\n");
	if (--pp->pp_ref == 0) {
		cprintf("page_decref: Remove page with pp_ref %d \n", pp->pp_ref);
		page_free(pp);
		}
}

// Given 'pgdir', a pointer to a page directory, pgdir_walk returns
// a pointer to the page table entry (PTE) for linear address 'va'.
// This requires walking the two-level page table structure.
//
// The relevant page table page might not exist yet.
// If this is true, and create == false, then pgdir_walk returns NULL.
// Otherwise, pgdir_walk allocates a new page table page with page_alloc.
//    - If the allocation fails, pgdir_walk returns NULL.
//    - Otherwise, the new page's reference count is incremented,
//	the page is cleared,
//	and pgdir_walk returns a pointer into the new page table page.
//
// Hint 1: you can turn a PageInfo * into the physical address of the
// page it refers to with page2pa() from kern/pmap.h.
//
// Hint 2: the x86 MMU checks permission bits in both the page directory
// and the page table, so it's safe to leave permissions in the page
// directory more permissive than strictly necessary.
//
// Hint 3: look at inc/mmu.h for useful macros that mainipulate page
// table and page directory entries.
//
// Notes: 
// pgdir is a virtual address pointer that indicates that start the page directory currently used (through CR3). 
// In our implementation, virtual addresses are equal to linear addresses. 
pte_t *
pgdir_walk(pde_t *pgdir, const void *va, int create)
{	
	cprintf("***** pgdir_walk\n");
	// Fill this function in
	// Cast the actual pointer to an int type. 
	uintptr_t va_int = (uintptr_t) va;
	
	// Overview: Walk through the page directory table to find the pointer to the base page table entry for the input linear address. 
	
	
	// 1) If a page table doesn't exist for the va, allocate one, and insert it into pgdir.
	// Get the virtual addresses that references the correct page directory entry. By treating pgdir as an array, we get the value output. 
	pde_t *dir_entry_p = pgdir + PDX(va_int);
	
	// Check if the directory entry flags indicate that the page table is not present. 
	if ( (*dir_entry_p & PTE_P) != PTE_P) {
	
		//If not present and create == false, then return null. 
		if (create == false) {
			return NULL; 
		}
		
		// CREAT NEW PAGE (and insert into directory)
		// Allocate a new page full of 0s. 
		struct PageInfo * newPage = page_alloc(ALLOC_ZERO);
		// If page_alloc fails due to no memory, return NULL.  
		if(!newPage) {
			// Code instructinos indicate this might be possible. 
			warn("pgdir_walk: page_alloc failed to get a new page. \n");
			// Important: Needs to be negative. 
			return NULL;
		} 
				
		// PAGE_ALLOC SUCCEEDED, then: 1) increment newPage ref, 2) insert it into the page table. 
		// 1) When the page is created pp_ref should be incremented. 
		assert(newPage->pp_ref == 0);
		newPage->pp_ref += 1; 
		
		// 2) Insert entry into page table. 
		// Obtain the physical address of New Page
		physaddr_t page_pa = page2pa(newPage);
		// Insert the PDE at the appropriate entry of the page directory defined by the virtual address. 
		// The entry is the PPN (the top 20 bits selected by PTE_ADDR) and the permission flags with Present bit enabled. 
		pgdir[PDX( (uintptr_t) va)] = PTE_ADDR(page_pa) | PTE_U | PTE_W | PTE_P ; 
		
	} 
	
	
	// 2) Get the virtual address that references the correct page table entry. Use information form the page directory to get this. 
	// PGNUM(dir_entry_v): Extracts the page number section from the directory entry. 
	// pages + page_number: Shifts the pages array pointer from the the base to the pointer to PageInfo for page "page_number" 
	// page2kva(pointer to PageInfo): Returns the virtual address that points the the beginning of the page it refers to. 
	pte_t * pte_start_p = (pte_t *) page2kva(pages + PGNUM(*dir_entry_p));
	
	//Return a pointer to the entry in the page table that holds the va mapping. 
	return pte_start_p + PTX(va_int);
}

//
// Map [va, va+size) of virtual address space to physical [pa, pa+size)
// in the page table rooted at pgdir.  Size is a multiple of PGSIZE, and
// va and pa are both page-aligned.
// Use permission bits perm|PTE_P for the entries.
//
// This function is only intended to set up the ``static'' mappings
// above UTOP. As such, it should *not* change the pp_ref field on the
// mapped pages.
//
// Hint: the TA solution uses pgdir_walk
// At the core, boot_map_region allows us to map a virtual address to a physical address that was created during the boot process. In this case, we know that the page has already been created, and we need to artificially link the virtual address to the physical address by update an entry in the page table. 
static void
boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
	// Fill this function in
	
	// Determine the virtual address of the page table entry that the pointer va maps to. 
	pte_t * pt_entry = pgdir_walk(pgdir, (void *) va, true ); 
	
	if (!pt_entry) {
		panic("boot_map_region: We cannot allocated memory for a page directory. ");
	}
	
}

//
// Map the physical page 'pp' at virtual address 'va'.
// The permissions (the low 12 bits) of the page table entry
// should be set to 'perm|PTE_P'.
//
// Requirements
//   - If there is already a page mapped at 'va', it should be page_remove()d.
//   - If necessary, on demand, a page table should be allocated and inserted
//     into 'pgdir'.
//   - pp->pp_ref should be incremented if the insertion succeeds.
//   - The TLB must be invalidated if a page was formerly present at 'va'.
//
// Corner-case hint: Make sure to consider what happens when the same
// pp is re-inserted at the same virtual address in the same pgdir.
// However, try not to distinguish this case in your code, as this
// frequently leads to subtle bugs; there's an elegant way to handle
// everything in one code path.
//
// RETURNS:
//   0 on success
//   -E_NO_MEM, if page table couldn't be allocated
//
// Hint: The TA solution is implemented using pgdir_walk, page_remove,
// and page2pa.
//
int
page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{	
	cprintf("********** page_insert\n");
	// Fill this function in
	
	// 1) If a page table doesn't exist for the va, allocate one, and insert it into pgdir. 
	pte_t * pt_entry = pgdir_walk(pgdir, va, true ); 
	
	// If the page table couldn't be allocated, return error. 
	if(!pt_entry) {
		return -E_NO_MEM; 
	}
	
	// Increment pp_ref. We increment first to handle the corner case. Essentially, we want to avoid freeing before inserting. 
	if (pp->pp_ref > 0) {
		warn("page_insert: Inserted a page that is referenced somewhere else.\n");
	}
	pp->pp_ref += 1;


	
	// 2) Check if there is a page already mapped at va. If there is, remove it. page_remove invalides the TLB for us.  
	// If the entry exists (no matter what entry is), remove it. 
	if (*pt_entry & PTE_P) {
		page_remove(pgdir, va); 
	
		// Debug: Check if the page still exists. 
		if (*pt_entry & PTE_P) {
			panic("page_insert: A page entry that just has been removed still exists. ");
		}
	}
	
	// DEBUG
	// Determine the old (currently in pte) and new PPN (from pp). Used to determine if we are adding the page multiple times to same va mapping. 
	physaddr_t new_page_pa = page2pa(pp);
	physaddr_t old_page_pa = PTE_ADDR(*pt_entry); 
	if (new_page_pa == new_page_pa) {
		cprintf("page_insert (insertin page into same va): link (%p), Ref (%d)\n", pp->pp_link, pp->pp_ref);
	}
	
	
	// ADD PAGE TO PAGE TABLE.  
	// 3) At this point, we know there is no page mapped at va. So, we need to insert the page passed throught  the argument.
	
	
	// Insert the PTE at the appropriate entry of the page table defined by the virtual address. 
	// The entry is the PPN (the top 20 bits selected by PTE_ADDR) and the permission flags with Present bit enabled. 
	*pt_entry = PTE_ADDR(new_page_pa) | perm | PTE_P; 
		
	
	return 0;
}

//
// Return the page mapped at virtual address 'va'.
// If pte_store is not zero, then we store in it the address
// of the pte for this page.  This is used by page_remove and
// can be used to verify page permissions for syscall arguments,
// but should not be used by most callers.
//
// Return NULL if there is no page mapped at va.
//
// Hint: the TA solution uses pgdir_walk and pa2page.
//
struct PageInfo *
page_lookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
	cprintf("***** page_lookup\n");
	// Fill this function in
	
	// 1) If the page table entry exists (the present bit has been set), return a pointer to the page table entry. Otherwise, we get null. 
	pte_t * pt_entry = pgdir_walk(pgdir, va, false ); 
	
	// 2) If the pt_entry doesn't exist, return null.
	// The page table doesn't exist in the directory.  
	if (!pt_entry) {
		return NULL; 
	}
	
	// The entry in the page table doesn't exist. 
	if ( !(*pt_entry & PTE_P)) {
		return NULL; 
	}
	
	// 3) If the pt_entry exists, pass information back to caller. 
	// If caller passed pointer in pte_store, pass back a pointer to the pte. 
	if (pte_store) {
		// pt_entry is the address of the pte. We pass this back to the caller by updating the value that pte_store points to. 
		(*pte_store) = pt_entry;
	}
	
	//Obtain the physical address by taking the top 20-bits of the pte (with PTE_ADDR). Then, add the offset from the virtual address. We could just use &pages[PGNUM(*pte_entry)] directly. 
	physaddr_t pa = PTE_ADDR(*pt_entry) | PGOFF((uintptr_t) va);
	// Obtain the PageInfo struct pointer
	struct PageInfo * page_p = pa2page(pa);
	
	return page_p;
}

//
// Unmaps the physical page at virtual address 'va'.
// If there is no physical page at that address, silently does nothing.
//
// Details:
//   - The ref count on the physical page should decrement.
//   - The physical page should be freed if the refcount reaches 0.
//   - The pg table entry corresponding to 'va' should be set to 0.
//     (if such a PTE exists)
//   - The TLB must be invalidated if you remove an entry from
//     the page table.
//
// Hint: The TA solution is implemented using page_lookup,
// 	tlb_invalidate, and page_decref.
//
void
page_remove(pde_t *pgdir, void *va)
{	
	cprintf("***** page_remove\n");
	// Fill this function in
	
	// 1) If the page table entry exists (the present bit has been set), return a pointer to the page table entry. Otherwise, we get null. Do not create a page table entry. 
	pte_t *pt_entry_p;
	struct PageInfo * page_p = page_lookup(pgdir, va, &pt_entry_p);
	
	cprintf("page_remove: Removing following page: %xpa, \n", page2pa(page_p) );
	cprintf("page_remove: link (%p), Ref (%d)\n", page_p->pp_link, page_p->pp_ref);
	
	
	
	// 2) If there is no page currently mapped at va, silently return
	if (!page_p) {
		warn("page_remove: Tried to remove a page that doesn't exist.");
		return; 
	}
	
	// UNMAP THE PHYSICAL PAGE at the virtual address. 
	// 3) Decrement the ref count on the page. When the refcount reaches 0, free the physical page. This also frees the page!
	page_decref(page_p);

	
	// 5) The page entry corresponding to va should be set to 0. We can also use **pte_store = (pte_t) 0x0.
	memset(pt_entry_p, 0, sizeof(pte_t));
	
	// 6) Invalidate the TLB. At this point, we have removed a value from the page table. 
	tlb_invalidate(pgdir, va);
	
	return;
	
	
	
}

//
// Invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
//
void
tlb_invalidate(pde_t *pgdir, void *va)
{
	// Flush the entry only if we're modifying the current address space.
	// For now, there is only one address space, so always invalidate.
	invlpg(va);
}


// --------------------------------------------------------------
// Checking functions.
// --------------------------------------------------------------

//
// Check that the pages on the page_free_list are reasonable.
//
static void
check_page_free_list(bool only_low_memory)
{
	struct PageInfo *pp;
	unsigned pdx_limit = only_low_memory ? 1 : NPDENTRIES;
	int nfree_basemem = 0, nfree_extmem = 0;
	char *first_free_page;

	if (!page_free_list)
		panic("page_free_list' is a null pointer!");

	if (only_low_memory) {
		// Move pages with lower addresses first in the free
		// list, since entry_pgdir does not map all pages.
		struct PageInfo *pp1, *pp2;
		struct PageInfo **tp[2] = { &pp1, &pp2 };
		for (pp = page_free_list; pp; pp = pp->pp_link) {
			int pagetype = PDX(page2pa(pp)) >= pdx_limit;
			*tp[pagetype] = pp;
			tp[pagetype] = &pp->pp_link;
		}
		*tp[1] = 0;
		*tp[0] = pp2;
		page_free_list = pp1;
	}

	// if there's a page that shouldn't be on the free list,
	// try to make sure it eventually causes trouble.
	for (pp = page_free_list; pp; pp = pp->pp_link)
		if (PDX(page2pa(pp)) < pdx_limit)
			memset(page2kva(pp), 0x97, 128);

	first_free_page = (char *) boot_alloc(0);
	for (pp = page_free_list; pp; pp = pp->pp_link) {
		// check that we didn't corrupt the free list itself
		assert(pp >= pages);
		assert(pp < pages + npages);
		assert(((char *) pp - (char *) pages) % sizeof(*pp) == 0);

		// check a few pages that shouldn't be on the free list
		assert(page2pa(pp) != 0);
		assert(page2pa(pp) != IOPHYSMEM);
		assert(page2pa(pp) != EXTPHYSMEM - PGSIZE);
		assert(page2pa(pp) != EXTPHYSMEM);
		assert(page2pa(pp) < EXTPHYSMEM || (char *) page2kva(pp) >= first_free_page);

		if (page2pa(pp) < EXTPHYSMEM)
			++nfree_basemem;
		else
			++nfree_extmem;
	}

	assert(nfree_basemem > 0);
	assert(nfree_extmem > 0);
}

//
// Check the physical page allocator (page_alloc(), page_free(),
// and page_init()).
//
static void
check_page_alloc(void)
{
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	int nfree;
	struct PageInfo *fl;
	char *c;
	int i;

	if (!pages)
		panic("'pages' is a null pointer!");

	// check number of free pages
	for (pp = page_free_list, nfree = 0; pp; pp = pp->pp_link)
		++nfree;

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(page2pa(pp0) < npages*PGSIZE);
	assert(page2pa(pp1) < npages*PGSIZE);
	assert(page2pa(pp2) < npages*PGSIZE);

	// temporarily steal the rest of the free pages
	fl = page_free_list;
	page_free_list = 0;

	// should be no free memory
	assert(!page_alloc(0));

	// free and re-allocate?
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));
	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(!page_alloc(0));

	// test flags
	memset(page2kva(pp0), 1, PGSIZE);
	page_free(pp0);
	assert((pp = page_alloc(ALLOC_ZERO)));
	assert(pp && pp0 == pp);
	c = page2kva(pp);
	for (i = 0; i < PGSIZE; i++)
		assert(c[i] == 0);

	// give free list back
	page_free_list = fl;

	// free the pages we took
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	// number of free pages should be the same
	for (pp = page_free_list; pp; pp = pp->pp_link)
		--nfree;
	assert(nfree == 0);

	cprintf("check_page_alloc() succeeded!\n");
}

//
// Checks that the kernel part of virtual address space
// has been setup roughly correctly (by mem_init()).
//
// This function doesn't test every corner case,
// but it is a pretty good sanity check.
//

static void
check_kern_pgdir(void)
{
	uint32_t i, n;
	pde_t *pgdir;

	pgdir = kern_pgdir;

	// check pages array
	n = ROUNDUP(npages*sizeof(struct PageInfo), PGSIZE);
	for (i = 0; i < n; i += PGSIZE)
		assert(check_va2pa(pgdir, UPAGES + i) == PADDR(pages) + i);


	// check phys mem
	for (i = 0; i < npages * PGSIZE; i += PGSIZE)
		assert(check_va2pa(pgdir, KERNBASE + i) == i);

	// check kernel stack
	for (i = 0; i < KSTKSIZE; i += PGSIZE)
		assert(check_va2pa(pgdir, KSTACKTOP - KSTKSIZE + i) == PADDR(bootstack) + i);
	assert(check_va2pa(pgdir, KSTACKTOP - PTSIZE) == ~0);

	// check PDE permissions
	for (i = 0; i < NPDENTRIES; i++) {
		switch (i) {
		case PDX(UVPT):
		case PDX(KSTACKTOP-1):
		case PDX(UPAGES):
			assert(pgdir[i] & PTE_P);
			break;
		default:
			if (i >= PDX(KERNBASE)) {
				assert(pgdir[i] & PTE_P);
				assert(pgdir[i] & PTE_W);
			} else
				assert(pgdir[i] == 0);
			break;
		}
	}
	cprintf("check_kern_pgdir() succeeded!\n");
}

// This function returns the physical address of the page containing 'va',
// defined by the page directory 'pgdir'.  The hardware normally performs
// this functionality for us!  We define our own version to help check
// the check_kern_pgdir() function; it shouldn't be used elsewhere.

static physaddr_t
check_va2pa(pde_t *pgdir, uintptr_t va)
{
	pte_t *p;

	pgdir = &pgdir[PDX(va)];
	if (!(*pgdir & PTE_P))
		return ~0;
	p = (pte_t*) KADDR(PTE_ADDR(*pgdir));
	if (!(p[PTX(va)] & PTE_P))
		return ~0;
	return PTE_ADDR(p[PTX(va)]);
}


// check page_insert, page_remove, &c
static void
check_page(void)
{
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	struct PageInfo *fl;
	pte_t *ptep, *ptep1;
	void *va;
	int i;
	extern pde_t entry_pgdir[];

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);

	// temporarily steal the rest of the free pages
	fl = page_free_list;
	page_free_list = 0;

	// should be no free memory
	assert(!page_alloc(0));

	// there is no page allocated at address 0
	assert(page_lookup(kern_pgdir, (void *) 0x0, &ptep) == NULL);

	// there is no free memory, so we can't allocate a page table
	assert(page_insert(kern_pgdir, pp1, 0x0, PTE_W) < 0);

	// free pp0 and try again: pp0 should be used for page table
	page_free(pp0);
	assert(page_insert(kern_pgdir, pp1, 0x0, PTE_W) == 0);
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	assert(check_va2pa(kern_pgdir, 0x0) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp0->pp_ref == 1);

	// should be able to map pp2 at PGSIZE because pp0 is already allocated for page table
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// should be no free memory
	assert(!page_alloc(0));

	// should be able to map pp2 at PGSIZE because it's already there
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// pp2 should NOT be on the free list
	// could happen in ref counts are handled sloppily in page_insert
	assert(!page_alloc(0));
	
	// check that pgdir_walk returns a pointer to the pte
	ptep = (pte_t *) KADDR(PTE_ADDR(kern_pgdir[PDX(PGSIZE)]));
	assert(pgdir_walk(kern_pgdir, (void*)PGSIZE, 0) == ptep+PTX(PGSIZE));

	// should be able to change permissions too.
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W|PTE_U) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);
	assert(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U);
	assert(kern_pgdir[0] & PTE_U);

	// should be able to remap with fewer permissions
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_W);
	assert(!(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U));

	// should not be able to map at PTSIZE because need free page for page table
	assert(page_insert(kern_pgdir, pp0, (void*) PTSIZE, PTE_W) < 0);

	// insert pp1 at PGSIZE (replacing pp2)
	assert(page_insert(kern_pgdir, pp1, (void*) PGSIZE, PTE_W) == 0);
	assert(!(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U));

	// should have pp1 at both 0 and PGSIZE, pp2 nowhere, ...
	assert(check_va2pa(kern_pgdir, 0) == page2pa(pp1));
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp1));
	// ... and ref counts should reflect this
	assert(pp1->pp_ref == 2);
	assert(pp2->pp_ref == 0);
	
	// pp2 should be returned by page_alloc
	assert((pp = page_alloc(0)) && pp == pp2);
	
	// unmapping pp1 at 0 should keep pp1 at PGSIZE
	page_remove(kern_pgdir, 0x0);

	assert(check_va2pa(kern_pgdir, 0x0) == ~0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp2->pp_ref == 0);

	// test re-inserting pp1 at PGSIZE
	assert(page_insert(kern_pgdir, pp1, (void*) PGSIZE, 0) == 0);
	assert(pp1->pp_ref);
	assert(pp1->pp_link == NULL);

	// unmapping pp1 at PGSIZE should free it
	page_remove(kern_pgdir, (void*) PGSIZE);
	assert(check_va2pa(kern_pgdir, 0x0) == ~0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == ~0);
	assert(pp1->pp_ref == 0);
	assert(pp2->pp_ref == 0);

	// so it should be returned by page_alloc
	assert((pp = page_alloc(0)) && pp == pp1);

	// should be no free memory
	assert(!page_alloc(0));

	// forcibly take pp0 back
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	kern_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// check pointer arithmetic in pgdir_walk
	page_free(pp0);
	va = (void*)(PGSIZE * NPDENTRIES + PGSIZE);
	ptep = pgdir_walk(kern_pgdir, va, 1);
	ptep1 = (pte_t *) KADDR(PTE_ADDR(kern_pgdir[PDX(va)]));
	assert(ptep == ptep1 + PTX(va));
	kern_pgdir[PDX(va)] = 0;
	pp0->pp_ref = 0;

	// check that new page tables get cleared
	memset(page2kva(pp0), 0xFF, PGSIZE);
	page_free(pp0);
	pgdir_walk(kern_pgdir, 0x0, 1);
	ptep = (pte_t *) page2kva(pp0);
	for(i=0; i<NPTENTRIES; i++)
		assert((ptep[i] & PTE_P) == 0);
	kern_pgdir[0] = 0;
	pp0->pp_ref = 0;

	// give free list back
	page_free_list = fl;

	// free the pages we took
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	cprintf("check_page() succeeded!\n");
}

// check page_insert, page_remove, &c, with an installed kern_pgdir
static void
check_page_installed_pgdir(void)
{
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	struct PageInfo *fl;
	pte_t *ptep, *ptep1;
	uintptr_t va;
	int i;

	// check that we can read and write installed pages
	pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));
	page_free(pp0);
	memset(page2kva(pp1), 1, PGSIZE);
	memset(page2kva(pp2), 2, PGSIZE);
	page_insert(kern_pgdir, pp1, (void*) PGSIZE, PTE_W);
	assert(pp1->pp_ref == 1);
	assert(*(uint32_t *)PGSIZE == 0x01010101U);
	page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W);
	assert(*(uint32_t *)PGSIZE == 0x02020202U);
	assert(pp2->pp_ref == 1);
	assert(pp1->pp_ref == 0);
	*(uint32_t *)PGSIZE = 0x03030303U;
	assert(*(uint32_t *)page2kva(pp2) == 0x03030303U);
	page_remove(kern_pgdir, (void*) PGSIZE);
	assert(pp2->pp_ref == 0);

	// forcibly take pp0 back
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	kern_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// free the pages we took
	page_free(pp0);

	cprintf("check_page_installed_pgdir() succeeded!\n");
}
