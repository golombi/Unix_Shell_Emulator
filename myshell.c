#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#include "LineParser.h"

#define MAX_CMD_LEN 2048


#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0
typedef struct process{
    cmdLine* cmd; /* the parsed command line*/
    pid_t pid; /* the process id that is
    running the command*/
    int status; /* status of the process:
    RUNNING/SUSPENDED/TERMINATED */
    struct process *next; /* next process in chain */
} process;


pid_t execute(cmdLine *pCmdLine); // Execute in child process
process* addProcess(process** process_list, cmdLine* cmd, pid_t pid); // Add and return process
void printProcessList(process** process_list); // For showprocs use
void freeProcessList(process** process_list); // At the end of usage
void updateProcessList(process **process_list); // Update status and get rid of previously terminated

int ** createPipes(int nPipes);
void releasePipes(int **pipes, int nPipes); // At the end of usage
int *leftPipe(int **pipes, cmdLine *pCmdLine); // Comm with previous process
int *rightPipe(int **pipes, cmdLine *pCmdLine); // Comm with following process

int** pipes = NULL;
int n = 0; // n-1 is the numebr of pipes for n>1

int main(int argc, char* argsv[]){

    FILE* input = stdin;
    FILE* output = stdout;

    //List of existing processes
    process* process_list = NULL;

    //Line input
    char cmdStr[MAX_CMD_LEN];

    //Parsed line input
    cmdLine* curCmdLine;

    pid_t cpid;
    int status;
    
    char process_added;

    while (1){
        process_added=0;

        //Get and print CWD
        if(getcwd(cmdStr, PATH_MAX)==NULL){
            perror("Error: ");
            continue;
        }
        fprintf(output, "%s$ ", cmdStr);
        
        //Read command from input
        if(fgets(cmdStr, MAX_CMD_LEN, input)==NULL){
            perror("Error: ");
            continue;
        }
        if(cmdStr[1]=='\0') continue;

        curCmdLine = parseCmdLines(cmdStr);

        if(strncmp("quit", curCmdLine->arguments[0], MAX_CMD_LEN)==0){
            break;
        }else if(strncmp("cd", curCmdLine->arguments[0], MAX_CMD_LEN)==0){
            //Directory can't be changed via child process as it'll only change ITS directory
            if(chdir(curCmdLine->arguments[1])<0){
                perror("Error: ");
            }
        }else if(strncmp("showprocs", curCmdLine->arguments[0], MAX_CMD_LEN)==0){
            printProcessList(&process_list);
        }else if(strncmp("stop", curCmdLine->arguments[0], MAX_CMD_LEN)==0){
            //Can't use "kill" because of ambiguity with shell
            kill(atoi(curCmdLine->arguments[1]), SIGINT);
        }else if(strncmp("nap", curCmdLine->arguments[0], MAX_CMD_LEN)==0){
            //Stop the process with such pid, sleep, then resume
            //Done via child to not block the shell process
            pid_t nap_pid = fork();
            if(nap_pid==0){
                kill(atoi(curCmdLine->arguments[2]), SIGTSTP);
                sleep(atoi(curCmdLine->arguments[1]));
                kill(atoi(curCmdLine->arguments[2]), SIGCONT);
                _exit(0);
            }else if(nap_pid<0){
                perror("Error: ");
            }
        }else{
            //Count and allocated needed pipes
            n = 0;
            cmdLine* tmp = curCmdLine;
            while(tmp!=NULL){
                n++;
                tmp=tmp->next;
            }
            if(n==0){
                pipes = NULL;
            }else{
                pipes = createPipes(n-1);
            }
            //Execute the command via child process
            process* firstProcess = NULL; 
            cmdLine* itr = curCmdLine; 
            //Execute and add all processes to the list
            while(itr!=NULL){
                cpid = execute(itr);
            
                process* res = addProcess(&process_list, itr, cpid);
                if(firstProcess==NULL)firstProcess=res; //Save the first process in the piping chain
                process_added=1; // Flag that execute was called
                
                itr=itr->next;
                res->cmd->next=NULL; //Prevent multiple frees
            } 
            //Close all pipes to ensure termination and release the resources
            int nc = n;
            while(nc>1){
                close(pipes[nc-2][0]);
                close(pipes[nc-2][1]);
                nc--;
            }
            releasePipes(pipes, n-1);
            
            //Processes are added to the beginning of the list.
            //We use that to access the relevant PIDs and wait.
            process* itrProcess = process_list;
            while(itrProcess!=firstProcess->next){
                //If blocking is set, wait for the child
                if(itrProcess->cmd->blocking>0){
                    waitpid(itrProcess->pid, &status, 0);
                }
                itrProcess=itrProcess->next;
            }
        }
        //If executed wasn't called, free the resource
        if(process_added==0)freeCmdLines(curCmdLine);//Free the cmd line if not saved
    }
    //Free the list memory after use
    freeProcessList(&process_list);
    exit(0);
}

