#include "kernel/types.h"
#include "user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

void findDir(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) // examine open state
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    if (*(p - 1) != '/') // ensure path ends with '/'
    {
        *p++ = '/';
    }
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        if (de.inum == 0 || de.name[0] == '.') // omit "." & ".."
            continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        // now buf indicates every file path under current directory
        if (stat(buf, &st) < 0)
        {
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        if (st.type == T_FILE && strcmp(p, filename) == 0) // find target file
        {
            printf("%s\n", buf);
        }
        if (st.type == T_DIR) // find into directory recursively
        {
            findDir(buf, filename);
        }
    }
    close(fd);
}

int main(int agrc, char *argv[])
{
    if (agrc != 3) // examine agrument num
    {
        printf("usage: find [path] [file_name]\n");
        exit(-1);
    }
    findDir(argv[1], argv[2]);
    exit(0);
}