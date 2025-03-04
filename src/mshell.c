#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"
#include "background_utils.c"
#include "pipeline_utils.c"

int fg_count = 0;

void sigchld_handler(int sig)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if(!is_background_process(pid))
        {
            --fg_count;
        }
        else
        {
            if (buffer_count < STATUS_BUFFER_SIZE)
            {
                status_buffer[buffer_count].pid = pid;  // mod jak chcemy nadpisywac
                status_buffer[buffer_count].status = status;
                ++buffer_count;
                // pozostale ignorujemy
            }
            remove_background_process(pid);
        }
    }
}

void print_status_buffer()
{
    for (int i = 0; i < buffer_count; i++)
    {
        if (WIFEXITED(status_buffer[i].status))
        {
            fprintf(stdout, "Background process (%d) terminated. (exited with status %d)\n", status_buffer[i].pid, WEXITSTATUS(status_buffer[i].status));
        }
        else if (WIFSIGNALED(status_buffer[i].status))
        {
            fprintf(stdout, "Background process (%d) terminated. (killed by signal %d)\n", status_buffer[i].pid, WTERMSIG(status_buffer[i].status));
        }
    }
    buffer_count = 0;
}

int isBuiltin(char* s)
{
    for (int i = 0; builtins_table[i].name != NULL; i++)
    {
        if (strcmp(builtins_table[i].name, s) == 0)
        {
            return i;
        }
    }
    return -1;
}

void redir_error(redirseq* redir_node)
{
   if (errno == ENOENT)
	{
    	fprintf(stderr, "%s: no such file or directory\n", redir_node->r->filename);
	}
	else if (errno == EACCES)
	{
    	fprintf(stderr, "%s: permission denied\n", redir_node->r->filename);
	}
}

bool setRedir(command *cmd)
{
  	if (cmd == NULL || cmd->redirs == NULL)
 	{
  		return true;
  	}
    redirseq *redir_node = cmd->redirs;
    int fd;

    do
    {
      	fd = 0;
        // <
        if (IS_RIN(redir_node->r->flags))
        {
            fd = open(redir_node->r->filename, O_RDONLY);
            if (fd == -1)
            {
               	redir_error(redir_node);
                return false;
            } else {
              	dup2(fd,STDIN_FILENO);
        		close(fd);
            }
        }

        // >
        if (IS_ROUT(redir_node->r->flags))
        {
            int flags = O_WRONLY | O_CREAT | O_TRUNC;

            fd = open(redir_node->r->filename, flags, S_IRUSR | S_IWUSR);
            if (fd == -1)
            {
               	redir_error(redir_node);
                return false;
            } else {
              	dup2(fd, STDOUT_FILENO);
        		close(fd);
            }
        }

        // >>
        if (IS_RAPPEND(redir_node->r->flags))
        {
            int flags = O_WRONLY | O_CREAT | O_APPEND;
            fd = open(redir_node->r->filename, flags, S_IRUSR | S_IWUSR);
            if (fd == -1)
            {
				redir_error(redir_node);
                return false;
            } else {
              	dup2(fd, STDOUT_FILENO);
        		close(fd);
            }
        }

        redir_node = redir_node->next;
    } while (redir_node != cmd->redirs);

    return true;
}


