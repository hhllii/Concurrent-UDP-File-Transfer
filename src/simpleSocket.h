#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <arpa/inet.h>
#include<unistd.h>
#include<ctype.h>
#include<vector>
#include <algorithm>
#include<iostream>
#include<fstream>
#include<cstdlib>

#define BUFFER_SIZE 1024 
#define MAX_SERVER 1024 
#define DEFAULT_PORT 80
#define MAX_FILENAME_LEN 255 
#define MAX_PATH_LEN 4096

using namespace std;

//strcpy connot excess the BUFFER_SIZE 

struct SimpleAddress{
	int port;
	char *address;
};

struct SimpleChunk{
	int size, offset; //size number of connection
    //code 0 for query, 1 for download
	char buffer[BUFFER_SIZE];
	bool endflag = false;
};

struct SimpleUDPmsg{
	int code; //code 1 for query, 2 for read request, 3 for ack
	int filesize;
	int offset;
	int chunksize;
	char filename[MAX_FILENAME_LEN];
	char buffer[BUFFER_SIZE];
};



//check the char* are digits
bool checkdigit(const char* line);

bool portVarify(const char* port); //with digit and int range

int getFileSize(FILE* fp); //largest 2G file for int

void filecat(FILE* fp1, FILE* fp2); // 1 byte copy

int createSocketAddr(struct sockaddr_in* serv_addr, const char *address, int port);

struct SimpleAddress getAddressbyLine(char* line);

//vector<int> getActiveSockList(vector<SimpleAddress> list); // return the socket created with active server

void setTimeout(int sockfd, int send_time, int recv_time);

void simpleSocketSend(int sockfd, SimpleChunk* chunk, int chunk_size);

int simpleSocketRecv(int sockfd, SimpleChunk* chunk, int chunk_size);

void simpleUDPSend(int sockfd, SimpleChunk* chunk, int chunk_size);

int simpleUDPRecv(int sockfd, SimpleChunk* chunk, int chunk_size);