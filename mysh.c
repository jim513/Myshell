#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_CMD_ARG 10
#define BUFSIZE 256

const char *prompt = "myshell> ";
char * cmdvector[MAX_CMD_ARG];
char * cmdpdivide[MAX_CMD_ARG];
char *divieded[MAX_CMD_ARG];
char cmdline[BUFSIZE];
static int BACKGROUND = 0;

void fatal();
void cmd_exit();
void cmd_cd(int argc,char *argv[]);
int check_bacground(char *cmd);
int makelist(char *s, const char *delimiters, char ** list, int MAX_LIST);
void wait_child(int sig);
void check_redirect(char *cmd);
void excute(char *temp);

void fatal(char *str){
	perror(str);
	exit(1);
}

void cmd_exit(){
	exit(2);
}

void cmd_cd(int argc, char *argv[]){
	char *path;

	if(argc >1)
		path = argv[1];
	else if((path = (char*)getenv("HOME")) == NULL)
		path=".";

	if(chdir(path) < 0)
		perror("WRONG PATH");
}

int check_background(char *cmd){
	int i;

	for(i = 0; i<strlen(cmd);i++){
		if(cmd[i] == '&')
		{
			cmd[i]= ' ';
			return 1;
		}
	}
	return 0;
}
		
int makelist(char *s, const char *delimiters, char ** list, int MAX_LIST){
	int i=0;
	int numtokens = 0;
	char *snew =NULL;

	if( ( s==NULL ) || (delimiters ==NULL) ) return -1;
	snew = s + strspn(s, delimiters);  /*Skip delimiters*/
	
	if ( (list[numtokens] = strtok(snew , delimiters)) == NULL )
		return numtokens;

	numtokens =1;

	while(1){
		if( (list[numtokens] = strtok(NULL,delimiters)) ==NULL)
			break;
		if(numtokens == (MAX_LIST-1)) return -1;
		numtokens++;
	}
	return numtokens;
}

void wait_child(int sig)
{
	int status;
	pid_t pid;
	while(pid =waitpid(-1,&status,WNOHANG)>0);
}

void check_redirect(char *cmd)
{	
 
	int fd, i ,len;
	char * target;
	len =strlen(cmd);
	
	for( i=len-1; i>=0; i--){

		switch(cmd[i]){
			case '<':
				target = strtok(&cmd[i+1]," \t");
				fd = open(target,O_RDONLY | O_CREAT,0644);
				if(fd < 0)
					fatal("file open error");
				dup2(fd,STDIN_FILENO);
				close(fd);
				cmd[i] = '\0';
				break;
			case '>':
				target = strtok(&cmd[i+1]," \t");	
				fd = open(target,O_WRONLY |O_CREAT| O_TRUNC,0644);
				if(fd < 0)
					fatal("file open error");
				dup2(fd,STDOUT_FILENO);
				close(fd);
				cmd[i] = '\0';
				break;
			default:break;
		}
		
	}
}

void excute(char * temp){
	int i=0;
	int num_cmd;
	int fd[2];

	pid_t pid;
	//UNBLOCK siganl
	sigset_t set;
	sigfillset(&set);
	if(!BACKGROUND)		
		sigprocmask(SIG_UNBLOCK,&set,NULL);

	//pipe divide
	num_cmd = makelist(temp,"|",cmdpdivide,MAX_CMD_ARG);
	for(i=0; i<num_cmd-1;i++){
		pipe(fd);
		switch(pid=fork()){
		case 0: //Child
 			close(fd[0]);
                	dup2(fd[1], STDOUT_FILENO);
			//check redirect
			check_redirect(cmdpdivide[i]);	
			makelist(cmdpdivide[i]," \t",divieded,MAX_CMD_ARG);
			execvp(divieded[0],divieded);//excute
			fatal("excute error");
			break;
		case -1: fatal("pipe fork error");
			break;
		default: //parent
			close(fd[1]);
                	dup2(fd[0], STDIN_FILENO);
			break;
		}
	}
	//if there is only one command
	check_redirect(cmdpdivide[i]);//check redirect	
	if(makelist(cmdpdivide[i]," \t",divieded,MAX_CMD_ARG)<=0)
		fatal("main()");	
	execvp(divieded[0],divieded);
	fatal("excute error");
}
int main(int argc, char **argv){
	int i=0;
	int status;
	pid_t pid;
	char temp[BUFSIZE];

	/* signal to deal with child process termination*/
  	struct sigaction act;        //to call sigaction()
	sigemptyset(&act.sa_mask);   //initialize mask
	act.sa_handler = wait_child; //set handling function
	act.sa_flags = SA_RESTART;   //set flag
	sigaction(SIGCHLD,&act , 0); 

	/* deal with SIGINT, SIGQUIT */
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set,SIGINT);
	sigaddset(&set,SIGQUIT);
	sigprocmask(SIG_BLOCK,&set,NULL); // BLOCK
	
	while(1){
		fputs(prompt,stdout);
		fgets(cmdline,BUFSIZE,stdin);
		cmdline[strlen(cmdline)-1] = '\0';
		
		memcpy(temp,cmdline,strlen(cmdline)+1);
		makelist(cmdline, " \t", cmdvector, MAX_CMD_ARG);
		BACKGROUND=check_background(temp);  //check background

		//check NULL command
		if(cmdvector[0] ==NULL)
			continue;
		
		if(!strcmp("cd",cmdvector[0])){ //change directory
			cmd_cd(2,cmdvector);
			continue;
		}
		if(!strcmp(cmdvector[0],"exit")) //exit
			cmd_exit();		
		
		switch(pid=fork()){
		case 0:
			excute(temp);
		case -1:
			fatal("main()");
		default:	
			if(!BACKGROUND)	
				waitpid(pid, NULL, 0);
			//waitpid(0,&status,WNOHANG);  //kill zombie process
		}
		
	}
	return 0;
}