void execute_pipeline(pipeline *pipe_data, sigset_t sigset)
{
    int pipe1[2], pipe2[2];
    int *curr_pipe, *prev_pipe = NULL;
    int idx;

    commandseq *current = pipe_data->commands;
    int num_commands = countCommands(pipe_data), exi;

    // obsluga dziur w pipeline
    if(empty_commands(pipe_data))
    {
        fprintf(stderr, SYNTAX_ERROR_STR "\n");
        return;
    }

    for (int i = 0; i < num_commands; ++i)
    {
        // obsluga pustych
        if (current->com == NULL)
        {
            continue;
        }
        int c = 0;
        argseq *argseq = current->com->args;
        do {
            ++c;
            argseq = argseq->next;
        } while (argseq != current->com->args);

        char *arg[c + 1];
        argseq = current->com->args;
        for (int j = 0; j < c; ++j) {
            arg[j] = argseq->arg;
            argseq = argseq->next;
        }
        arg[c] = NULL;

        // obsluga buildin
        if(num_commands == 1)
        {
            if((idx = isBuiltin(arg[0])) >= 0)
            {
                builtins_table[idx].fun(arg);
                return;
            }
        }

        curr_pipe = (i % 2 == 0) ? pipe1 : pipe2;

        if(num_commands > 1 &&  i < num_commands - 1)
        {
        	if (pipe(curr_pipe) < 0)
        	{
            	fprintf(stderr, "Pipe error\n");
            	exit(EXIT_FAILURE);
        	}
        }

        sigprocmask(SIG_BLOCK, &sigset, NULL);

        pid_t pid = fork();

        if (pid < 0)
        {
            fprintf(stderr, "Fork error\n");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            // przywracamy dfl handler dla sigint w dziecku
            struct sigaction sa;
            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sigaction(SIGINT, &sa, NULL);

            // obsluga pipe tylko dla wielu komend
            if(num_commands > 1)
            {
                if (i > 0)
                {
                    close(0);
                    dup(prev_pipe[0]);          // przepinamy
                    close(prev_pipe[0]);        // nie potrzebujemy juz
                    close(prev_pipe[1]);         // zamykamy write poprzedniego

                }

                if (i < num_commands - 1)
                {
                    close(1);
                    dup(curr_pipe[1]);
                    close(curr_pipe[1]);
                    close(curr_pipe[0]);
                }
            }


            // odpinamy terminal dziecku, dzieki czemu nie zabija go ctrl-c
            if(is_pipeline_in_background(pipe_data))
            {
                setsid();
            }

            if(setRedir(current->com))
            {
                exi = execvp(arg[0], arg);
            }
            else
            {
                exit(EXIT_FAILURE);
            }

            if (exi == -1)
            {
                if (errno == ENOENT) {
                    fprintf(stderr, "%s: no such file or directory\n", arg[0]);
                } else if (errno == EACCES) {
                    fprintf(stderr, "%s: permission denied\n", arg[0]);
                } else {
                    fprintf(stderr, "%s: exec error\n", arg[0]);
                }
                fflush(stderr);
                exit(EXEC_FAILURE);
            }
        }
        else
        {
            if(is_pipeline_in_background(pipe_data))
            {
                add_background_process(pid);
            }
            else
            {
                ++fg_count;
            }

            if(num_commands > 1)
            {
            	if (i > 0)
            	{
               		close(prev_pipe[0]);
                	close(prev_pipe[1]);
            	}

            	if(i < num_commands - 1)
            	{
                	prev_pipe = curr_pipe;
            	}

            }
        }

        sigprocmask(SIG_UNBLOCK, &sigset, NULL);

        current = current->next;
    }

    // wiemy, ze nie moze to byc chld, wiec najsensownej blokowac sigkill, zeby shell mogl pracowac
    sigset_t non_chld;
    sigemptyset(&non_chld);
    sigaddset(&non_chld, SIGKILL);

    sigprocmask(SIG_BLOCK,&sigset,NULL);

    while (fg_count > 0)
    {
        sigsuspend(&non_chld);
    }
    sigprocmask(SIG_UNBLOCK,&sigset,NULL);
}


int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sigset_t set = sa.sa_mask;
    //sa.sa_flags = SA_RESTART;  // reset po przerwanych syscol

    sigaction(SIGCHLD, &sa, NULL);

    struct sigaction sa_sigint;
    sa_sigint.sa_handler = SIG_IGN;
    sigemptyset(&sa_sigint.sa_mask);
    sigaction(SIGINT, &sa_sigint, NULL);

	pipelineseq * ln;
	command *com;

    char buffer[MAX_LINE_LENGTH + 1];
    char lineToParse[MAX_LINE_LENGTH + 1];
    int r = 1, lineToParseCurr = 0;
    bool overflow = false;
    int countPipe;


	// uzupelniamy fstat
	struct stat statbuf;
	fstat(STDIN_FILENO, &statbuf);

    while (1)
	{
		// sprawdza czy to urzadzenie znakowe
		if (S_ISCHR(statbuf.st_mode))
		{
            if(buffer_count > 0)
            {
                print_status_buffer();
            }
            fprintf(stdout, PROMPT_STR);
            fflush(stdout);
        }


		// sygnal czesto bedzie zaburzal prace blokujacych
        do {
            r = read(STDIN_FILENO, buffer, MAX_LINE_LENGTH);
        } while (r < 0 && errno == EINTR);

        if (r == 0) break;


        char* current = buffer;
        char* end = buffer + r;

        while (current < end)
        {
            char* newline = memchr(current, '\n', end - current);

            if (newline == NULL)
            {
                int remaining = end - current;
                if (lineToParseCurr + remaining < MAX_LINE_LENGTH)
                {
                    memcpy(lineToParse + lineToParseCurr, current, remaining);
                    lineToParseCurr += remaining;
                }
                else
                {
                    overflow = true;
                }
                break;
            }

            int lineLength = newline - current;

            if (!overflow)
            {
                if (lineToParseCurr + lineLength < MAX_LINE_LENGTH)
              {
                    memcpy(lineToParse + lineToParseCurr, current, lineLength);
                    lineToParseCurr += lineLength;
                    lineToParse[lineToParseCurr] = '\0';
                }
                else
                {
                    overflow = true;
                }
            }

            if (!overflow && lineToParseCurr > 0)
            {
                if (lineToParse[0] == '#')
                {
                    lineToParseCurr = 0;
                }
                else
                {
                    pipelineseq* ln = parseline(lineToParse);
                    countPipe = countPipelines(ln);

                    for (int j = 0; j < countPipe; ++j)
                    {
                        execute_pipeline(ln->pipeline, set);
                        ln = ln->next;
                    }
                }
            }

            if (overflow)
            {
                fprintf(stderr, SYNTAX_ERROR_STR "\n");
                overflow = false;
            }

            lineToParseCurr = 0;
            current = newline + 1;
        }


    }

    return 0;
}



