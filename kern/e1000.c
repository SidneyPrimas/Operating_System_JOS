#include <kern/e1000.h>
#include <inc/assert.h>
#include <kern/pmap.h>
#include <inc/types.h>
#include <kern/pmap.h>
#include <inc/error.h>
#include <inc/string.h>

// LAB 6: Your driver code here

// Static functions
static int init_transmit(void); 
static int init_receive(void); 

// Base address for the memory mapped io for E1000. 
// We use volatile here since this region in memory can be updated by hardware. 
// Thus, evertime we use any address related to e1000_io, we need to query the memory directly. 
// Essentially, we disable the cache. 
volatile uint32_t *e1000_io;

int pci_attach_E1000(struct pci_func *pcif) {

	int r; 

	// Setups up the E1000 by update pcif struct. 
	pci_func_enable(pcif); 
	
	// Returns the base of the mapped region for the MMIO region.  
	// Since E1000 initialization happens before user environments created with Kern_pgdir, E1000 mmio will be included in all future enveronmnets (in kernel space). 
	// pcif->reg_base include the physical addresses (which represent the IO mappings). To access these through the page table, we need to add the MMU mapping. 
	e1000_io = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	
	// Note: Since e1000_io is an array of 32-bit registers, need to index into with byte_offset/4. 
	int reg_STATUS = E1000_STATUS/sizeof(*e1000_io); 
	cprintf("Status Register Check: %x \n", e1000_io[reg_STATUS]); 
	assert(e1000_io[reg_STATUS] == 0x80080783);
	
	// Init the transmit functionality in E1000.
	if ((r = init_transmit()) < 0) {
		return r; 
	}
		
	// Init the receive functionality in E1000. 
	if ((r = init_receive()) < 0 ) {
		return r; 
	}
	
	return 0; 
}

static int init_receive(void) {
	
	// Program Receive Address Registers (RAL/RAH) with ethernet address
	// RAL0 and RAH0 used to check for Ethernet MAC address of the Ethernet controller. 
	// For these registers, always write low-to-high. 
	int reg_RAL = E1000_RAL/sizeof(*e1000_io); 
	int reg_RAH = E1000_RAH/sizeof(*e1000_io); 
	e1000_io[reg_RAL] = MAC_LOWER;  
	e1000_io[reg_RAH] = E1000_RAH_AV | MAC_HIGHER; // Ensure to indicate address is valid. 
	
	// Initialize the Multicaset Table Array to 0b.
	// The MTA array has 128 entries of 32-bits.
	int reg_MTA = E1000_MTA/sizeof(*e1000_io); 
	memset((void *) &e1000_io[reg_MTA], 0, E1000_MTA_SIZE*sizeof(*e1000_io)); 
	
	// Note: No interrupts enabled. So, IMS, RDTR not modified. 
	
	// Allocate receive descriptor list (must be aligned on a 16-byte boundary). 
	// Ensure list is 16-byte aligned in physical memory. 
	assert((PADDR(&rx_desc_list)%16) == 0); 
	// Program the base address into RDBAL (Recevie Descriptor Base Address Lower) 
	// Must be a physical address since E1000 uses direct memory access. 
	int reg_RDBAL = E1000_RDBAL/sizeof(*e1000_io);
	int reg_RDBAH = E1000_RDBAH/sizeof(*e1000_io); 
	e1000_io[reg_RDBAL] = PADDR(&rx_desc_list);
	e1000_io[reg_RDBAH] = (uint32_t) 0x0;
	
	// Set Receive Descriptor Length register to the size (in bytes)
	// The value must be 128 byte alinged. 
	// Must have atleast 128 receive descriptors (according to lab)
	int reg_RDLEN = E1000_RDLEN/sizeof(*e1000_io); 
	e1000_io[reg_RDLEN] = n_rx_desc * rx_desc_size; 
	
	// Initialize the Receive Descriptor Head and Tail (RDH/RDT).
	// Head should point to the first valid receive descriptor (available to be used by E1000)
	// Tail should point to one descriptor beyond the last valid descriptor. 
	// Tail should be monitored for when it has an updated packet.
	// Important: RDH != RDT or E1000 will think there are no available descriptors. They are only equal when RDH loops around and has filled ALL descriptors. 
	int reg_RDH = E1000_RDH/sizeof(*e1000_io);
	int reg_RDT = E1000_RDT/sizeof(*e1000_io);
	e1000_io[reg_RDH] = (uint32_t) 0x1; 
	e1000_io[reg_RDT] = (uint32_t) 0x0; 
	
	/* POPULATE RECEIVE DESCRIPTOR LIST */
	size_t n; 
	for(n = 0; n < n_rx_desc; n++) {
		struct RX_Desc rx_desc_new;
		struct PageInfo *buffer_page; 
		
		// Create a buffer page: the buffer used to store a packet to be transmitted
		if ((buffer_page = page_alloc(ALLOC_ZERO)) == NULL) {
			panic("init_receive: page_alloc error while creating descriptors. \n");
			return -E_NO_MEM; 
		} 
		
		// Physical address of page has already been mapped to corresponding virtual address (during memory setup). So, no need to re-insert and re-map page. 
		// Make sure to increment page reference since it's in use!
		// Note: Re-inserting is a bad idea since we will remove previous va to pa mapping and replace with this one. 
		buffer_page->pp_ref++; 
		
		
		// Include the physical address of the buffer in the descriptor. 
		// E1000 can use this pa for DMA. And, kernel can find va from this pa (mapped above). 
		rx_desc_new.addr_lower = 	page2pa(buffer_page); 
		rx_desc_new.addr_upper = 	0x0; 
		// Initialize all E1000 set registers to 0x0. 
		// Especially important to initialize status to 0x0 (since EOP and DD must be 0 so software can identify when E1000 has finished working with a packet). 
		rx_desc_new.length = 			0x0;  
		rx_desc_new.checksum = 		0x0; 
		rx_desc_new.status = 			0x0; 
		rx_desc_new.errors = 			0x0; 
		rx_desc_new.special = 		0x0; 
		
		rx_desc_list[n] = rx_desc_new; 
	}
	
	
	// Program the Recieve Control Register (RTCL). Control etherent controller receiver functions. 
	// Note: Configure E1000 to strip CRC
	// Note: Broadcast Accept Mode (BAM) is enabled to accept Broadcast packets. 
	// Note: Use 2048 buffer size for simplicity (as long as above 1518, or a single packet size).
	// Note: BSEX remains at 0 to selected 2048 bytes. 
	// Note: Strip CRC to be compatible with grading script. 
	// Don't need to update: LPE (default), LBM (default), RMTS (interrupts), MO (multicast),  
	int reg_RCTL = E1000_RCTL/sizeof(*e1000_io);
	e1000_io[reg_RCTL] = E1000_RCTL_SECRC | E1000_RCTL_SZ_2048 | E1000_RCTL_BAM | E1000_RCTL_EN; 
	
	return 0; 
}

