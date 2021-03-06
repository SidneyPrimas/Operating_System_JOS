#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

/* system call numbers */
enum {
	SYS_cputs = 0,
	SYS_cgetc,
	SYS_getenvid,
	SYS_env_destroy,
	SYS_page_alloc,
	SYS_page_map,
	SYS_page_unmap,
	SYS_exofork,
	SYS_env_set_status,
	SYS_env_set_trapframe,
	SYS_env_set_pgfault_upcall,
	SYS_yield,
	SYS_ipc_try_send,
	SYS_ipc_recv, //13
	SYS_time_msec,
	SYS_change_priority,
	SYS_transmit_packet, //16
	SYS_receive_packet,  //17
	SYS_get_mac_addr, //18
	NSYSCALLS
};

#endif /* !JOS_INC_SYSCALL_H */
