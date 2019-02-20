#include "TFTP_Server.h" 

int new_port;

int sendFileSize(int sockfd, struct sockaddr *client, const char* filename){
    struct SimpleUDPmsg sentbuf;
    struct SimpleUDPmsg *psentbuf = &sentbuf;
    char filePath[MAX_PATH_LEN];
    strcpy(filePath,FILE_PATH);
    strcat(filePath,filename);
    // Handle the command
    FILE *fp;
    int fileSize;
    //const char* filePath = "set"; // setfile
    fp = fopen(filePath, "r");
    if(fp == NULL){
        printf("Open file error! No such file\n");
        fileSize = -1;
    }else{
        // Get file size
        fileSize = getFileSize(fp);
    }

    // Build msg packet
    sentbuf.code = 1;
    strcpy(sentbuf.filename,filename);
    sentbuf.filesize = fileSize;
    //*issue waste the buffer just used for sending file size

    int send_size = sendto(sockfd,&sentbuf,sizeof(sentbuf),0,client,sizeof(sockaddr_in));
    if(send_size < 0){
        printf("sendto error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    printf("*Sent file size: %i\n", fileSize);
    return 0;
}

int recvACK(int sockfd, struct sockaddr *client){
    struct SimpleUDPmsg ACKbuf;
    struct SimpleUDPmsg *pACKbuf = &ACKbuf;
    memset(&ACKbuf,0,sizeof(ACKbuf));
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int recv_size =recvfrom(sockfd,&ACKbuf,sizeof(ACKbuf),0, client, &addrlen);
    if(recv_size < 0){
        printf("ACK recvfrom error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    if(ACKbuf.code != 3){
        printf("ACK receive error. \n");
        return -2;
    }
    return 0;
}

int sendFile(int sockfd, struct sockaddr *client, struct SimpleUDPmsg *recvbuf){
    int sendsock;
    const char* localaddr = "127.0.0.1";
    struct sockaddr_in servaddr;
    struct sockaddr_in *pservaddr = &servaddr;
    int numChunk = recvbuf->numchunk; 
    printf("Request file %s \n", recvbuf->filename);

    // Create new port
    int sendPort;
    if(createSocketAddr(pservaddr, localaddr, 0) < 0){
        printf("\n Invalid address: %s\n", localaddr);
    }
    // pservaddr->sin_port = 0;
    // sendPort = ntohs(pservaddr->sin_port);
    
    //new_port += 1; //*bug might got port error already in use
    // New socket create
    if( (sendsock = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ){
        printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    if( bind(sendsock, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("Bind new socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    socklen_t addrlen = sizeof(struct sockaddr_in);
    if( getsockname(sendsock, (struct sockaddr*)&servaddr, &addrlen) == -1){
        printf("Get new socket name error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    sendPort = ntohs(pservaddr->sin_port);
    printf("New port: %d \n", sendPort);

    // Set timeout
    setTimeout(sendsock, 2, 2);


    char filePath[MAX_PATH_LEN];
    strcpy(filePath,FILE_PATH);
    strcat(filePath,recvbuf->filename);
    // Handle the command
    FILE *fp;
    //const char* filePath = "set"; // setfile
    fp = fopen(filePath, "r");
    if(fp == NULL){
        printf("open file error!\n");
        return -1;
    }
    char fileBuffer[1000];
    int block_len = 0;
    struct SimpleUDPmsg sendbuf;
    struct SimpleUDPmsg *psendbuf = &sendbuf;
    struct SimpleUDPmsg ACKbuf;
    struct SimpleUDPmsg *pACKbuf = &ACKbuf;
    int sendCount = 0;

    //First send ACK to acknowledge new port
    sendbuf.code = 3;
    sendbuf.serverPort = sendPort;
    int send_size = sendto(sockfd,&sendbuf,sizeof(sendbuf),0,client,sizeof(struct sockaddr_in));
    if(send_size < 0){
        printf("sendto error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    // ACK new port
    if(recvACK(sendsock, client ) < 0){
        printf("No ACK new port\n");
        return -1;
    }else{
        printf("New port ACK\n");
    }
    int filesize = getFileSize(fp);

    // Fp to send start pos
    fseek(fp, recvbuf->offset * recvbuf->chunksize, SEEK_SET );
    printf("*Start sending file!\n");
    while ((sendCount + 1000) < recvbuf->chunksize && (block_len = fread(fileBuffer, sizeof(char), 1000, fp)) > 0 )
    {
        // Send data
        //clean struct
        memset(&sendbuf,0,sizeof(sendbuf));
        sendbuf.code = 2;
        strcpy(sendbuf.buffer, fileBuffer);

        int send_size = sendto(sendsock,&sendbuf,sizeof(sendbuf),0,client,sizeof(struct sockaddr_in));
        if(send_size < 0){
            printf("sendto error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        memset(fileBuffer,0,strlen(fileBuffer));
        //sleep(2); //test for sending block
        sendCount += block_len;
        //wait for ack

        if(recvACK(sendsock, client) < 0){
            printf("No ACK\n");
            return -1;
        }
    }
    //send last block
    int chunksize = recvbuf->chunksize;
    int numchunk = filesize / chunksize; 
    if(recvbuf->offset == (numchunk - 1)){
        //last part of fiile
        printf("Send last part filesize:%d, chunksize:%d\n", filesize, chunksize);
        memset(&sendbuf,0,sizeof(sendbuf));
        int lastLen = filesize - recvbuf->offset * chunksize;
        block_len = fread(fileBuffer, sizeof(char), lastLen, fp); 
        sendbuf.code = 3;
        strncpy(sendbuf.buffer, fileBuffer, block_len);
        int send_size = sendto(sendsock,&sendbuf,sizeof(sendbuf),0,client,sizeof(struct sockaddr_in));
        if(send_size < 0){
            printf("sendto error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        memset(fileBuffer,0,strlen(fileBuffer));
        if(recvACK(sendsock, client) < 0){
            return -1;
        }
    }else{//send remaining byte
        memset(&sendbuf,0,sizeof(sendbuf));
        block_len = fread(fileBuffer, sizeof(char), chunksize - sendCount, fp); 
        sendbuf.code = 3;
        strcpy(sendbuf.buffer, fileBuffer);
        int send_size = sendto(sendsock,&sendbuf,sizeof(sendbuf),0,client,sizeof(struct sockaddr_in));
        if(send_size < 0){
            printf("sendto error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        memset(fileBuffer,0,strlen(fileBuffer));
        if(recvACK(sendsock, client) < 0){
            return -1;
        }
    }

    printf("*End of data send\n");
    //free(sendchunkPtr);
    // Close file 
    if(fclose(fp) == -1){
        printf("Close file error!\n");
        exit(1);
    }

}

void *sendThread(void *arg){
    struct ThreadAttri *temp;
    temp = (struct ThreadAttri *)arg;

    if(sendFile(temp->sockfd , (struct sockaddr *)&(temp->client), &(temp->recvbuf)) < 0){
        printf("File send failed!\n");
        free(arg);
        pthread_exit((void*)-1);
    }
    free(arg);
    pthread_exit((void*)1);
}

int handleMsg(int sockfd){ //error return -1
    struct SimpleUDPmsg recvbuf;
    struct SimpleUDPmsg *precvbuf = &recvbuf;
    struct sockaddr_in client;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int recv_size =recvfrom(sockfd,&recvbuf,sizeof(recvbuf),0,(struct sockaddr *)&client, &addrlen);
    if(recv_size < 0){
        printf("recvfrom error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    int code = recvbuf.code; 
    switch(code){
        case 1: //query file
            sendFileSize(sockfd, (struct sockaddr *)&client, recvbuf.filename);
            break;
        case 2: //read request
            //create new thread to handle send file
            pthread_t thid;
            struct ThreadAttri *ptAttri;
            ptAttri = (struct ThreadAttri*)malloc(sizeof(struct ThreadAttri));
            ptAttri->client = client;
            ptAttri->recvbuf = recvbuf;
            ptAttri->sockfd = sockfd;
            if (pthread_create(&thid,NULL,sendThread,(void*)ptAttri) == -1){
             printf("Thread create error!\n");
             return -1;
            }
            //sendFile(sockfd , (struct sockaddr *)&client, precvbuf);
            break;
        default:
            printf("No such message code: %d\n", code);
            return -1;
    }
    return 0;
}



int main(int argc, char** argv){
    int  sockfd, connfd;
    struct sockaddr_in servaddr;
    struct sockaddr_in *pservaddr = &servaddr;
    //char  buffer[1024];
    int  n;
    SimpleChunk chunk;
    SimpleChunk *chunkPtr = &chunk;
    char* filename;

    if(argc != 2){
        printf("Usage: ./myserver <port>\n");
        return 0;
    }

    // Varify port
    if(!portVarify(argv[1])){
        printf("\n Port invalid\n"); 
        return 0;
    }
    // Listen socket create
    if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ){
        printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }
    const char* localaddr = "127.0.0.1";
    if(createSocketAddr(pservaddr, localaddr, atoi(argv[1])) < 0){
        printf("\n Invalid address: %s\n", localaddr);
    }
    //new_port += atoi(argv[1]) + 10000; //*bug might got port error already in use

    if( bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("Bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }


    printf("======Waiting for client's request======\n");
    while(1){
        handleMsg(sockfd);
        printf("*End of one client msg\n");
    }
    return 0;
}