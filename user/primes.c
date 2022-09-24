#include "kernel/types.h"
#include "user/user.h"

#define RANGE 35

void next(int leftPipe[2])
{
    close(leftPipe[1]);
    int prime;      // first number read from left pipe
    int forked = 0; // indicates wheather had forked

    // read first number and print
    if (read(leftPipe[0], &prime, sizeof(int)) == sizeof(int))
    {
        printf("prime %d\n", prime);
        int rightPipe[2];
        pipe(rightPipe);
        int data;

        // continue reading
        while (read(leftPipe[0], &data, sizeof(int)) == sizeof(int))
        {
            // pass to right pipe
            if (data % prime)
            {
                // fork at the first pass(ensure multi process runs concurrently indeed)
                if (forked == 0)
                {
                    forked = 1;
                    if (fork() == 0)
                    {
                        close(leftPipe[0]);
                        next(rightPipe);
                    }
                }
                write(rightPipe[1], &data, sizeof(int));
            }
        }
        close(leftPipe[0]);
        close(rightPipe[0]);
        close(rightPipe[1]);
        wait(0);
    }

    exit(0);
}

int main(int argc, char *argv[])
{
    int rightPipe[2];
    pipe(rightPipe);

    for (int i = 2; i <= RANGE; ++i)
        write(rightPipe[1], &i, sizeof(int));

    if (fork() == 0)
    {
        next(rightPipe);
    }
    else
    {
        close(rightPipe[1]);
        close(rightPipe[0]);
        wait(0);
    }

    exit(0);
}