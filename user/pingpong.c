#include "kernel/types.h"
#include "user.h"

int main(int agrc, char *argv[])
{
    if (agrc != 1) // examine agrument num
    {
        printf("pingpong needs no argument!\n");
        exit(-1);
    }
    int p[2];
    pipe(p);
    int rc = fork();
    if (rc < 0) // fork() fail
    {
        printf("fork() fail!");
        exit(-1);
    }
    else if (rc == 0) // child process
    {
        char childReadBuf[5];
        char childWriteBuf[5] = "pong";
        read(p[0], childReadBuf, 5);
        close(p[0]);
        printf("%d: received %s\n", getpid(), childReadBuf);
        write(p[1], childWriteBuf, 5);
        close(p[1]);
    }
    else // parent process
    {
        char parentWriteBuf[5] = "ping";
        char parentReadBuf[5];
        write(p[1], parentWriteBuf, 5);
        close(p[1]);
        wait(0); // ensure parent read after child write
        read(p[0], parentReadBuf, 5);
        close(p[0]);
        printf("%d: received %s\n", getpid(), parentReadBuf);
    }
    exit(0);
}