#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>


int ex(char *args[])
{
	int i;
	extern char **environ;

		/* 没有输入命令 */
        if (!args[0])
           return 0;

        /* 内建命令 */
        if (strcmp(args[0], "cd") == 0) {
            if (args[1]) 
                if (chdir(args[1])!= 0)
					perror("chdir");
            return 0;
        }

        if (strcmp(args[0], "pwd") == 0) {
            char wd[4096];
            puts(getcwd(wd, 4096));
            return 0;
        }

        if (strcmp(args[0], "exit") == 0)
            return -1;

		if (strcmp(args[0], "env") == 0) {
			for(i=0;*(environ+i);i++)
				printf("%s\n",*(environ+i));
			return 0;
		}

		if (strcmp(args[0], "export") == 0) {
			if(args[1]) {
				for(i=0;args[1][i]!='=' && args[1][i]!='\0';i++)
					;
				if(args[1][i]=='\0')
				{
					perror("error");
					printf("Usage:export <name>=<value>\n");
					return 0;
				}
				args[1][i]='\0';
				setenv(args[1],args[1]+i+1,1);
			}
			else {
			perror("error");
			printf("Usage:export <name>=<value>\n");
			}
			return 0;
		}

		if (strcmp(args[0], "echo") == 0) {
			if (args[1]) {
				int replace=0;
				int j;
				for(i=1;args[i];i++)
				{
					/*  是否需要替代变量  */
					for(j=0;args[i][j];j++)
					{
						if (args[i][j] == '$') {
							replace=1;
							break;
						}
						if (args[i][j] == '\'') {
							replace=0;
							break;
						}
					}
					if (replace == 1) {
						/*  找到替代的变量并打印  */
						char *s=&args[i][j+1];
						char *out=NULL;
						if ((out=getenv(s)) != NULL)
							printf("%s ",out);
					}
					else 
					{
						for(j=0;args[i][j];j++) {
							if (args[i][j] == '\'' || args[i][j] == '\"')
								continue;
							printf("%c",args[i][j]);
						}
						printf(" ");
					}
				}
			}
			printf("\n");
			return 0;
		}
		
        /* 外部命令 */
        pid_t pid = fork();
        if (pid == 0) {
            /* 子进程 */
            execvp(args[0], args);
            /* execvp失败 */
            return 255;
        }
        /* 父进程 */
        wait(NULL);
}



int main()
{
	int i,j;
	int flag;				//计算管道的数量
	int loc[8];				//管道的位置
	extern char **environ;
	int flagin;
	extern FILE *stdin;
	char cmd[256];
    char *args[128];
    while(1) 
    {
		printf("# ");
		fflush(stdin);
		fgets(cmd, 256, stdin);

		/* 将<Enter>去除，结尾应为\0 */
    	for (i = 0; cmd[i] != '\n' && cmd[i]!='\0'; i++)
			;
		cmd[i] = '\0';

		/* 将空格全部去掉，并将指令分割开 */
		args[0] = cmd;
	    for (i = 0; *args[i]; i++)
		{
			args[i+1]=args[i]+1;
			while(*args[i+1]!= ' '&& *args[i+1])
				args[i+1]++;
			while(*args[i+1] == ' ') {	
				*args[i+1]='\0';
				args[i+1]++;
			}
		}
   		args[i] = NULL;

		/* 判断是否需要引入管道 */
		flag=0;
		flagin=0;
		loc[0]=0;
		for(j=1;j<i;j++) {
			if (strcmp(args[j],  "|") == 0) { 
				flag++;
				args[j]=NULL;
				loc[flag]=j+1;
			}
		}

		int savefd[2];
		int pipefd[2];
		savefd[0]=dup(STDIN_FILENO);
		savefd[1]=dup(STDOUT_FILENO);
		
		for(i=0;i<flag;i++) {
    	   pid_t cpid;
		   flagin=1;
           if (pipe(pipefd) == -1) {
               perror("pipe");
               exit(EXIT_FAILURE);
           }

           cpid = fork();
           if (cpid == -1) {
               perror("fork");
               exit(EXIT_FAILURE);
           }

           if (cpid == 0) {    					// 子进程写入数据流 
               close(pipefd[0]);         		 // Close unused read end 
			   dup2(pipefd[1],STDOUT_FILENO);	
			   if (ex((args+loc[i])) == -1)
				   break;
			   close(pipefd[0]);
			   _exit(EXIT_SUCCESS);
           }
			
		   /* 父进程从管道读取数据 */ 
        	close(pipefd[1]); 
			dup2(pipefd[0],STDIN_FILENO);
			if (ex((args+loc[flag])) == -1)
			   break;
			flag--;
			close(pipefd[0]);
			wait(NULL);
		}
		/* 恢复父进程的输入方向 */
		dup2(savefd[0],STDIN_FILENO);
		
		/* 输入流>的重定向 */
		int in=0;
		for(j=1;args[j];j++) 
			if (strcmp(args[j],  ">") == 0) { 
				in++;
				args[j]=NULL;
				break;
			}

		if (in > 0) {
		   pid_t cpid;
		   flagin=1;
		   int pipefd[2];

		   if (pipe(pipefd) == -1) {
               perror("pipe");
               exit(EXIT_FAILURE);
           }

           cpid = fork();
           if (cpid == -1) {
               perror("fork");
               exit(EXIT_FAILURE);
           }

           if (cpid == 0) {    					// 子进程写入数据流 
               close(pipefd[0]);         		 // Close unused read end 
			   dup2(pipefd[1],STDOUT_FILENO);	
			   if (ex(args) == -1)
				   break;
			   close(pipefd[0]);
			   _exit(EXIT_SUCCESS);
           }
		   
		   /* 父进程从管道读取数据 */ 
        	close(pipefd[1]); 
			dup2(pipefd[0],STDIN_FILENO);
			char mess[128];
			int n;
			int fd;
			if ((fd=open(*(args+j+1),O_CREAT|O_RDWR|O_TRUNC,S_IRWXU | S_IRWXG | S_IROTH )) !=-1) {
				while((n=read(pipefd[0],mess,128)) > 0)
					write(fd,mess,n);
			}
			else {
				perror("Open file");
			}
			close(pipefd[0]);
			wait(NULL);
		}

		/* 恢复父进程的输入方向 */
		dup2(savefd[0],STDIN_FILENO);
		dup2(savefd[1],STDOUT_FILENO);

		if (flagin)
			continue;
		if (ex(args)== -1)
			break;
	}
}
