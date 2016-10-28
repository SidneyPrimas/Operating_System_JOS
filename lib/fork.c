// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	// Include extra checks that corresponding page is present. 
	// Need to be checked in order with directory first. 
	if ( !(uvpd[PDX((uintptr_t) addr)] & PTE_P) || !(uvpt[PGNUM((uintptr_t) addr)] & PTE_P) || ((uvpt[PGNUM((uintptr_t) addr)] & PTE_AVAIL) != PTE_COW)) {
		panic("pgfault: Error in page-fault %x", uvpt[PGNUM((uintptr_t) addr)]);
	}
	
	if (!(err & FEC_WR)) {
		panic("pgfault: no write error in page fault: %x", err);
	}


	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
		// In the child, allocate a fresh page for the exception stack (that is a copy of the parents exceptions stack). 
	// Create a page in the current env at the page fault address. 
	if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("pgfault: sys_page_alloc: %e", r);
	// Copy data from the old page (the page that faulted) to the temporary page. 
	memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
	// Remap the physical page created in the above sys_page_alloc so that addr points to it. 
	// Currently have two pointers to physical address. 
	if ((r = sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
		panic("pgfault: sys_page_map: %e", r);
	// Remove the temporary mapping (so only have single pointer to the new allocated page). 
	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("pgfault: sys_page_unmap: %e", r);
	

	
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.	
	
	void * addr = (void *) (pn * PGSIZE); 
	
	// If the entry is writable or COW, remap in parent and child to COW. 
	if (((uvpt[pn] & PTE_W) == PTE_W) || ((uvpt[pn] & PTE_AVAIL) == PTE_COW)) {
		// Takes the pte in the parents mapping and copies it over to the same pte in the child's mapping (mapped to the same physical address). 
		if ((r = sys_page_map(0,  addr, envid, addr, PTE_P|PTE_U|PTE_COW)) < 0)
			panic("duppage: sys_page_map: %e", r);
	
		// Our mapping must be made copy-on-write (instead of writable)
		if ((r = sys_page_map(0,  addr, 0, addr, PTE_P|PTE_U|PTE_COW)) < 0)
			panic("duppage: sys_page_map: %e", r);
	}
	else {
		
		// If the entry present, but read-only, still copy it to child!!!
		// Mark as read-only
		if ((r = sys_page_map(0,  addr, envid, addr, PTE_P|PTE_U)) < 0)
			panic("duppage: sys_page_map: %e", r);
	}
	
	
	
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
		
	envid_t envid;
	int r;
	
	// Set the pgfault handler: If an environment is made through fork, pgfault function will be called.
	// This is not a system call (but is a seperate call since we need special precautions the first time it's initialized. 
	// Sets a global variable (so this will always be the handler) with current entry point. Need to change entry point to change handler. 
	set_pgfault_handler(pgfault);

	// Allocate a new child environment.
	// The kernel will initialize it with a copy of our register state,
	// so that the child will appear to have called sys_exofork() too -
	// except that in the child, this "fake" call to sys_exofork()
	// will return 0 instead of the envid of the child.
	// When we create the new environment, we already setup the page direcotry (but not the page tables). 
	envid = sys_exofork();

	if (envid < 0)
		panic("sys_exofork in forkc.: %d", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// We're the parent. Envid is the child's envid. 
	
	// COPY OVER THE UVPT TO CHILD
	// Using duppage, update map of each page below USTACKTOP in both child and parent. 
	unsigned ustacktop_pg = PGNUM(USTACKTOP); 

	unsigned pn; 
	for (pn = 0; pn < ustacktop_pg; pn++) {
		
		// We should awalys have access to the directory. 
		pde_t pde = uvpd[PDX(pn<<PGSHIFT)]; 
		// Check to make sure present (both the pte and pde)
		if ((pde & PTE_P) && (uvpt[pn] & PTE_P)) {
			// Sanity check: ensure in user-space. Need to ensure these sequentially or will throw page fault since directory is not marked as user. 
			assert(pde & PTE_U);
			assert(uvpt[pn] & PTE_U);
			
			// As long as the pte is present, we map it. Figure out permissions in duppage. 
			duppage(envid, pn); 
			
		}
	}
	
	
	
	
	// HANDLE USER EXCEPTION STACK
	// In the child, allocate a fresh page for the exception stack (this is a blank page)
	void * uxstack_bottom = (void *)(UXSTACKTOP-PGSIZE); 
	if ((r = sys_page_alloc(envid, uxstack_bottom, PTE_P|PTE_U|PTE_W)) < 0)
		panic("fork: sys_page_alloc: %e", r);
		
	
	
	//Important: Need to initialize the child's environment to use same entry point after user page fault. 
	// This is an environmental variable that needs to be initialized. 
	extern void _pgfault_upcall();
	sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
	
		
	//Note: At this point, we have mapped the page tables and directory to the child. However, we have not created any of the new pages in the child. 
	
	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("fork: sys_env_set_status: %e", r);

	return envid;
	
	
	
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
