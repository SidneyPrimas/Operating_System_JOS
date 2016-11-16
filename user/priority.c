// Priority testing script

#include <inc/lib.h>

#define CHILDNUM 10
#define WORK 50

void
umain(int argc, char **argv)
{
	int id, i;
	
	// Set the parent to high priority. 
	int error = sys_change_priority(3);
	if (error < 0) {
		panic("Error \n");
	}
	
	// Create Child
	for (i = 0; i < CHILDNUM; i++) {
		if ((id = fork()) == 0) {
			// Child is executing. 
			int priority = i%3; 
			error = sys_change_priority(priority);
			if (error < 0) {
				panic("Error \n");
			}
			cprintf("Child [%x] Created. Set Priority Level to %d. \n", thisenv->env_id, priority);
			
			
			// Do work to demonstrate priority. 
			int work_size = WORK;  
			int n; 
			for(n = 0; n < work_size; n++) {
				//Yield to demonstrate higher priority children will finish first. 
				sys_yield();
				cprintf("Child [%x], Priority [%d], Work [%d] \n", thisenv->env_id, priority, n);
			
			}
			
			return;
		}
	
	}
	
	// Parent just exits once creating it's children. 
}

