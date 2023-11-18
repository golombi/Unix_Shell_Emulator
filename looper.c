#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

void handler(int sig);
int main(int argc, char **argv){

	printf("Starting the program\n");
	while(1) {
			signal(SIGCONT, handler);
	signal(SIGTSTP, handler);
	signal(SIGINT, handler);
		sleep(2);
	}

	return 0;
}

void handler(int sig){
	signal(sig, SIG_DFL);
	/*
	if(sig==SIGCONT){
		signal(SIGTSTP, handler);
	}else if(sig==SIGTSTP){
		signal(SIGCONT, handler);
	}
	*/
	fprintf(stdout, "A signal was recieved: %s\n", strsignal(sig));
	fflush(stdout);
	raise(sig);
}
