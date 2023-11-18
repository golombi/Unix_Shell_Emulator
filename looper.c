#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

void signalHandler(int sig);

int main(int argc, char **argv){ 

	signal(SIGINT, signalHandler);
	signal(SIGCONT, signalHandler);
	signal(SIGTSTP, signalHandler);

	printf("Starting the program\n");


	while(1) {
		//printf("Looping!\n");
		sleep(1);
	}

	return 0;
}

void signalHandler(int sig){
	switch (sig)
	{
	case SIGTSTP:
		signal(SIGCONT, signalHandler);
		break;
	case SIGCONT:
		signal(SIGTSTP, signalHandler);
		break;
	case SIGINT:
		printf("Signal %s received\n", strsignal(sig));
		exit(0);
	default:
		break;
	}
	printf("Signal %s received\n", strsignal(sig));
	signal(sig, SIG_DFL);
 	raise(sig);
}