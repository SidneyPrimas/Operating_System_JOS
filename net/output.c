#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;


// Note: Runs within network server environment. This is a seperate environment. 
// Note: Tested wtih user environment testoutput.c. 
// Note: ns_envid is the environment id of the environment that set the request. 

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	
	// TODO: 
	// + How Do I check NS_ENVID (does it need to come from that environment)
	// + What virtual address do I use? 
	// + How do I use NSIPCBUF (and how do I use REQVA). 
	// + Check if perm_store is wrong
	// + How do I cast union Nsipc
	// Currently I am not transmiting anytthing in my packet (think I am getting data from other location). 
	
	// Variables
	int r; 
	int output; 
	envid_t *from_env_store = NULL; 
	int *perm_store = NULL; 
	// Va where we receive network data from other environemnts
	char * pci_pg = (char *) REQVA; 
	
	// Map the receive page at REQVA. Same va address used to send data from user environment. 
	if ((r = sys_page_alloc(sys_getenvid(), pci_pg, PTE_U | PTE_W | PTE_P)) < 0) {
		panic("Error in net/output.c: %e \n", r);
	}
	
	
	// Infinit loop that 1) receives data from usr environment and 2) sends it out on the network. 
	for (;;) {
		// ipc_recv (sys call) blocks until environment recieves data. 
		if ((output = ipc_recv(from_env_store, pci_pg, perm_store)) < 0) {
			panic("Error with ipc_recv in net/output.c (%e) \n", output); 
		} 
	
		// Process a Output NSREQ (transmit request) 
		if(output == NSREQ_OUTPUT) {
			
			// Cast the received data into Nsipc union data struct. 
			union Nsipc *nsipc_buffer_p = (union Nsipc *) pci_pg; 
			
			// Transmit data via E1000 Network Card using a system call. 
			// If buffer full, continue to transmit until packet accepted. 
			for (;;) {
				r = sys_transmit_packet(nsipc_buffer_p->pkt.jp_data, nsipc_buffer_p->pkt.jp_len);
				if (r >= 0) {
					//Successful transmission. Handle next packet. 
					break; 
				} else if (r == -E_TX_BUFF_FULL) {
					// NIC buffer full. Need to continute 
					cprintf("NIC Buffer Full. Re-try. \n");
					continue; 
				} else {
					panic("Error in net/output.c. Issue with sys_transmit_packet. \n");
				}
			}
		
		
		// Flag any other requests: used for debugging. 
		} else {
			cprintf("Warn: ipc_recv got a non-network value in net/output.c \n");
			continue; 
		}
	}
}
