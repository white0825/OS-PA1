/**********************************************************************
 * Copyright (c) 2021-2022
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>


#include "types.h"
#include "list_head.h"
#include "parser.h"

/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */
extern struct list_head history;

struct entry{
	struct list_head list;
	char *command;
};

int time_limit=2;
char process_name[64];

int id=0;

void timeout(int sig)
{	
	alarm(0);
	fprintf(stderr, "%s is timed out\n", process_name);
	kill(id, SIGKILL);	
	return;
}

int convert_int(char *string)
{
	int num=0;
	int len=strlen(string);
	int j=1;
	for(int i=len-1;i>=0;i--){
		num+=(string[i]-'0') * j;
		j*=10;
	}
	return num;
}

int process_command(char * command);
int run_command(int nr_tokens, char * const tokens[]);

int find_command(int num)
{
	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;
	
	struct list_head *pos=NULL;
	struct entry *out=NULL;
	
	int i=0;
	char target_command[128];
	list_for_each(pos, &history){
		out=list_entry(pos, struct entry, list);
		if(i==num){
			strcpy(target_command, out->command);			
		}
		i++;
	}
	
	parse_command(target_command, &nr_tokens, tokens);
	
	if(target_command[0]=='!'){
		int new_num=convert_int(tokens[1]);
		return find_command(new_num);
	}
	else{
		return run_command(nr_tokens, tokens);
	}
	
	return -1;
}

int see_history()
{
	struct list_head *pos=NULL;
	struct entry *out=NULL;
	
	int i=0;
	list_for_each(pos, &history){
		out=list_entry(pos, struct entry, list);
		//int len=strlen(out->command);
		fprintf(stderr, "%2d: %s", i, out->command);
		//printf("%c\n",out->command[len]);
		/*if(out->command[len-1]!='\n'){
			fprintf(stderr, "\n");
		}*/
		i++;	
	}
	return 1;
}

/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */
int run_command(int nr_tokens, char * const tokens[])
{
	struct sigaction act;
	act.sa_handler=timeout;
	sigemptyset(&act.sa_mask);
	act.sa_flags=0;
	sigaction(SIGALRM, &act, 0);
	
	strcpy(process_name,tokens[0]);	
	
	if (strcmp(tokens[0], "exit") == 0) return 0;
	
	else if (strcmp(tokens[0], "!") == 0){
		int num=convert_int(tokens[1]);
		
		return find_command(num);
		/*struct list_head *pos=NULL;
		struct entry *out=NULL;
		
		int i=0;
		list_for_each(pos, &history){
			out=list_entry(pos, struct entry, list);
			if(i==num){
				//fprintf(stderr, "%d: %s", i, out->command);
				return process_command(out->command);
				break;
				
			}
			i++;
		}*/
	}

	else if (strcmp(tokens[0], "cd") == 0){
	
		if(nr_tokens==1){
			char *homePath=getenv("HOME");
			chdir(homePath);
			return 1;
		}

		else if(strcmp(tokens[1], "~")==0){
			char *homePath=getenv("HOME");
			chdir(homePath);
			return 1;
		}

		else if(chdir(tokens[1])==0){
			return 1;
		}
		else{
			return -1;
		}
	}
	
	else if (strcmp(tokens[0], "history") == 0){
		return see_history();
	}
	
	else if(strcmp(tokens[0], "timeout") == 0){
		if(nr_tokens==1){
			fprintf(stderr, "Current timeout is %d second\n", time_limit);
			return 1;
		}
		else{
			time_limit=convert_int(tokens[1]);
			if(time_limit==0){
				printf("Timeout is disabled\n");
			}
			else{
				fprintf(stderr, "Timeout is set to %d seconds\n", time_limit);
			}
			return 1;
		}
		
		return -1;
	}
	
	else{
		char command[128]="/bin/";

		if(strcmp(tokens[0], "ls")==0) strcat(command, tokens[0]);
		if(strcmp(tokens[0], "pwd")==0) strcat(command, tokens[0]);
		if(strcmp(tokens[0], "cp")==0) strcat(command, tokens[0]);
		if(strcmp(tokens[0], "echo")==0) strcat(command, tokens[0]);
		if(strcmp(tokens[0], "touch")==0) strcat(command, tokens[0]);
		if(strcmp(tokens[0], "rm")==0) strcat(command, tokens[0]);
		if(strcmp(tokens[0], "sleep")==0) strcat(command, tokens[0]);

		if(strcmp(command, "/bin/")==0) strcpy(command, tokens[0]);
		
		
		pid_t pid;

		pid=fork();
		id=pid;
		
		alarm(time_limit);

		if(pid==0){
			if(execv(command, tokens )==-1){
				fprintf(stderr, "Unable to execute %s\n", tokens[0]);
				return -1;
			}
		
			return -1;
			
		}
		if(pid>0){	
			waitpid(-1, 0, 0);		
			return 1;
		}
	}

	return -1;
}


/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */
void append_history(char * const command)
{
	struct entry *new=NULL;
	new=(struct entry*)malloc(sizeof(struct entry));

	int len=strlen(command);

	new->command=NULL;

	new->command=(char*)malloc(sizeof(char)*(len+1));

	strcpy(new->command, command);
	
	//new->command[len-1]='\0';
	
	//printf("test: %s\n",new->command);

	list_add_tail(&(new->list), &history);
}




/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
int initialize(int argc, char * const argv[])
{
	return 0;
}


/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
void finalize(int argc, char * const argv[])
{

}


/***********************************************************************
 * process_command(command)
 *
 * DESCRIPTION
 *   Process @command as instructed.
 */
int process_command(char * command)
{
	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
}
