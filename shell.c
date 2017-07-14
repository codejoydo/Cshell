#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>

#define CURR_DIR_MAX 200
#define USER_NAME_MAX 200
#define HOST_NAME_MAX 200
#define FILE_NAME_MAX 200
#define TOKEN_MAX 200
#define CMD_MAX 200

int set_background, last, njobs = 0;
pid_t shell_pgid;
static int shell_terminal = 0;

typedef struct s{
	char name[CMD_MAX];
	pid_t pid;
}s;

s job_arr[2000];

void shell_prompt(){

	char raw_curr_dir[CURR_DIR_MAX+1];
	char user_name[USER_NAME_MAX+1];
	char host_name[HOST_NAME_MAX+1];

	getlogin_r( user_name, sizeof(user_name) );
	gethostname( host_name, sizeof(host_name) );
	getcwd( raw_curr_dir, sizeof(raw_curr_dir) );

	char * tokens[TOKEN_MAX], * p;
	int n=0,i;
	for (p = strtok(raw_curr_dir, "/"); p; p = strtok(NULL, "/"))
		tokens[n++] = p;
	if(n==1)
		tokens[1] = " ";

	if( !strcmp(tokens[0],"home") && !strcmp(tokens[1],user_name) ){
		char curr_dir[CURR_DIR_MAX+1] = "~", delimiter[2] = "/";
		for(i=2;i!=n;i++){
			strcat( curr_dir, delimiter);
			strcat( curr_dir, tokens[i]);
		}
		printf("<%s@%s:%s> ", user_name , host_name, curr_dir);
	}
	else{
		char curr_dir[CURR_DIR_MAX+1] = "", delimiter[2] = "/";
		for(i=0;i!=n;i++){
			strcat( curr_dir, delimiter);
			strcat( curr_dir, tokens[i]);
		}
		if( !strcmp(curr_dir,"") )
			strcat( curr_dir, delimiter);
		printf("<%s@%s:%s> ", user_name , host_name, curr_dir);
	}
}

int built_in(char *args[TOKEN_MAX+1], int nargs){

	/* pwd */
	if( !strcmp(args[0],"pwd") ){
		char curr_dir[CURR_DIR_MAX+1];
		getcwd( curr_dir, sizeof(curr_dir) );
		printf("%s", curr_dir);
		return 1;
	}
	/* cd */
	else if( !strcmp(args[0],"cd") ){
		if( nargs==1 ){
			chdir( getenv("HOME") );
		}
		else{
			/* managing relative addresses */
			if( args[1][0]=='~' && (args[1][1]=='/' || args[1][1]=='\0') ){
				chdir( getenv("HOME") );
				args[1][0] = '.';
			}
			chdir( args[1] );
		}
		return 1;
	}
	/* echo */
	else if( !strcmp(args[0], "echo") ){
		if( nargs==1 ){
			return 1;
		}
		else{
			char * s = strtok( args[1], "\"");
			printf("%s\n", s);
		}
	}
	/* exit */
	else if( !strcmp(args[0],"exit") ){
		return 0;
	}
}

void exec_jobs(){
	int i;
	for(i=0;i<njobs;i++)
		printf("[%d] %s [%d]\n", i+1, job_arr[i].name, job_arr[i].pid);
}

void exec_kjob(char ** args, int nargs){
	if(nargs > 3){
		fprintf( stderr ,"error : too many arguments\n");
		return;
	}
	else if(nargs < 3){
		fprintf( stderr ,"error : too few arguments\n");
		return;
	}
	else{
		int job_id = (int)args[1][0]-1-'0';
		if(job_id >= njobs){
			fprintf( stderr ,"error : job id ivalid\n");
			return;
		}
		int sig = (int)args[2][0]-'0';
		pid_t job_pid = job_arr[job_id].pid;
		printf("%d %d\n", job_pid, sig);
		kill(job_pid, sig);
	}
}

void exec_overkill(){
	int i;
	for(i=0;i<njobs;i++)
		kill(job_arr[i].pid, 9);
}

