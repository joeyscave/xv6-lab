#include "kernel/types.h"
#include "kernel/param.h"
#include "user.h"

#define RANGEBUF 512

/**
 * @brief parse args from buf into argv
 * @return total number of args in argv list + 1
 **/
int parse(char *buf, char *argv[], int i)
{
    char *rpix, *lpix = buf;
    int num = i;
    while (*lpix)
    {
        while (*lpix == ' ') // skip blank
        {
            lpix++;
        }
        rpix = lpix;
        while (*rpix != ' ' && *rpix != '\n') // traverse to the end of current argument
        {
            rpix++;
        }
        *rpix = 0;
        argv[num++] = lpix;
        lpix = rpix + 1;
    }
    if (num > MAXARG) // examine argumnet number
    {
        printf("xargs: too many arguments!\n");
        exit(-1);
    }
    return num;
}

int main(int argc, char *largv[])
{
    if (argc == 1) // examine agrument num
    {
        printf("usage: xargs [command] [arg...]\n");
        exit(-1);
    }

    char *argv[MAXARG];
    char buf[RANGEBUF];
    int i;
    for (i = 0; i < argc - 1; i++)
    {
        argv[i] = largv[i + 1];
    }
    while (1)
    {
        memset(buf, 0, RANGEBUF); // init buf
        gets(buf, RANGEBUF);      // read rest argument
        if (buf[0] == 0)          // EOF
        {
            exit(0);
        }
        argv[parse(buf, argv, i)] = 0; // supplement end sign to argv list
        if (fork() == 0)               // exec in child process
        {
            if (exec(argv[0], argv) == 0) // exec fail
            {
                printf("xargs: exec() fail!\n");
                exit(-1);
            }
        }
        wait(0);
    }
    exit(0);
}