// Receive single packet using a single descriptor.
// Size is a pointer to the size of the packet in bytes (updated by function)
int e1000_receive_packet(void * packet, size_t * size) {
	// Make sure input arguments have expected values (addresses are not NULL).  
	assert(packet != NULL);
	assert(size != NULL);
	
	//TODO: Debug
	//cprintf("Run e1000_receive_packet \n");
	
	// Use RDT (Receive Descriptor Tail) to determine which descriptor will have next receive packet. 
	int reg_RDT = E1000_RDT/sizeof(*e1000_io);
	int desc_tail_n = e1000_io[reg_RDT];
	int desc_tail_next = (desc_tail_n == n_rx_desc-1) ? 0 : desc_tail_n + 1;
	
	// Read info in next descriptor
	struct  RX_Desc current_desc = rx_desc_list[desc_tail_next];
	
	//TODO: Debug
	//cprintf("Status: %x \n", current_desc.status);
	//int reg_RDH = E1000_RDH/sizeof(*e1000_io);
	//cprintf("Head: %d \n", e1000_io[reg_RDH]);
	//cprintf("Tail: %d \n", e1000_io[reg_RDT]);
	
	// If Descrptor Done bit OR End of Packet (EOP)  NOT set, then descriptor NOT ready to be used. 
	// User must resend data. 
	if (!(current_desc.status & E1000_RXD_STAT_DD) || !(current_desc.status & E1000_RXD_STAT_EOP)) {
		// Debug
		//warn("DD | EOP NOT set. Descriptor still needs to be processed by E1000. \n");
		return -E_RX_BUFF_FULL; 
	}
	
	
	/* NEXT DESCRIPTOR READY TO BE PROCESSED */
	// Update Receive Descriptor Tail (to indicate we are working on new descriptor, and free previous one)
	e1000_io[reg_RDT] = desc_tail_next; 
	
	// Check errors/debugging
	if(current_desc.errors) {
		panic("e1000_receive_packet: packed sent with error: %x \n", current_desc.errors);
	}
	
	assert(current_desc.length <= max_receive_size); 
	
	// TODO: Debugging
	//cprintf("Address: %08x \n", current_desc.addr_lower); 
	//cprintf("Length: %d \n", current_desc.length); 	
	
	// Extract important data (and make available to caller)
	*size = (size_t) current_desc.length;
	memcpy(packet, KADDR(current_desc.addr_lower), *size);
	
	// Reset descriptor variables. 
	current_desc.length = 			0x0;  
	current_desc.status = 			0x0; //Need to reset status for this implementation to work. 
	current_desc.errors = 			0x0; 
	
	// Update the descriptor (in transmit descriptor que). 
	rx_desc_list[desc_tail_next] = current_desc;
	
	
	return 0; 
	
	
}


