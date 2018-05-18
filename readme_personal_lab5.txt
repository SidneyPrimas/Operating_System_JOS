Notes: 
+ Originally, spawn does not copy file descriptors at all (meaning the new environment will have an empty file descriptor list). And, yield does a copy-on-write, meaning that the child process will have a copy of the file descriptors, instead of shared files. 
+ Library operating system: This is the exokernel implementation, where we have a library operating system that are really user environments. 
+ The graphical window of qemu appears as the keyboard input to JOS. The console (the nox interface) appears as a serial port to JOS. Now, we have enabled the keyboard input!

Possible Issues: 
+ In sys_env_set_trapframe, might need to check if eip and esp are allowed.
