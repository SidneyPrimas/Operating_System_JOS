Notes: 
+ QEMU's User Mode Network Stack: This is the network stack we leverage inside of QEMU. This provides a virtual router as well. JOS's IP has not meaning outside of the virtual network built by QEMU. QEMU connecs port on host machine with a port in JOS, and shuttles data back and forth to connecto to the real host. 
+ Network Stack: Instead of writing the network stack from scratch, we use 1wIP, which is an open source TCP/IP protocol suite that includes a network stack. 
+ User environments use calls in lib/sockets.c (which uses a filedescriptor based API) to interact with server. 1wIP maintains unique ID's for all open sockets. Use info in struct Fd to map the file descriptors for each environment to unique ID spaces. 
+ Since BSD socket call like accept and recv can block indefinitely, the dispatcher creates a thread and processes the request in the newly created thread. So, every request is threaded. 
+ Output Environment: Takes packets created by lwIP and forwards towrds network drivers. 
+ Input Environment: Environment that pulls the packet out of kernel space, and sends it to the coer server environment. 
+ The Timer Environment: Used by lwIP for network timeouts. 


Possible Issues: 
+ Is it the correct assumption that CPU 0 will always run? Is it the correct implementatino to only increment ticks through CPU 0. 
+ How do we ensure that descriptor lists are 16 byte aligned. 
+ Should we initialize transmit or receive first? 
+ Still not convinced REQVA is a seperate virtual address? For example, do we guarantee to never re-use REQVA? 

To Do: 
+ Currently I allocate an entire page for tx_desc_list. Instead, just keep it as a global varialbe in the e1000.h file, so there is no need to add a page. Just use PADDR to provide the pa to the TBAL register. 
+ Replace sizeof(*e1000_io)
+ Possibly move to 4096 byte buffer to use entire allocated page (instead of 2048 size buffer)
+ Fix syntactic sugar!

Questions: 
+ How does space get safed for global variables? How is this done for the MMU? 
+ How do we make sure our descriptor list is 0 aligned. Assigning a page and using that address has been overly complicated. 
+ Why would the print statement be causing a race conditions? Seems like this is due to compiler optimization! And, only happens when I add anoter system call (no longer happens when I remove a system call). 
+ Is output.c run within the network server environment? is that the only thing run within this environment. 
