#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <string> 
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <pthread.h>

#include "simpleSocket.h"

#define DEST_PATH "./dest/"

struct ThreadAttri{
    int t_idx, connectNum, filesize;
    char filename[MAX_FILENAME_LEN]; //use one buffer 
};

void getFilepath(int idx, const char* filename, char* filepath);

int getFilesize(int sockfd, const char* filename);

int getActiveSockList(vector<SimpleAddress> &list, vector<int> &socklist, const char* filename);

int sendReadRequest(int sockfd, const char* filename, int offset);

int downloadFile(int sockfd, const char* filename, int offset);
