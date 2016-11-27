#include <kern/e1000.h>
#include <inc/assert.h>
#include <kern/pmap.h>
#include <inc/types.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here

// Base address for the memory mapped io for E1000. 
// We use volatile here since this region in memory can be updated by hardware. 
// Thus, evertime we use any address related to e1000_io, we need to query the memory directly. 
// Essentially, we disable the cache. 
volatile uint32_t *e1000_io;

int pci_attach_E1000(struct pci_func *pcif) {

	int r; 
	struct PageInfo *tx_desc_page;
	//struct tx_desc_list[n_tx_desc];

	// Setups up the E1000 by update pcif struct. 
	pci_func_enable(pcif); 
	
	// Returns the base of the mapped region for the MMIO region.  
	// Since E1000 initialization happens before user environments created with Kern_pgdir, E1000 mmio will be included in all future enveronmnets (in kernel space). 
	// pcif->reg_base include the physical addresses (which represent the IO mappings). To access these through the page table, we need to add the MMU mapping. 
	e1000_io = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	
	int reg_STATUS = E1000_STATUS/sizeof(*e1000_io); 
	cprintf("Status Register Check: %x \n", e1000_io[reg_STATUS]); 
	assert(e1000_io[reg_STATUS] == 0x80080783);
	
	// Transmit Initialization //
	// Create the transmit descriptor list. 
	// Allocate a full page to hold the transmit descriptor array. 
	if ((tx_desc_page = page_alloc(ALLOC_ZERO)) == NULL) {
		panic("pci_attach_E1000: page_alloc error. \n");
		return -1; 
	} 
	
	// Since we will only access  the tx_desc_list from the kernel, we map the list into the kernel space using page2kva. 
	// the page2kva mapping allows us to directly map the physical and virtual address to each other. 
	// We map this in kern_pgdir so it's propogated to all other environments. 
	if ((r = page_insert(kern_pgdir, tx_desc_page, page2kva(tx_desc_page), PTE_W | PTE_P)) < 0) {
		panic("pci_attach_E1000: page_page_insert error. \n");
		return r; 
	}
	
	// Program the base address into TDBAL (Transmit Descriptor Base Address Lower) 
	// Must be a physical address since E1000 uses direct memory access. 
	int reg_TDBAL = E1000_TDBAL/sizeof(*e1000_io);
	e1000_io[reg_TDBAL] = page2pa(tx_desc_page);
	
	// Set the Transmit Descriptor Length (TDLEN) register to the size (in bytes) of the descriptor ring. 
	// Each transmit descriptor is 16 bytes, and needs to be 128 byte alignted. 
	// So, we will use 32 descriptors = 32*16 = 512 bytes. 
	int reg_TDLEN = E1000_TDLEN/sizeof(*e1000_io);
	e1000_io[reg_TDLEN] =  n_tx_desc * tx_desc_size; 
	
	// Initialize the Transmit Descriptor Length head and Tail (TDH/TDT) to 0. 
	int reg_TDH = E1000_TDH/sizeof(*e1000_io);
	int reg_TDT = E1000_TDT/sizeof(*e1000_io);
	e1000_io[reg_TDH] = (uint32_t) 0x0; 
	e1000_io[reg_TDT] = (uint32_t) 0x0; 
	
	// Initialize the TCTL register
	// 1) Set Enable bit. 
	// 2) Set the pad short packets bit
	// 3) Do not touch collision threshold since in full duplex mode. 
	// 4) Configure Collision Distance. For full dupslect mode, set to 40h. 
	int reg_TCTL = E1000_TCTL/sizeof(*e1000_io);
	e1000_io[reg_TCTL] = E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_COLD_FULL; 
	
	// Program Transmit IPG
	// Use the 802.3 standard
	// IPGT == 10
	// IPGR1 == 8
	// IPGR2 == 12 (but only significant in half-duplex mode)
	int reg_TIPG = E1000_TIPG/sizeof(*e1000_io);
	e1000_io[reg_TIPG] = (E1000_TIPG_IPGR2 << IPGR2_SHIFT) | (E1000_TIPG_IPGR1 << IPGR1_SHIFT) | (E1000_TIPG_IPGT << IPGT_SHIFT);
	
	
	// Next steps: 
	// Create the list/table of transmit descriptros (tx_desc_list). For each descriptor, update the tx_desc in the list. 
	// For each descriptor, allocate a page that functions as the buffer. The desc should have the physical address that points to the corresponding page. 
	// So, each descriptor will have a page allocated to it. 
	
	
	
	return 0; 
}
