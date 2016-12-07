#include "ns.h"
#include <inc/lib.h>
#include <inc/mmu.h>
#include <inc/error.h>

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server (where the network server is the user application using the network)
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	
	// Variables
	int r; 
	int output; 
	// Va to page used with IPC calls. 
	char * pci_pg = (char *) REQVA; 
	size_t size = 0; 
	// Va to page used to network calls to E1000. 
	// Since environment dedicated network communication, we know UTEMP not being used for anything else inthis env. 
	char * net_pg = (char *) UTEMP;
	
	
	// Page used to receive packets from E1000
	// Allocate an entire page (since E1000 receive code allocates an entire page).
	// Other option: Send a array of size max_receive_size 
	if ((r = sys_page_alloc(thisenv->env_id, net_pg, PTE_U | PTE_W | PTE_P)) < 0) {
		panic("Error in net/input.c: %e \n", r);
	}
	
	
	
	// Infinit loop that 1) receives data from E1000 device driver and 2) sends data to user network server. 
	for (;;) {
	
		
		
		// Continually polls the network server, waiting to receive a packet. 
		for (;;) {
			r = sys_receive_packet(net_pg, &size);
			if (r == -E_RX_BUFF_FULL) {
				// Receive buffer is currently full. 
				cprintf("Receive Buffer Full in NIC. Re-try. \n");
				continue; 
			} else if (r < 0) {
				panic("Error in net/input.c. Issue with sys_transmit_packet. (%e) \n", r);
			} 
		}
		
		// Secure page to route data from current user environment to user server program. 
		// We will essentially hand-off the physical version of this page to the server program. 
		if ((r = sys_page_alloc(thisenv->env_id, pci_pg, PTE_U | PTE_W | PTE_P)) < 0) {
			panic("Error in net/input.c: %e \n", r);
		}
		
		// Copy data to page to be sent
		//Copy size and net_pg data into pci_pg with the struct jif_pkt format. 
		// The union Nsipc enclodes jif_pkt and is of size PGSIZE. 
		memcpy(pci_pg, &size, sizeof(int)); 
		memcpy(pci_pg + sizeof(int), net_pg, PGSIZE-sizeof(int));
		
		// TODO: Debugging
		union Nsipc *nsipc_buffer_p = (union Nsipc *) pci_pg; 
		cprintf("Size: %d \n", nsipc_buffer_p->pkt.jp_len );
		cprintf("Data: %x \n", nsipc_buffer_p->pkt.jp_data[0] );
		
		/* Successfully Received packet from System Call */
		// ipc_send puts sys_ipc_send into a while loop to continue to send data until it succeeds. 
		// Blocks until succeeds or fails
		ipc_send(ns_envid, NSREQ_INPUT, pci_pg, PTE_U | PTE_P); 
		
		
		// By 'sending' the page, two environments hold va to the same physical page. 
		// We need to unmap the va in this env to ensure there is no race conditions. 
		if ((r = sys_page_unmap(thisenv->env_id, pci_pg)) < 0) {
				panic("net/input.c: Could not unmap page. \n");
		}
		
	}
	
}
