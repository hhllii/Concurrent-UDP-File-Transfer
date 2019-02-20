#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<ctype.h>
#include <pthread.h>

#include "simpleSocket.h" 

#define FILE_PATH "./files/"

struct ThreadAttri{
    int sockfd;
    struct sockaddr_in client;
    struct SimpleUDPmsg recvbuf;
};

int sendFileSize(int sockfd, struct sockaddr *client, const char* filename);

int recvACK(int sockfd);

int sendFile(int sockfd, struct sockaddr *client, struct SimpleUDPmsg *recvbuf);

int handleMsg(int sockfd);