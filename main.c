#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include "LineParser.h"

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0
typedef struct process{
	cmdLine* cmd; /* the parsed command line*/
	pid_t pid; /* the process id that is running the command*/
	int status; /* status of the process: RUNNING/SUSPENDED/TERMINATED */
	struct process *next; /* next process in chain */
} process;


void execute(cmdLine* line);
void addProcess(process** process_list, cmdLine* cmd, pid_t pid);
void printProcessList(process** process_list);
void freeProcessList(process* process_list);
void updateProcessList(process **process_list);
void updateProcessStatus(process* process_list, int pid, int status);

char debug = 0;
process* process_list = 0;
int main(int argc, char **argv){
	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "-d")==0){
			debug = 1;
		}else{
			fprintf(stderr, "An undefined argument\n");
	       		return 1;
		}
	}

	printf("Starting the program\n");
	char cwd[PATH_MAX];
	char cmd[2048];
	cmdLine *line;
	while(1) {
		if(NULL==getcwd(cwd, PATH_MAX)) return 1;
		fputs(cwd, stdout);fputs("$ ", stdout);
		fgets(cmd, 2048, stdin);if(strcmp(cmd, "quit\n")==0) break;
		if(cmd[1]==0) continue;
		line = parseCmdLines(cmd);
		if(strcmp(line->arguments[0], "cd")==0){
			if(line->argCount!=2 || chdir(line->arguments[1])<0){
				perror("Error");
			}
			freeCmdLines(line);
			continue;
		}else if(strcmp(line->arguments[0], "showprocs")==0){
			if(line->argCount!=1){
				fprintf(stderr, "ERROR: 'showprocs' gets a single argument\n");
			}else{
				printProcessList(&process_list);
			}
			freeCmdLines(line);
			continue;
		}
		int pid = fork();
		int status;
		if(pid){
			if(debug)printf("PID: %d\nCMD: %s\n", pid, cmd);
			addProcess(&process_list, line, pid);
			if(line->blocking){
				waitpid(pid, &status, 0);
			}
		}else{
			execute(line);
			_exit(0);
		}
		line = 0;
	}
	freeProcessList(process_list);
	return 0;
}

void execute(cmdLine* line){
	if(strcmp(line->arguments[0], "nap")==0){
		if(line->argCount!=3){
			fprintf(stderr, "Invalid number of args for nap\n");
		}else{
			int t = atoi(line->arguments[1]);
			int pid = atoi(line->arguments[2]);
			kill(pid, SIGTSTP);
			sleep(t);
			kill(pid, SIGCONT);
		}
	}else if(strcmp(line->arguments[0], "stop")==0){
		if(line->argCount!=2){
			fprintf(stderr, "Invalid number of args for stop\n");
		}else{
			int pid = atoi(line->arguments[1]);
			kill(pid, SIGINT);
		}
	}else if(0>execvp(line->arguments[0], line->arguments)){
		perror("Error");
		_exit(1);
	}
}

void addProcess(process** process_list, cmdLine* cmd, pid_t pid){
	process* link = malloc(sizeof(process));
	link->cmd = cmd;
	link->pid = pid;
	link->status = RUNNING;
	link->next=*process_list;
	*process_list = link;
}

void printProcessList(process** process_list){
	updateProcessList(process_list);
	process* cur = *process_list;
	char status[20];
	printf("PID      Commnad      STATUS\n");
	while(cur!=0){
		status[0]=0;
		switch(cur->status){
			case TERMINATED:
				strcat(status, "Terminated");
				break;
			case RUNNING:
				strcat(status, "Running");
				break;
			case SUSPENDED:
				strcat(status, "Suspended");
				break;
			default:
				strcat(status, "INVALID STATE");
		}
		printf("%d     	%s	%s\n",
		cur->pid, cur->cmd->arguments[0], status);
		cur = cur->next;
	}
	//clean terminated processes:

	cur = *process_list;
	process* prv = 0;
	while(cur>0){
		if(cur->status==TERMINATED){
			if(prv==0){
				*process_list = cur->next;
				cur->next = 0;
				freeProcessList(cur);
				cur = *process_list;
			}else{
				prv->next = cur->next;
				cur->next=0;
				freeProcessList(cur);
				cur = prv->next;
			}
		}else{
			prv = cur;
			cur = cur->next;
		}
	}
}

void freeProcessList(process* process_list){
	if(process_list){
		freeCmdLines(process_list->cmd);
		freeProcessList(process_list->next);
		free(process_list);
	}
}

void updateProcessList(process **process_list){
	process* cur = *process_list;
	int status;
	int pid;
	while(cur!=0){
		pid = waitpid(cur->pid, &status,
				WNOHANG | WUNTRACED | WCONTINUED);
		if(pid>0){ // If waitpid noticed a change in state
			if(WIFEXITED(status) || WIFSIGNALED(status)){
				cur->status = TERMINATED;
			}else if(WIFSTOPPED(status)){
				cur->status = SUSPENDED;
			}else if(WIFCONTINUED(status)){
				cur->status = RUNNING;
			}
		}else if(pid<0){ // If waitpid never saw the process(including now)
			cur->status = TERMINATED;
		}
		cur = cur->next;
	}
}

void updateProcessStatus(process* process_list, int pid, int status){
	while(process_list>0){
		if(process_list->pid==pid){
			process_list->status = status;
			break;
		}
		process_list = process_list->next;
	}
}

/*
typedef struct cmdLine
{
    char * const arguments[MAX_ARGUMENTS];  command line arguments (arg 0 is the command)
    int argCount;		  number of arguments
    char const *inputRedirect;    input redirection path. NULL if no input redirection
    char const *outputRedirect;	   output redirection path. NULL if no output redirection
    char blocking;	  	boolean indicating blocking/non-blocking
    int idx;				 index of current command in
				the chain of cmdLines (0 for the first)
    struct cmdLine *next;	 next cmdLine in chain
} cmdLine;
*/
