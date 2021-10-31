#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>

#define MAX_LINE 1024

//void output_file(char* path) {
//    FILE *fp = fopen(path, "r");
//    char buf[MAX_LINE];
//    while (!feof(fp)) {
//        memset(buf, 0 , MAX_LINE);
//        fgets(buf, MAX_LINE, fp);
//        if (buf[0] != 0) printf("line:%s", buf);
//    }
//    fclose(fp);
//}

int my_getline(int fd, void *buf, size_t maxlen){
    int n, rc;
    char c, *bufp = buf;
    for (n = 1; n < maxlen ; n++) {
        if ( (rc = read(fd, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n') {
                n++;
                break;
            }
        }
        else if (rc == 0) {
            if (n == 1) return 0; // EOF, no data read;
            else break;// EOF, some data was read.

        }
        else return -1; //Error


    }
    *bufp = 0;
    return n-1;

}

void output_file(int fd) {
    char buf[MAX_LINE];
    while(my_getline(fd, buf, MAX_LINE) != 0) {
        printf("line: %s", buf);
        fflush(stdout);
    }
    close(fd);
}

int main() {
    char paths[3][20] = {
            "file1.txt" , "file2.txt" , "file3.txt"
    };
    // 可以使用
//    pid_t pids[3];
//    if((pids[0] = fork()) == 0 ){
//        output_file(paths[0]);
//    }else if((pids[1] = fork()) == 0){
//        output_file(paths[1]);
//    }else{
//        output_file(paths[2]);
//    }

//    可以使用
//    pthread_t tids[3];
//    int ret_thrd1 = pthread_create(&tids[0], NULL, (void *(*)(void *)) &output_file, paths[0]);
//    int ret_thrd2 = pthread_create(&tids[1], NULL, (void *(*)(void *)) &output_file, paths[1]);
//    int ret_thrd3 = pthread_create(&tids[2], NULL, (void *(*)(void *)) &output_file, paths[2]);
//
//    if (ret_thrd1 != 0) {
//        printf("线程1创建失败\n");
//    } else {
//        printf("线程1创建成功\n");
//        pthread_exit(NULL);
//    }
//    if (ret_thrd2 != 0) {
//        printf("线程2创建失败\n");
//    } else {
//        printf("线程2创建成功\n");
//    }
//    if (ret_thrd3 != 0) {
//        printf("线程3创建失败\n");
//    } else {
//        printf("线程3创建成功\n");
//    }

    int fds[3];
    int finished_statuses[3] = {0, 0 , 0};
    fd_set ready_set;
    FD_ZERO(&ready_set);
    for (int i = 0 ; i < 3; i++) {
        fds[i] = open(paths[i], O_RDONLY);
        FD_SET(fds[i], &ready_set);
        if(FD_ISSET(fds[i], &ready_set)){
            output_file(fds[i]);
        }
    }
    return 0;
}