#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#define MX_SZ 100
#define CM_SZ 50
int execute_in_parallel(char *infile, char *outfile)
{
	//! path handling
	char* paths = getenv("CS330_PATH");
	if(!paths){
		exit(-1);
	}
	char* path[(int)strlen(paths)];
	int npath =0;
	char*token = strtok(paths,":");
	while(token!= NULL){
		char tok[(int)strlen(token)+1];
		strcpy(tok,token);
		strcat(tok,"/");
		path[npath] = (char*)malloc((strlen(tok)+1)*sizeof(char));
		strcpy(path[npath], tok);
		npath++;
		token = strtok(NULL,":");
	}
	//! path ends
	//! get commands
	FILE* fp = fopen(infile, "r");
	if(!fp){
		perror("error opening input file");
		exit(-1);
	}
	char *cmds[CM_SZ];
	int ncmd =0;
	char str[MX_SZ];
	while(fgets(str, MX_SZ, fp)){
		int len = strlen(str);
		str[len-1] = '\0';
		cmds[ncmd] = (char*)malloc((len+1)*sizeof(char));
		strcpy(cmds[ncmd], str);
		ncmd++;
	}

	//! end scanning commands
	pid_t pid = getpid();
	pid_t cpid;
	int outfd = open(outfile, O_WRONLY | O_CREAT);
	if(outfd <0){
		perror("can't create or open output file");
		exit(-1);
	}
	int pfd[ncmd][2];
	for(int i=0;i<ncmd && (pid == getpid());i++){
		char*cmd = cmds[i];
		if(pipe(pfd[i]) < 0){
			perror("pipe can't be processed");
			exit(-1);
		}
		//! create argv[] for execv
		char*  argv[(int)strlen(cmd) +1];
        int narg =0;
        char *arg = strtok(cmd," ");
        while(arg!=NULL){
			argv[narg] = (char*)malloc(((int)strlen(arg) +1)*sizeof(char));
			argv[narg] = arg;
			arg = strtok(NULL," ");
			narg++;
        }
        argv[narg] = NULL;
		//! argv[] done
		cpid = fork();
		if(cpid <0 ){
			perror("Fork can't be processed");
			exit(-1);
		}
		if(!cpid){
			if(dup2(pfd[i][1],1) < 0){
				perror("Dup2 didn't work");
				exit(-1);
			}
			for(int i=0;i<npath;i++){
				char*curr_path = (char*)malloc((strlen(path[i])+1)*sizeof(char));
				strcpy(curr_path,path[i]);
				strcat(curr_path, argv[0]);
				execv(curr_path, argv);
			}
			if(write(1, "UNABLE TO EXECUTE\n", 18) <0){
				perror("Cannot write for invalid commands");
				exit(-1);
			}
			exit(-1);
		}
	}
	wait(NULL);
	for(int i=0;i<ncmd;i++){
		char buff[4096];
		ssize_t buffsz = read(pfd[i][0] , buff,4096);
		if(buffsz < 0 ){
			perror("Some child processes didn't go well");
			exit(-1);
		}
		buff[buffsz] = '\0';
		if(write(outfd, buff , buffsz) < 0){
			perror("Can't write to output file");
			exit(-1);
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	return execute_in_parallel(argv[1], argv[2]);
}
