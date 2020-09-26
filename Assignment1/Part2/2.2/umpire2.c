#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define ROCK        0 
#define PAPER       1 
#define SCISSORS    2 

#define STDIN 		0
#define STDOUT 		1
#define STDERR		2


#include "gameUtils.h"

int getWalkOver(int numPlayers); // Returns a number between [1, numPlayers]


int main(int argc, char *argv[])
{
	if(argc <=1 || argc == 3){
		perror("expected 2 or greater than 3 arguments");
		exit(-1);
	}
	int nround = 10;
	if(argc == 4 && argv[1][0] == '-' && argv[1][1] == 'r'){
		nround = atoi(argv[2]);
	}
	// printf("Number of round: %d\n",nround);
	char* infile;
	if(argc ==2){
		infile = argv[1];
	}else{
		infile = argv[3];
	}
	int infd = open(infile, O_RDONLY);
	if(infd < 0){
		perror("can't open input file or wrong name provided");
		exit(-1);
	}
	char nplay[64], cchar;
	int ppos =0;
	int rdcnt;
	do{
		if((rdcnt = read(infd , &cchar, 1)) < 0){
			perror("can't read input files");
			exit(-1);
		}
		nplay[ppos++] = cchar;
	}while(rdcnt && cchar != '\n');
	nplay[ppos-1] = '\0';
	int nplayers = atoi(nplay);
	// printf("Number of players: %d\n" , nplayers);
	char executables[nplayers][101];
	for(int i=0;i<nplayers;i++){
		int npos =0;
		
		do{
			if((rdcnt = read(infd , &cchar, 1)) < 0){
				perror("can't read input files");
				exit(-1);
			}
			executables[i][npos++] = cchar;
		}while(rdcnt && cchar != '\n');
		executables[i][npos-1] = '\0';
		// printf("%s\n", executables[i]);
	}
	int active[nplayers] , nactive = nplayers;
	for(int i=0;i<nplayers;i++){
		active[i] =1;
	}
	int std_in[nplayers][2], std_out[nplayers][2];
	for(int i=0;i<nplayers;i++){
		if(pipe(std_in[i]) < 0){
			perror("pipe failed");
			exit(-1);
		}
		if(pipe(std_out[i]) < 0){
			perror("pipe failed");
			exit(-1);
		}
	}
	pid_t pid , cpid[nplayers];
	// pid = getpid();
	for(int i=0;i<nplayers;i++){
		cpid[i] = fork();
		if(!cpid[i]){
			close(std_in[i][1]);
			close(std_out[i][0]);
			if(dup2(std_in[i][0],0) < 0){
				perror("error in dup");
				exit(-1);
			}
			if(dup2(std_out[i][1],1) < 0){
				perror("error in dup");
				exit(-1);
			}
			char* paramList[] = {executables[i], NULL};
			if(execv(executables[i] , paramList)){
				perror("execv error");
				exit(-1);
			}
			exit(-1);
		}
	}
	while(nactive!=1){
		char record[nplayers][nround];
		int activePlayer[nactive];
		int pc =0;
		for(int i=0;i<nplayers;i++){
			if(!active[i])continue;
			if(pc < nactive-1)printf("p%d ", i);
			else printf("p%d\n",i);
			activePlayer[pc++] = i;
			for(int k=0;k<nround;k++){
				if(write(std_in[i][1],"GO",3) < 0){
					perror("write error");
					exit(-1);
				}
				if(read(std_out[i][0],&cchar,1) < 0){
					perror("read error; from children pipe");
					exit(-1);
				}
				record[i][k] = cchar;
			}
			record[i][nround] = '\0';
		}
		// printf("\n");
		int wid =-1;
		if(nactive%2)wid = getWalkOver(nactive);
		int pair =0;
		while(pair < pc){
			if(wid == activePlayer[pair]){
				pair++;
				continue;
			}
			int pid1 , pid2;
			pid1 = activePlayer[pair];
			if(wid == activePlayer[pair + 1]){
				pid2 = activePlayer[pair+2];
				pair += 3;
			}else{
				pid2 = activePlayer[pair+1];
				pair += 2;
			}
			int score1 =0, score2 =0;
			for(int rnd =0;rnd<nround;rnd++){
				if(
					(record[pid1][rnd] == '1' && record[pid2][rnd] == '0')||
					(record[pid1][rnd] == '2' && record[pid2][rnd] == '1')||
					(record[pid1][rnd] == '0' && record[pid2][rnd] == '2')
				){score1++;}
				else if(
					(record[pid1][rnd] == '0' && record[pid2][rnd] == '1')||
					(record[pid1][rnd] == '1' && record[pid2][rnd] == '2')||
					(record[pid1][rnd] == '2' && record[pid2][rnd] == '0')
				){score2++;}
			}
			if(score1 == score2){
				active[pid2] =0;
			}else if(score1 > score2){
				active[pid2] =0;
			}else{
				active[pid1] =0;
			}
			nactive--;
		}
	}
	for(int i=0;i<nplayers;i++){
		if(active[i])printf("p%d", i);
	}
	return 0;
}
