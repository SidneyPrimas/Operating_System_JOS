// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Displays the stacktrace", mon_backtrace},
	{ "showmappings", "Hello World", showmappings}, 
};

/***** Implementations of basic kernel monitor commands *****/
int 
showmappings(int argc, char **argv, struct Trapframe *tf) {

	char *output = argv[1];
	cprintf("Ouput: %s \n", output);
	return 0;
}


int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
// ebp: Position of stack pointer just when the function starts (this is stored in %ebp during function execution). 
// eip: Instrunction address to which control will return when instruction returns (this is the instruction address we return to when current function finishes execution). 
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Read_ebp returns the current value of the ebp register. This first value will not be on the stack. 
	uint32_t current_bp_v = read_ebp();
	// The value of current_bp_v turned into an address is a pointer to the previous bp
	uint32_t *prev_bp_p = (uint32_t *) current_bp_v;
	
	uint32_t final_bp_v = 0x0;
	
	// Print all relevant items within current_bp_p stack frame. 
	while(current_bp_v != final_bp_v) 
	{	
		
		
		// Obtain Instruction Pointer. This is always one address position prior to prev_bp_p.
		// Once we have the correct address of the eip, dereference it to get the value. 
		// When adding an integer to an address/pointer, we offset the bits by the size of the  object that the pointer points to. In this case, that should be 32-bits. 
		uint32_t eip_v = *(prev_bp_p + 1);
		uint32_t *eip_p = (uint32_t *) eip_v;
		
		
		// Implement STAB functionality
		struct Eipdebuginfo info;
		
		//Debuginfo_eip takes the numerical value of the address (not as a poiner)
		debuginfo_eip( (uint32_t) (eip_p), &info);
		
		
		cprintf("ebp %x eip %x args %08x %08x %08x %08x %08x \n", 
			current_bp_v, 
			eip_v, 
			*(prev_bp_p + 2),
			*(prev_bp_p + 3),
			*(prev_bp_p + 4),
			*(prev_bp_p + 5),
			*(prev_bp_p + 6));
		
		// Determine the offset of eip from the first method instruction. 
		int offset = (uint32_t) eip_p- info.eip_fn_addr; 
			
		cprintf("\t %s:%d: %.*s+%d \n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, offset);
			
		
		// Update current stack pointer, and repeat for the next stack frame. 
		current_bp_v = *prev_bp_p;
		prev_bp_p = (uint32_t *) current_bp_v;
	}
	

	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