pid_t execute(cmdLine *pCmdLine){
    pid_t pid = fork();
    if(pid==0){
        //Allow piping / input/output redirection by replacing std streams with files
        if(pCmdLine->inputRedirect!=NULL){
            int inputRedirect = open(pCmdLine->inputRedirect, O_RDONLY);
            fclose(stdin);
            dup(inputRedirect);
        }
        if(pCmdLine->outputRedirect!=NULL){
            int outputRedirect = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT, 0777);
            fclose(stdout);
            dup(outputRedirect);
        }
        int* lp = leftPipe(pipes, pCmdLine);
        int* rp = rightPipe(pipes, pCmdLine);
        if(lp!=NULL){
            fclose(stdin);
            dup(lp[0]);
        }
        if(rp!=NULL){
            fclose(stdout);
            dup(rp[1]);
        }

        // Close all pipes that relate to other processes or where duplicated
        while(n>1){
            close(pipes[n-2][0]);
            close(pipes[n-2][1]);
            n--;
        }

        if(0>execvp(pCmdLine->arguments[0], pCmdLine->arguments)){
            perror("Error: ");
            _exit(-1); //Use _exit instead of exit to not close the parent FDs.     
        }
    }else if(pid<0){
        perror("Error: ");
    }
    return pid;
} 

process* addProcess(process** process_list, cmdLine* cmd, pid_t pid){
    process* pr = malloc(sizeof(process));
    pr->cmd=cmd;
    pr->pid=pid;
    pr->status=RUNNING;
    pr->next=*process_list;
    *process_list=pr;
    return pr;
}

//<process id> <the command> <process status>
void printProcessList(process** process_list){
    printf("PID     Command     Status\n");
    updateProcessList(process_list);
    process* cur = *process_list;
    while (cur!=NULL){
        char* statusStr;
        switch (cur->status){
            case RUNNING:
                statusStr = "Running";
                break;
            case SUSPENDED:
                statusStr = "Suspended";
                break;
            default:
                statusStr = "Terminated";
                break;
        }
        printf("%d      %s      %s\n", cur->pid, cur->cmd->arguments[0], statusStr);
        cur=cur->next;
    }
}

void freeProcessList(process** process_list){
    process* prv;
    process* cur = *process_list;
    while(cur!=NULL){
        prv=cur;
        cur=cur->next;
        freeCmdLines(prv->cmd);
        free(prv);
    }
}

void updateProcessList(process **process_list){
    process* cur;
    //Clear previously terminated processes at the beginning of the array
    while((*process_list)!=NULL&&(*process_list)->status==TERMINATED){
        cur = *process_list;
        (*process_list)=(*process_list)->next;
        freeCmdLines(cur->cmd);
        free(cur);
    }
    cur = *process_list;
    process* prv;
    while(cur!=NULL){
        //Clear previously terminated processes 
        if(cur->status==TERMINATED){
            prv->next=cur->next; //Prv is defined as we assured at the beginning
            freeCmdLines(cur->cmd);
            free(cur);
            cur=prv->next;
            continue;
        }

        //WNOHANG make waitpid Non-blocking
        //WUNTRSCED and WCONTINUED are for detecting suspended or continued processes
        pid_t rpid = waitpid(cur->pid, &cur->status, WNOHANG | WUNTRACED | WCONTINUED);
        if(rpid>0){
            if(WIFSIGNALED(cur->status)||WIFEXITED(cur->status)){
                cur->status=TERMINATED;
            }else if(WIFSTOPPED(cur->status)){
                cur->status=SUSPENDED;
            }else if(WIFCONTINUED(cur->status)){
                cur->status=RUNNING;
            }
        }else if(rpid<0){
            //For some reason, on my Ubunto machine the waitpid call fails to detect a child existed:
            // "Error: No child processes"
            cur->status=TERMINATED;
        }
        prv=cur;
        cur=cur->next;
    }
}


int ** createPipes(int nPipes){
    int** pipes;
    pipes=(int**) calloc(nPipes, sizeof(int*));

    for (int i=0; i<nPipes;i++){
        pipes[i]=(int*) calloc(2, sizeof(int));
        pipe(pipes[i]);
    }  
    return pipes;

}
void releasePipes(int **pipes, int nPipes){
    for (int i=0; i<nPipes;i++){
        free(pipes[i]);
    
    }  
    free(pipes);
}
int *leftPipe(int **pipes, cmdLine *pCmdLine){
    if (pCmdLine->idx == 0) return NULL;
    return pipes[pCmdLine->idx -1];
}

int *rightPipe(int **pipes, cmdLine *pCmdLine){
    if (pCmdLine->next == NULL) return NULL;
    return pipes[pCmdLine->idx];
}