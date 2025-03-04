#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>



#include "../include/builtins.h"

int count_arguments(char *argv[]) {
    int count = 0;
    while (argv[count] != NULL) {
        count++;
    }
    return count;
}

int lexit(char *argv[]);
int lkill(char *[]);
int echo(char *[]);
int undefined(char *[]);
int lcd(char *[]);
int lls(char *[]);

builtin_pair builtins_table[] = {
        {"exit",    &lexit},
        {"lecho",   &echo},
        {"lcd",     &lcd},
        {"lkill",   &lkill},
        {"lls",     &lls},
        {NULL, NULL}
};



int echo(char *argv[]) {
    int i = 1;
    if (argv[i]) printf("%s", argv[i++]);
    while (argv[i])
        printf(" %s", argv[i++]);

    printf("\n");
    fflush(stdout);
    return 0;
}

int undefined(char *argv[]) {
    fprintf(stderr, "Command %s undefined.\n", argv[0]);
    return BUILTIN_ERROR;
}

int lcd(char *argv[])
{
  if(count_arguments(argv) > 2)
  {
      fprintf(stderr, "Builtin lcd error.\n");
      return BUILTIN_ERROR;
  }
    char *target_dir = (argv[1]) ? argv[1] : getenv("HOME");
    if (!target_dir || chdir(target_dir) == -1)
    {
        fprintf(stderr, "Builtin lcd error.\n");
        return BUILTIN_ERROR;
    }
    return 0;
}

int lls(char *argv[]) {
    if(count_arguments(argv) > 1)
    {
        perror("Builtin lls error");
        return BUILTIN_ERROR;
    }

    DIR *dirp = opendir(".");
    if (!dirp) {
        perror("Builtin lls error");
        return BUILTIN_ERROR;
    }

    struct dirent *drip;
    while ((drip = readdir(dirp)) != NULL) {
        if (drip->d_name[0] != '.') {
            printf("%s\n", drip->d_name);
        }
    }

    fflush(stdout);
    if (closedir(dirp) == -1) {
        perror("Builtin lls error");
        return BUILTIN_ERROR;
    }
    return 0;
}

int lexit(char *argv[]) {
    exit(EXIT_SUCCESS);
    return 0;
}

int conversion_checker(long long s)
{
    if((s == LONG_MAX || s == LONG_MIN) && errno == ERANGE)
    {
        return -1;
    }
    return 0;
}

int lkill(char *argv[]) {
    if(count_arguments(argv) > 3)
    {
        fprintf(stderr, "Builtin lcd error.\n");
        return BUILTIN_ERROR;
    }

    long long s = SIGTERM;
    pid_t pid;

    if (argv[1] != NULL && argv[1][0] == '-')
    {
        if (isdigit(argv[1][1]))
        {
            s = strtol(argv[1] + 1, NULL, 10);
            if (conversion_checker(s) == -1)
            {
                fprintf(stderr, "Builtin lls error");
                return BUILTIN_ERROR;
            }
        }
        else
        {
            fprintf(stderr, "Builtin lkill error.\n");
            return BUILTIN_ERROR;
        }
        if (argv[2] != NULL)
        {
            pid = strtol(argv[2], NULL, 10);
            if (conversion_checker(s) == -1)
            {
                fprintf(stderr, "Builtin lls error");
                return BUILTIN_ERROR;
            }
        }
        else
        {
            fprintf(stderr, "Builtin lkill error.\n");
            return BUILTIN_ERROR;
        }
    }
    else if (argv[1] != NULL)
    {
        pid = strtol(argv[1], NULL, 10);
        if (conversion_checker(s) == -1)
        {
            fprintf(stderr, "Builtin lls error");
            return BUILTIN_ERROR;
        }
    }
    else
    {
        fprintf(stderr, "Builtin lkill error.\n");
        return BUILTIN_ERROR;
    }

    if (kill(pid, s) == -1)
    {
        fprintf(stderr, "Builtin lkill error");
        return BUILTIN_ERROR;
    }
    return 0;
}
