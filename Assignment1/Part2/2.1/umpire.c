#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wait.h>


int main(int argc, char* argv[]) {
	if(argc <3){
		perror("Some of the players are not ready to play");
		exit(-1);
	}
	int score1 =0 , score2 =0;
	pid_t pid, cpid1, cpid2;
	// pid = getpid();
	int std1[2][2], std2[2][2];
	for(int i=0;i<2;i++){
		if(pipe(std1[i]) < 0){
			perror("pipe failed");
			exit(-1);
		}
	}
	for(int i=0;i<2;i++){
		if(pipe(std2[i]) < 0){
			perror("pipe failed");
			exit(-1);
		}
	}

	// player 1
	
	for(int i=0;i<10;i++){
		if(write(std1[0][1], "GO",3) < 0){
			perror("write failed");
			exit(-1);
		}
	}
	cpid1 = fork();
	if(cpid1 < 0){
		perror("fork didn't work");
		exit(-1);
	}
	if(!cpid1){
		if(dup2(std1[0][0],0) <0){
			perror("dup2 failed");
			exit(-1);
		}
		if(dup2(std1[1][1],1)<0){
			perror("dup2 failed");
			exit(-1);
		}
		if(execl(argv[1], argv[1], NULL)){
			perror("execl failed");
			exit(-1);
		}
		exit(-1);
	}
	char p1[11];
	for(int i=0;i<10;i++){
		char c;
		if(read(std1[1][0], &c,1) <0){
			perror("read failed");
			exit(-1);
		}
		p1[i] = c;
	}
	p1[10] = '\0';
	
	//player2
	for(int i=0;i<10;i++){
		if(write(std2[0][1], "GO",3) < 0){
			perror("write failed");
			exit(-1);
		}
	}
	cpid2 = fork();
	if(cpid2 <0){
		perror("cpid didn't work");
		exit(-1);
	}
	if(!cpid2){
		close(std2[0][1]);
		close(std2[1][0]);
		if(dup2(std2[0][0],0) <0){
			perror("dup2 failed");
			exit(-1);
		}
		if(dup2(std2[1][1],1)<0){
			perror("dup2 failed");
			exit(-1);
		}
		if(execl(argv[2], argv[2], NULL)){
			perror("execl failed");
			exit(-1);
		}
		exit(-1);
	}
	char p2[11];
	for(int i=0;i<10;i++){
		char d;
		if(read(std2[1][0], &d,1) <0){
			perror("read failed");
			exit(-1);
		}
		p2[i] = d;
	}
	p2[10] = '\0';
	for(int i=0;i<10;i++){
		if((p1[i] == '0' && p2[i] == '1') || (p1[i] == '1' && p2[i] == '2') || (p1[i] == '2' && p2[i] == '0') )score2++;
		else if((p2[i] == '0' && p1[i] == '1') || (p2[i] == '1' && p1[i] == '2') || (p2[i] == '2' && p1[i] == '0') )score1++;
	}
	printf("%d %d", score1, score2);
	return 0;
}