// Sets up E1000 register for transmit functionality
static int init_transmit(void) {
	
	// Ensure list is 16-byte aligned in physical memory. 
	assert((PADDR(&tx_desc_list)%16) == 0); 

	// Program the base address into TDBAL (Transmit Descriptor Base Address Lower) 
	// Must be a physical address since E1000 uses direct memory access. 
	int reg_TDBAL = E1000_TDBAL/sizeof(*e1000_io);
	int reg_TDBAH = E1000_TDBAH/sizeof(*e1000_io); 
	e1000_io[reg_TDBAL] = PADDR(&tx_desc_list);
	e1000_io[reg_TDBAH] = 0x0; 
	
	// Set the Transmit Descriptor Length (TDLEN) register to the size (in bytes) of the descriptor ring. 
	// Each transmit descriptor is 16 bytes, and needs to be 128 byte aligned. 
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
	
	size_t n; 
	for(n = 0; n < n_tx_desc; n++) {
		struct TX_Desc tx_desc_new;
		struct PageInfo *buffer_page; 
		
		// Create a buffer page: the buffer used to store a packet to be transmitted
		if ((buffer_page = page_alloc(ALLOC_ZERO)) == NULL) {
			panic("init_transmit: page_alloc error while creating descriptors. \n");
			return -E_NO_MEM; 
		} 
		
		// Physical address of page has already been mapped to corresponding virtual address (during memory setup). So, no need to re-insert and re-map page. 
		// Make sure to increment page reference since it's in use!
		// Note: Re-inserting is a bad idea since we will remove previous va to pa mapping and replace with this one. 
		buffer_page->pp_ref++; 
		
		
		// Include the physical address of the buffer in the descriptor. 
		// E1000 can use this pa for DMA. And, kernel can find va from this pa (mapped above). 
		tx_desc_new.addr_lower = page2pa(buffer_page); 
		tx_desc_new.addr_upper = 0x0; 
		// Length: Measred in bytes
		// Max Length: 16288 bytes per descriptor and 16288 bytes total. 
		// The maximum size of an ethernet packet is 1518 bytes. We use this as max. 
		// 1518 bytes fits within PGSIZE. 
		tx_desc_new.length = 0x0; 
		// CSO is optional
		tx_desc_new.cso = 0x0; 
		// CMD is optional: Used to setup Report Status (RS) and Descriptor Done (RD) 
		// DEXT must be set to 0 for Legazy descriptor
		tx_desc_new.cmd = E1000_TDESC_CMD_RS | E1000_TXD_CMD_EOP; 
		// Provides status if RS set in CMD
		// Needs to be set initially, indicating descroptor is ready to use. 
		tx_desc_new.status = E1000_TXD_STAT_DD; 
		// CSS is optional
		tx_desc_new.css = 0x0; 
		// Used to setup tagging information (only works if CTRL.VME is 1b and EOP at 1b in CMD). 
		tx_desc_new.special = 0x0; 
		
		tx_desc_list[n] = tx_desc_new; 
	}	

	return 0; 
}



// Transmite single packet using a single descriptor.
// Size is the size of the packet to be sent in bytes. 
int e1000_transmit_packet(void * packet, size_t size) {
	// Make sure packet we are sending sufficiently small data packets (below max_packet_size)
	if (size > max_packet_size) {
		panic("e1000_transmit_packet: Packet size is too large. \n");
		return -E_NETWORK_GEN; 
	}
	// Note: Included second assert due to concern about additional bytes being automatically added due to header (above actual payload). 
	assert(size <= 1000); 
	
	// Transmit Descriptor Tail Register (TDT): Offset to the next descriptor to be written to. 
	// Get descriptor at the location the transmit descriptor tail is pointing to. 
	int reg_TDT = E1000_TDT/sizeof(*e1000_io);
	int desc_offset = e1000_io[reg_TDT];
	struct  TX_Desc current_desc = tx_desc_list[desc_offset];

	
	
	// If Descrptor Done bit  NOT set, then descriptor not ready to be used. 
	// User must resend data. 
	if ((current_desc.status & E1000_TXD_STAT_DD) == 0x0) {
		warn("DD NOT set. Descriptor still needs to be processed by E1000. \n");
		return -E_TX_BUFF_FULL; 
	}
	
	// Descriptor available! 
	// Reset DD bit in status. Update the descriptor length. 
	current_desc.status = (current_desc.status & ~E1000_TXD_STAT_DD); 
	current_desc.length = size; 
	
	// Update transmit buffer page with packet data (copy data over). 
	// current_desc.addr_lower provides physical address, so need to convert to *va. 
	memcpy(KADDR(current_desc.addr_lower) ,packet ,size); 
	
	// TODO: Debugging
	//cprintf("Address: %08x \n", current_desc.addr_lower); 
	//cprintf("CMD/CSO/Length: %08x \n", current_desc.cmd << (16 + 8)  | current_desc.cso << 16 | current_desc.length); 
	//cprintf("Special/CSS/Status: %06x \n", current_desc.special << (8 + 8)  | current_desc.css << 8 | current_desc.status); 
	
	// Update the descriptor (in transmit descriptor que). 
	tx_desc_list[desc_offset] = current_desc;
	
	// Update the TDT (after the descriptor is updated) 
	int desc_offset_next = (desc_offset == n_tx_desc-1) ? 0 : desc_offset + 1;
	e1000_io[reg_TDT] = desc_offset_next; 
	
	return 0; 
	
	
}

