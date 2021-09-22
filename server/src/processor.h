#ifndef SERVER_PROCESSOR_H
#define SERVER_PROCESSOR_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <getopt.h>
#include <fcntl.h>
#include <pthread.h>

#define  SIZE  1024
#define  ROOT_PATH_SIZE 200
#define  ARGUMENT_SIZE 200

enum UserStatus {no_log, has_name, logged};

struct ClientPthread
{
    int connfd;                         // 用于命令连接的连接socket
    int data_connfd[2];                 // 用于数据连接的监听socket和连接socket
    struct sockaddr_in *data_addr;
    enum UserStatus userStatus;         // 该客户端的登录状态
    int is_passive;                     // 数据传输模式
    char client_pwd[ARGUMENT_SIZE];     // 客户端所在工作目录
    char filepath[ARGUMENT_SIZE];       // 希望重命名的文件路径
    int start_byte;                     // 用于断点续传的起始字节
};

void parseArguments(int argc, char **argv);
int parseInstruction(struct ClientPthread* pthread_data, char sentence[]);
void* pthreadFunc(void *arg);

void writeBuffer(int connfd, char* sentence);
void readBuffer(int connfd, char* sentence);
void prepareDataConnection(struct ClientPthread* pthread_data);
char* numberToString(int num, char* str);
char* getHostIp();

int processor(struct ClientPthread* pthread_data, char* order, char* argument);


// 命令的处理函数
void userHandler(struct ClientPthread* pthread_data, char* sentence);
void passHandler(struct ClientPthread* pthread_data, char* sentence);
void pasvHandler(struct ClientPthread* pthread_data);
void portHandler(struct ClientPthread* pthread_data, char* sentence);
void retrHandler(struct ClientPthread* pthread_data, char* sentence);
void storHandler(struct ClientPthread* pthread_data, char* sentence);
void appeHandler(struct ClientPthread* pthread_data, char* sentence);
void restHandler(struct ClientPthread* pthread_data, char* sentence);
void listHandler(struct ClientPthread* pthread_data);
void rnfrHandler(struct ClientPthread* pthread_data, char* sentence);
void rntoHandler(struct ClientPthread* pthread_data, char* sentence);
void mkdHandler(struct ClientPthread* pthread_data, char* sentence);
void rmdHandler(struct ClientPthread* pthread_data, char* sentence);
void pwdHandler(struct ClientPthread* pthread_data);
void cwdHandler(struct ClientPthread* pthread_data, char* sentence);
void systHandler(int connfd);
void typeHandler(int connfd, char* sentence);
void quitHandler(int connfd);
void aborHandler(int connfd);
#endif //SERVER_PROCESSOR_H
