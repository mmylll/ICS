
#include <io.h>
#include <fcntl.h>
#include <stdio.h>


#define BUF_SIZE 1024
int main() {
    char buf[BUF_SIZE];
    /* read some characters into buf */
    int fd1 = open("input.txt", O_RDONLY);
    int fd2 = open("input.txt", O_RDONLY);
//    dup2(fd2, fd1); /* ! mark ! */
    read(fd1, buf, 3);
    close(fd1);
    read(fd2, &buf[3], 4);
    close(fd2); /* send buf to server*/
    write(STDOUT_FILENO,buf,strlen(buf));
    return 0;
}