void exec_fg(char ** args, int nargs){
	if(nargs > 2){
		fprintf( stderr ,"error : too many arguments\n");
		return;
	}
	else if(nargs < 2){
		fprintf( stderr ,"error : too few arguments\n");
		return;
	}
	else{
		int i,job_id = (int)args[1][0]-1-'0',job_pid = job_arr[job_id].pid;
		char job_name[CMD_MAX];
		strcpy(job_name, job_arr[job_id].name);
		if(job_id >= njobs){
			fprintf( stderr ,"error : job id ivalid\n");
			return;
		}
		for(i=job_id;i<njobs-1;i++)
			job_arr[i]=job_arr[i+1];
		njobs--;	
		tcsetpgrp (shell_terminal, getpgid(job_pid));
		kill(job_pid,SIGCONT);
		int status;
		waitpid(job_pid, &status, WUNTRACED);
		if(WIFSTOPPED(status)){
			tcsetpgrp(shell_terminal,shell_pgid);
			strcpy(job_arr[njobs].name, job_name);
			job_arr[njobs].pid = job_pid;
			njobs++;
		}
		tcsetpgrp (shell_terminal, shell_pgid);
	}
}

void handler(int signo){
	int pid,status;
	while(1){
		pid = waitpid(WAIT_ANY, &status, WNOHANG);
		if(pid > 0){
			int i,j;
			for(i = 0; i < njobs; i++){
				if(job_arr[i].pid == pid){
					fprintf( stderr ,"%s with pid %d exited normally\n", job_arr[i].name, pid);
					for(j = i; j < njobs-1; j++)
						job_arr[i] = job_arr[i+1];
					njobs--;
					break;
				}
			}

			break;
		}
		else if(pid == 0)
			break;
		else if(pid < 0){
			break;
		}
	}
	return;
}

