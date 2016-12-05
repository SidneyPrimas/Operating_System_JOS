#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;


// Note: Runs within network server environment. This is a seperate environment. 
// Note: Tested wtih user environment testoutput.c. 

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
	struct jif_pkt *pkt = (struct jif_pkt*)REQVA;
	
	if ((r = sys_page_alloc(sys_getenvid(), pkt, PTE_U | PTE_W | PTE_P)) < 0) {
		panic("Error in net/output.c: %e \n", r);
	}
	
	
	// Check for NSREQ_OUTPUT
	for (;;) {
		// ipc_recv (sys call) blocks until environment recieves data. 
		if ((output = ipc_recv(from_env_store, pkt, perm_store)) < 0) {
			panic("Error with ipc_recv in net/output.c (%e) \n", output); 
		} 
		
		if(output != NSREQ_OUTPUT) {
			// Used for debugging
			panic("Careful: ipc_recv got a non-network value in net/output.c \n");
			continue; 
		}
		
		//if (*from_env_store != ns_envid) {
		//	continue; 
		//	panic("Help me again \n");
		//}
		
		// At this point, we received an NSREQ_OUTPUT from the IPC
		union Nsipc *nsipc_test = (union Nsipc *) pkt; 
		
		// Transmit data via E1000 Network Card
		r = sys_transmit_packet(&nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
		if (r < 0) {
			panic("Help me. \n");
		}
	}
}
