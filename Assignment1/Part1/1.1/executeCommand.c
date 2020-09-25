#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <wait.h>
int executeCommand (char *cmd) {
	char *paths = getenv("CS330_PATH");
	char* token = strtok(paths,":");
	pid_t pid,cpid;
	int status;
	char*  argv[(int)strlen(cmd) +1];
	int temp =0;
	char *arg = strtok(cmd," ");
	while(arg!=NULL){
		argv[temp] = (char*)malloc(((int)strlen(arg) +1)*sizeof(char));
		argv[temp] = arg;
		arg = strtok(NULL," ");
		temp++;
	}
	argv[temp] = NULL;
	while(token!= NULL){
		char tok[(int)strlen(token)+1];
		strcpy(tok,token);
		strcat(tok,"/");
		pid = fork();
		if(pid <0 ){
			perror("Fork can't be processed");
			exit(-1);
		}
		if(!pid){
			strcat(tok,argv[0]);
			if(execv(tok,argv)){
				// perror("execv can't be processed");
				exit(-1);
			}
			exit(-1);
		}
		cpid = wait(&status);
		if(!WEXITSTATUS(status))return WEXITSTATUS(status);
		token = strtok(NULL,":");
	}
	printf("UNABLE TO EXECUTE\n");
	return -1;
}

int main (int argc, char *argv[]) {
	return executeCommand(argv[1]);
}
