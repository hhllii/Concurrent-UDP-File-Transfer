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

int sendFile(int sockfd, struct sockaddr *client, struct SimpleUDPmsg *recvbuf){
    int sendsock;
    const char* localaddr = "127.0.0.1";
    struct sockaddr_in servaddr;
    struct sockaddr_in *pservaddr = &servaddr;

    printf("Request file %s \n", recvbuf->filename);

    // Create new port
    if(createSocketAddr(pservaddr, localaddr, new_port) < 0){
        printf("\n Invalid address: %s\n", localaddr);
    }
    new_port += 1; //*bug might got port error already in use
    // New socket create
    if( (sendsock = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ){
        printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }
    if( bind(sendsock, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("Bind new socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    // Set timeout
    setTimeout(sendsock, 1, 1);

    if (connect(sendsock, client, sizeof(sockaddr_in)) < 0) 
    { 
        printf("\n Connection Failed %s(errno: %d)\n",strerror(errno),errno);
    } 

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
        // if (send(sendsock, psendbuf, sizeof(sendbuf), 0) < 0){
        //     printf("Send data error: %s(errno: %d)\n", strerror(errno), errno);
        // }
        int send_size = sendto(sockfd,&sendbuf,sizeof(sendbuf),0,client,sizeof(struct sockaddr_in));
        if(send_size < 0){
            printf("sendto error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        memset(fileBuffer,0,strlen(fileBuffer));
        //sleep(2); //test for sending block
        sendCount += block_len;
        //wait for ack
        memset(&ACKbuf,0,sizeof(ACKbuf));
        // if( recv(sockfd, recvbuf, sizeof(recvbuf), 0) < 0 ){
        //     printf("ACK error %s(errno: %d)\n", strerror(errno),errno);
        //     return -1;
        // }
        
        socklen_t addrlen = sizeof(struct sockaddr_in);
        int recv_size =recvfrom(sockfd,&ACKbuf,sizeof(ACKbuf),0,client, &addrlen);
        if(recv_size < 0){
            printf("recvfrom error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        if(ACKbuf.code != 3){
            printf("ACK receive error. \n");
            return -1;
        }
    }
    //send last block
    int filesize = recvbuf->filesize;
    int chunksize = recvbuf->chunksize;
    int numchunk = filesize / chunksize; 
    if(recvbuf->offset == numchunk - 1){
        //last part of fiile
        memset(&sendbuf,0,sizeof(sendbuf));
        int lastLen = filesize - recvbuf->offset * chunksize;
        block_len = fread(fileBuffer, sizeof(char), lastLen, fp); 
        strncpy(sendbuf.buffer, fileBuffer, block_len);
        int send_size = sendto(sockfd,&sendbuf,sizeof(sendbuf),0,client,sizeof(struct sockaddr_in));
        if(send_size < 0){
            printf("sendto error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        memset(fileBuffer,0,strlen(fileBuffer));
    }else{//send remaining byte
        memset(&sendbuf,0,sizeof(sendbuf));
        block_len = fread(fileBuffer, sizeof(char), chunksize - sendCount, fp); 
        strcpy(sendbuf.buffer, fileBuffer);
        int send_size = sendto(sockfd,&sendbuf,sizeof(sendbuf),0,client,sizeof(struct sockaddr_in));
        if(send_size < 0){
            printf("sendto error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        memset(fileBuffer,0,strlen(fileBuffer));
    }

    printf("*End of data send\n");
    //free(sendchunkPtr);
    // Close file 
    if(fclose(fp) == -1){
        printf("Close file error!\n");
        exit(1);
    }

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
            sendFile(sockfd , (struct sockaddr *)&client, precvbuf);
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
    new_port += atoi(argv[1]) + 10000; //*bug might got port error already in use

    if( bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("Bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }


    printf("======Waiting for client's request======\n");
    while(1){
        handleMsg(sockfd);
        printf("*End of one client msg\n");
        // if( (connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
        //     printf("Accept socket error: %s(errno: %d)",strerror(errno),errno);
        //     continue;
        // }else{
        //     // Set timeout
        //     setTimeout(connfd, 3, 0);
        //     //connection success receiving file name
        //     memset(&chunk,0,sizeof(struct SimpleChunk));
        //     if(simpleSocketRecv(connfd, chunkPtr, sizeof(struct SimpleChunk)) < 0){
        //         //error with socket
        //         printf("*End of client \n");
        //         continue;
        //     }else{
        //         printf("Received filename from client: %s\n", chunk.buffer);
        //         printf("Num-connection: %d\nOffset: %d\n", chunk.size, chunk.offset);
        //         filename = chunk.buffer;
        //     }


        //     char filePath[MAX_PATH_LEN];
        //     strcpy(filePath,FILE_PATH);
        //     strcat(filePath,filename);
        //     // Handle the command
        //     FILE *fp;
        //     //const char* filePath = "set"; // setfile
        //     fp = fopen(filePath, "r");
        //     if(fp == NULL){
        //         printf("open file error!\n");
        //         // fclose(fp);
        //         continue;
        //     }

        //     // Get file size
        //     int fileSize = getFileSize(fp);
        //     printf("*File size: %i\n", fileSize);

        //     // Get the file part size for each thread
        //     int partSize = fileSize/chunk.size; //*bug might have some precise lose with the last part of file
        //     printf("*File part size: %i\n", partSize);

        //     // Send file
        //     char fileBuffer[1000];
        //     int block_len = 0;
        //     struct SimpleChunk sendchunk;
        //     struct SimpleChunk *sendchunkPtr = &sendchunk;
        //     int sendCount = 0;
        //     // Fp to send start pos
        //     fseek(fp, chunk.offset * partSize, SEEK_SET );

        //     while ((sendCount + 1000) < partSize && (block_len = fread(fileBuffer, sizeof(char), 1000, fp)) > 0 )
        //     {
        //         // Send data
        //         //clean struct
        //         memset(&sendchunk,0,sizeof(sendchunk));
        //         sendchunk.size = block_len;
        //         strcpy(sendchunk.buffer, fileBuffer);
        //         simpleSocketSend(connfd, sendchunkPtr, sizeof(struct SimpleChunk));
        //         memset(fileBuffer,0,strlen(fileBuffer));
        //         //sleep(2); //test for sending block
        //         sendCount += block_len;
        //     }
        //     //send last block
        //     if(chunk.offset == chunk.size - 1){
        //         //last part of fiile
        //         memset(&sendchunk,0,sizeof(sendchunk));
        //         int lastLen = fileSize - chunk.offset * partSize;
        //         block_len = fread(fileBuffer, sizeof(char), lastLen, fp); 
        //         sendchunk.size = block_len;
        //         strncpy(sendchunk.buffer, fileBuffer, block_len);
        //         //strcpy(sendchunk.buffer, fileBuffer);
        //         simpleSocketSend(connfd, sendchunkPtr, sizeof(struct SimpleChunk));
        //         memset(fileBuffer,0,strlen(fileBuffer));
        //     }else{//send remaining byte
        //         memset(&sendchunk,0,sizeof(sendchunk));
        //         block_len = fread(fileBuffer, sizeof(char), partSize - sendCount, fp); 
        //         sendchunk.size = block_len;
        //         strcpy(sendchunk.buffer, fileBuffer);
        //         simpleSocketSend(connfd, sendchunkPtr, sizeof(struct SimpleChunk));
        //         memset(fileBuffer,0,strlen(fileBuffer));
        //     }

        //     printf("*End of data send\n");
        //     //free(sendchunkPtr);
        //     // Close file 
        //     if(fclose(fp) == -1){
        //         printf("Close file error!\n");
        //         exit(1);
        //     }
        //} 
        // End of this connection
        //close(connfd);
    }
    return 0;
}