void not_built_in(char *args[TOKEN_MAX+1], int nargs, int cmdno, int in, int out){

	/* process comand for & */
	/* variable to set whether child process will be foreground or background */
	set_background = 0;

	/* if '&' after command, child process background */
	if(!strcmp(args[nargs-1],"&")){
		set_background = 1;
		args[nargs-1] = NULL;
		nargs--;
	}
	if(args[nargs-1][strlen(args[nargs-1])-1]=='&'){
		set_background = 1;
		char * temp = (char*)malloc(strlen(args[nargs-1])*sizeof(char));
		strncpy(temp,args[nargs-1],strlen(args[nargs-1])-1);
		strcpy(args[nargs-1],temp);
		args[nargs-1][strlen(args[nargs-1])-1] == '.';
		nargs--;
	}

	/* process command for I/O redirection */
	int i,j,OF=0,IF=0,SOF=0,SIF=0,APPEND=0;
	char infile[FILE_NAME_MAX] = ""; 
	char outfile[FILE_NAME_MAX] = "";
	char tp[2];
	/* for out file */
	for(i=0;i<nargs;i++){
		for(j=0;j<strlen(args[i]);j++){
			if(OF){
				tp[0] = args[i][j];
				tp[1] = '\0';
				strcat(outfile,tp);
			}
			if(args[i][j] == '>'){
				if(strlen(args[i]) > 1)
					if(args[i][j+1] == '>')
						APPEND=1;
				OF = (j)?(2):(1);
				args[i][j] = '\0';
			}
		}
		if(OF == 2) OF = 1;
		else if(OF == 1) args[i] = NULL;
	}
	/* for in file */
	for(i=0;i<nargs && args[i]!=NULL;i++){
		for(j=0;j<strlen(args[i]);j++){
			if(IF){
				tp[0] = args[i][j];
				tp[1] = '\0';
				strcat(infile,tp);
			}
			if(args[i][j] == '<'){
				IF = (j)?(2):(1);
				args[i][j] = '\0';
			}
		}
		if(IF == 2) IF = 1;
		else if(IF == 1) args[i] = NULL;
	}

	pid_t process;
	signal( SIGCHLD, handler );
	switch ( process = fork()	){
		case -1:
			perror("Could not create child process");
			exit(-1);
		case 0:
			if(set_background){
				setpgid(0,0);
			}
			signal (SIGINT, SIG_DFL);
			signal (SIGQUIT, SIG_DFL);
			signal (SIGTSTP, SIG_DFL);
			signal (SIGTTIN, SIG_DFL);
			signal (SIGTTOU, SIG_DFL);
			signal (SIGCHLD, SIG_DFL);
			if(cmdno!=last){
				if(in!=0){
					dup2(in,0);
					close(in);
				}
				if(out!=1){
					dup2(out,1);
					close(out);
				}
			}
			if(IF){
				int in_file = open(infile,O_RDONLY);
				dup2(in_file,0);
				close(in_file);
			}
			if(OF){
				int out_file;
				if(APPEND){
					out_file = open(outfile,O_RDONLY | O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
				}
				else{
					out_file = open(outfile,O_RDONLY | O_WRONLY | O_CREAT, S_IRWXU);
				}
				dup2(out_file,1);
				close(out_file);
			}
			if( execvp( *args, args) == -1){
				fprintf( stderr, "%s : command not found\n", args[0]);
				exit(1);
			}
		default:
			if(!set_background){
				int status;
				waitpid(process, &status, WUNTRACED);
				if(WIFSTOPPED(status)){
					tcsetpgrp(shell_terminal,shell_pgid);
					strcpy(job_arr[njobs].name, args[0]);
					job_arr[njobs].pid = process;
					njobs++;
				}
			}
			else{
				strcpy(job_arr[njobs].name, args[0]);
				job_arr[njobs].pid = process;
				njobs++;
			}
	}
	return;
}	

int command(){

	int saved_in=dup(0);
	int saved_out=dup(1);
	char junk;
	char cmd[CMD_MAX+1];
	cmd[0] = '\0';
	scanf("%[^\n]",cmd);
	junk=getchar();
	if(cmd[0] == '\0'){
		return 1;
	}

	/* parsing commands wrt ';' */
	char * commands[CMD_MAX+1], *p;
	int ncommands=0,i,j;
	for (p = strtok(cmd, ";"); p != NULL; p = strtok(NULL, ";"))
		commands[ncommands++] = p;

	/* parsing commands wrt pipe '|' */
	for(i = 0; i != ncommands ; i++){

		char * cmds[TOKEN_MAX+1], * q;
		int ncmds=0;
		for (q = strtok(commands[i], "|"); q != NULL; q = strtok(NULL, "|"))
			cmds[ncmds++] = q;
		cmds[ncmds]='\0';
		last=ncmds-1;
		int fd[2],in=0;

		for(j = 0; j < ncmds; j++){
			pipe(fd);
			if(j!=last) pipe(fd);
			if(j==last) if(in!=0) dup2(in,0);

			/* parse each cmd for it's args */
			char * args[TOKEN_MAX+1], * r;
			int nargs=0;
			for (r = strtok(cmds[j], " \t"); r != NULL; r = strtok(NULL, " \t"))
				args[nargs++] = r;
			args[nargs]='\0';

			if(!strcmp(args[0],"pwd") || !strcmp(args[0],"cd") || !strcmp(args[0], "echo") || !strcmp(args[0],"exit")){
				if(!built_in(args, nargs))
					return 0;
			}
			else if(!strcmp(args[0],"jobs")){
				exec_jobs();
			}
			else if(!strcmp(args[0],"kjob")){
				exec_kjob(args, nargs);
			}
			else if(!strcmp(args[0],"fg")){
				exec_fg(args, nargs);
			}
			else if(!strcmp(args[0],"overkill")){
				exec_overkill();
			}
			else if(!strcmp(args[0],"quit")){
				return 0;
			}
			else{
				not_built_in(args, nargs, j, in, fd[1]);
			}
			if(j!=last) close(fd[1]); /* close write end of pipe for the next command to read from pipe */
			if(j!=last) in = fd[0];
		}
		dup2(saved_in,0);
		dup2(saved_out,1);
	}
	return 1;
}

int main(){

	/* place shell into seperate foreground process group and also ignore signals */
	shell_pgid = getpid();
	setpgid(shell_pgid, shell_pgid);
	tcsetpgrp(shell_terminal, shell_pgid);
	signal (SIGINT, SIG_IGN);
	signal (SIGQUIT, SIG_IGN);
	signal (SIGTSTP, SIG_IGN);
	signal (SIGTTIN, SIG_IGN);
	signal (SIGTTOU, SIG_IGN);
	signal (SIGCHLD, SIG_IGN);

	while(1){
		shell_prompt();
		if(!command())
			break;
	}
	return 0;
}
