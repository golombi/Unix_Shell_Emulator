# Unix_Shell_Emulator
A unix shell emulator writen in c that allows running commands, sending signals, and keeping track of child processes. Includes piping and input/output redirection. 

Usage (was checked, including for memory leaks, on Ubunto):

make
./myshell

Commands can be ran while blocking/not-blocking the shell - Add & at the end of the line for non-blocking mode.

Most commands are ran via a child process created by forking but some are ran by the shell process.
The latter type consists of:

quit - exit the shell.
cd <dir> - change directory (As each process has its own cwd).
showprocs - lists the running, suspended and 'freshly terminated' commands executed by child processes.
stop <pid> - sends SIGINT to the child with appropriate pid.
nap <sec> <pid> - sends SIGTSTP followed by SIGCONT after <sec> seconds to the child with appropriate pid (Non-Blocking).

Piping and IO Redirection examples:
ls | grep my | wc -l | wc -l
