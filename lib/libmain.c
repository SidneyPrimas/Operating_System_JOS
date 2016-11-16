// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, uvpd, and uvpt.

#include <inc/lib.h>

extern void umain(int argc, char **argv);

// Creates a global variable that stores the environment information within the user space. 
const volatile struct Env *thisenv;
const char *binaryname = "<unknown>";

void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	// LAB 3: Your code here.
	
	// As we start a new environment or process, setup the global thisenv variable that is owned by the user space. 
	// The kernel space already has this, but it's in kernel space. 
	// envs is in read memory shared by the Kernel and User (between the two spaces). 
	envid_t envid = sys_getenvid(); 
	// Ensure that we have
	if (envid == 0) {
		panic("libmain (should be warning): Possibly referring to a non-initialized environment or 'curenv'. All enviornments are initialized to 0. \n");
	}
	
	thisenv = &envs[ENVX(envid)];
	if (thisenv->env_status == ENV_FREE || thisenv->env_id != envid) {
		panic("libmain: Environment is either not-active, or we pulled an outdated environment from the envs array (look at the unique identifier of the envid => see envc). \n");
	}

	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}

