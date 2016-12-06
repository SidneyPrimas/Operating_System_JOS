#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

// E1000 parameters
#define E1000_VENDOR_ID 0x8086 
#define E1000_DEVICE_ID 0x100E
// E1000 MAC Address (52:54:00:12:34:56)
#define MAC_LOWER 			0x12005452	
#define MAC_HIGHER			0x00005634


#define n_tx_desc 32
#define tx_desc_size 16
#define n_rx_desc 256
#define rx_desc_size 16
#define max_receive_size 2048

#define max_packet_size 1518

// Functions
int pci_attach_E1000(struct pci_func *pcif); 
int e1000_transmit_packet(void * packet, size_t size); 
int e1000_receive_packet(void * packet, size_t * size); 


struct TX_Desc
{
	uint32_t addr_lower; 
	uint32_t addr_upper; 
	uint16_t length; 
	uint8_t cso; 
	uint8_t cmd; 
	uint8_t status; 
	uint8_t css; 
	uint16_t special; 
}; 



struct RX_Desc
{
	uint32_t addr_lower; 
	uint32_t addr_upper;  
	uint16_t length; 
	uint16_t checksum; 
	uint8_t status; 
	uint8_t errors; 
	uint16_t special; 
};

// Global Variables 
struct TX_Desc tx_desc_list[n_tx_desc];
struct RX_Desc rx_desc_list[n_rx_desc]; 


/* Register Set. (82543, 82544)
 *
 * Registers are defined to be 32 bits and  should be accessed as 32 bit values.
 * These registers are physically located on the NIC, but are mapped into the
 * host memory address space.
 *
 * RW - register is both readable and writable
 * RO - register is read only
 * WO - register is write only
 * R/clr - register is read only and is cleared when read
 * A - register array
 */
#define E1000_CTRL     0x00000  /* Device Control - RW */
#define E1000_CTRL_DUP 0x00004  /* Device Control Duplicate (Shadow) - RW */
#define E1000_STATUS   0x00008  /* Device Status - RO */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */


/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

/* Selected Settings for Transmit Control */
#define E1000_TCTL_COLD_FULL   0x00040000    /* setting collision distance to 40h*/

/* Selected Settings for TIPG  */
#define E1000_TIPG_IPGT 	0x0A
#define IPGT_SHIFT				0x00
#define E1000_TIPG_IPGR1 	0x08
#define IPGR1_SHIFT				0x0A
#define E1000_TIPG_IPGR2 	0x0C
#define IPGR2_SHIFT				0x14

/* Select Settings for Transmit Descriptor Field */
#define E1000_TDESC_CMD_RS 		(0x1<<3)
#define E1000_TXD_CMD_EOP    	(0x1<<0) /* End of Packet */
#define E1000_TXD_STAT_DD    	0x00000001 /* Descriptor Done */

/* Receive Registers */
#define E1000_RAL       0x05400  		/* Receive Address (LOW) - RW Array */
#define E1000_RAH       0x05404 	 	/* Receive Address (HIGH) - RW Array */
#define E1000_RAH_AV    (0x1<<31)		/* Receive Address Address Valid (AV) */
#define E1000_MTA      	0x05200  		/* Multicast Table Array - RW Array */
#define E1000_MTA_SIZE  128  				/* Multicast Table Array - Size of array*/
#define E1000_RDBAL    	0x02800  		/* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    	0x02804  		/* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    	0x02808  		/* RX Descriptor Length - RW */
#define E1000_RDH      	0x02810  		/* RX Descriptor Head - RW */
#define E1000_RDT      	0x02818  		/* RX Descriptor Tail - RW */
#define E1000_RCTL     0x00100  /* RX Control - RW */

/* Masks for Receive Descriptor */
#define E1000_RXD_STAT_DD    		(0x1<<0) /* Descriptor Done */
#define E1000_RXD_STAT_EOP    	(0x1<<1) /* End of Packet */

/* Receive Control */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */


#endif	// JOS_KERN_E1000_H
