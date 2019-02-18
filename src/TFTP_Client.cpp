#include "TFTP_Client.h"

using namespace std;

int fileSize;
int chunkSize;

void getFilepath(int idx, const char* filename, char* filepath){
    // Construct file path
    char filepre[5];
    sprintf(filepre,"%d", idx); 
    strcpy(filepath,DEST_PATH);
    strcat(filepath,filepre);
    strcat(filepath,"_");
    strcat(filepath,filename);
}

//>=0 the size  =-1 connection error =-2 no file
int getFilesize(int sockfd, const char* filename){
    struct SimpleUDPmsg sentbuf;
    struct SimpleUDPmsg *psentbuf = &sentbuf;

    // Build msg packet
    sentbuf.code = 1;
    strcpy(sentbuf.filename,filename);

    // Send query msg
    if (send(sockfd, psentbuf, sizeof(sentbuf), 0) < 0)
    {
        printf("Send data error: %s(errno: %d)\n", strerror(errno), errno);
    }

    printf("*Sent file query: %s\n", filename);

    //waiting for the return msg
    struct SimpleUDPmsg recvbuf;
    struct SimpleUDPmsg *precvbuf = &recvbuf;
    socklen_t addrlen = sizeof(struct sockaddr_in);


    if( recv(sockfd, precvbuf, sizeof(recvbuf), 0) < 0 ){
        printf("Socket error %s(errno: %d)\n", strerror(errno),errno);
        return -1;
    }

    if(recvbuf.filesize == -1){
        return -2;
    }
    //printf("*File size: %d\n", recvbuf.filesize);
    return recvbuf.filesize;
    
}

vector<int> getActiveSockList(vector<SimpleAddress> list, const char* filename){
    vector<int> socklist;
    for(auto add:list){
        struct sockaddr_in serv_addr;
        struct sockaddr_in* serv_addrp = &serv_addr;
        int sockfd = 0;
        // Create socket address
        if(createSocketAddr(serv_addrp, add.address, add.port) < 0){
            printf("\n Invalid address: %s\n", add.address);
        }

        if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ){
            printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
            continue;
        }
        // Set timeout
        setTimeout(sockfd, 1, 1);

        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        { 
            printf("\n Connection Failed %s(errno: %d)\n",strerror(errno),errno);
            continue;
        } 

        if(getFilesize(sockfd, filename) == -1){
            //connection failed 
            //printf("Failed to connect this server");
            continue;
        }

        //valid socket save it
        socklist.push_back(sockfd);
    }
    return socklist;
}

void *downloadThread(void *arg){
    struct ThreadAttri *temp;
    temp = (struct ThreadAttri *)arg;
    // Get Attri
    int connectNum = temp -> connectNum;
    int *socklist = temp -> socklist;
    int t_idx = temp -> t_idx;
    // Create socket
    int sockfd = socklist[t_idx];
    printf("Thread %d start\n", t_idx);

    // Send read filename request
    char* filename = temp -> filename; //only the ori filename will be modified as create new file
    struct SimpleUDPmsg sentbuf;
    struct SimpleUDPmsg *psentbuf = &sentbuf;
    // Build msg packet
    sentbuf.code = 2;
    sentbuf.offset = t_idx;
    sentbuf.chunksize = chunkSize;
    strcpy(sentbuf.filename,filename);
    // Send request
    if (send(sockfd, psentbuf, sizeof(sentbuf), 0) < 0)
    {
        printf("Send data error: %s(errno: %d)\n", strerror(errno), errno);
    }

    printf("*Sent file read request: %s\n", filename);

    // Recv file and store
    printf("*Server return message: \n");
    struct SimpleUDPmsg recvbuf;
    struct SimpleUDPmsg *precvbuf = &recvbuf;

    // Construct file path
    char filepath[MAX_PATH_LEN]; //*bug didn't check the len if filename too long 
    getFilepath(t_idx, filename, filepath);
    FILE* fp = fopen(filepath, "w");
    if(fp == NULL){
        printf("Create file error!\n");
        pthread_exit((void*)-1);
    }
    // // Got the new port from server
    // struct sockaddr_in server;
    // socklen_t addrlen = sizeof(struct sockaddr_in);
    // memset(&recvbuf,0,sizeof(recvbuf));

    // //Create new socket
    // int recvsock;
    // if( (recvsock = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ){
    //     printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
    //     fclose(fp);
    //     pthread_exit((void*)-1);
    // }
    // // Set timeout
    // setTimeout(recvsock, 1, 1);
    // int recv_size = recvfrom(sockfd,&recvbuf,sizeof(recvbuf),0,(struct sockaddr *)&server, &addrlen);
    // if(recv_size < 0){
    //     printf("recvfrom error: %s(errno: %d)\n",strerror(errno),errno);
    //     fclose(fp);
    //     pthread_exit((void*)-1);
    // }

    // // connect socket with address
    // if (connect(recvsock, (struct sockaddr *)&server, sizeof(server)) < 0) 
    // { 
    //     printf("\n Connection Failed %s(errno: %d)\n",strerror(errno),errno);
    //     fclose(fp);
    //     pthread_exit((void*)-1);
    // } 

    // // Save the first buffer
    // fwrite(recvbuf.buffer, sizeof(char), strlen(recvbuf.buffer), fp);

    while(1){

        memset(&recvbuf,0,sizeof(recvbuf));
        if( recv(sockfd, precvbuf, sizeof(recvbuf), 0) < 0 ){
            printf("Socket error %s(errno: %d)\n", strerror(errno),errno);
            break;
        }
        fwrite(recvbuf.buffer, sizeof(char), strlen(recvbuf.buffer), fp);

        // Send ACK
        sentbuf.code = 3;
        if (send(sockfd, psentbuf, sizeof(sentbuf), 0) < 0){
            printf("Send data error: %s(errno: %d)\n", strerror(errno), errno);
            break;
        }
    }
    // End of connection
    //close(sockfd);
    fclose(fp);
    pthread_exit((void*)1);
}


//start client
int main(int argc, char const *argv[]) 
{ 
    int sockfd = 0;
    struct sockaddr_in serv_addr; 
    int connectNum;
    if(argc != 4){
        printf("usage: ./myclient <server-info.txt> <num-chunks> <filename>\n");
        return 0;
    }
    // Handle the num-connections
    if(!checkdigit(argv[2])){
        printf("Invalid num-chunks\n");
        return 0;
    }
    if(atoi(argv[2]) > MAX_SERVER){
        printf("Excess the max num-chunks\n");
        return 0;
    }
    if(strlen(argv[3]) > MAX_FILENAME_LEN){
        printf("Excess the max filename length\n");
        return 0;
    }


    const char* serverFilename = argv[1]; //set servers list
    connectNum = atoi(argv[2]); //set connection num
    const char* filename = argv[3]; //set file name
    FILE *serverFp = fopen(serverFilename, "r");
    if(serverFp == NULL){
        printf("open server-info file error! No such file: %s\n", argv[1]);
        exit(1);
    }

    // Read address list
    //struct SimpleAddress addList[MAX_SERVER];
    vector<SimpleAddress> addList;
    char serverBuffer[1024];
    int add_index = 0;
    //add all address
    while(fgets(serverBuffer, 1024, serverFp) != NULL){
        struct SimpleAddress sa;
        printf("*address line: %s\n", serverBuffer);
        addList.push_back(getAddressbyLine(serverBuffer));
    }
    //add_index - 1 is the last address

    int port = addList[0].port;
    char* address = addList[0].address;

    fclose(serverFp);

    vector<int> vsocklist = getActiveSockList(addList, filename);

    //set connection num
    int activeNum = vsocklist.size();
    if(activeNum == 0){
        printf("No server aviliable! Check server-info.");
        return 0;
    }
    connectNum = min(connectNum, activeNum);

    //vector to int []
    int socklist[MAX_SERVER];
    for(int i =0; i < vsocklist.size(); ++i){
        socklist[i] = vsocklist[i];
        printf("socket id: %i \n", socklist[i]);
    }

    // Get the file size
    fileSize = getFilesize(socklist[0], filename);
    if(fileSize == -2){
        printf("*File not found: %s\n", filename);
    }else if(fileSize == -1){
        printf("Socket connection error, server failed\n");
    }
    printf("*Got file size: %d\n", fileSize);
    chunkSize = fileSize / connectNum;
//==================================Thread Start==========================================
    //create thread
    vector<pthread_t> threadList;
    vector<struct ThreadAttri*> attrilist;
    struct ThreadAttri *ptAttri;
    //struct ThreadAttri *ptAttri[connectNum];
    for(int i = 0; i < connectNum; ++i){//i >= number of server
        pthread_t thid;
        ptAttri = (struct ThreadAttri*)malloc(sizeof(struct ThreadAttri));
        attrilist.push_back(ptAttri);
        ptAttri->connectNum = connectNum;
        ptAttri->filesize = fileSize;
        memcpy(ptAttri->socklist,socklist,sizeof(socklist));
        // tAttri->socklist = socklist;
        ptAttri->t_idx = i;
        memcpy(ptAttri->filename,filename,sizeof(filename));
        // tAttri->filename = filename;
        if (pthread_create(&thid,NULL,downloadThread,(void*)ptAttri) == -1){
             printf("Thread create error!\n");
             return -1;
        }
        threadList.push_back(thid);
    }
    // Join thread
    for(int i = 0; i < connectNum; ++i){
        // int *t_res;
        // if (pthread_join(threadList[i], (void**)&t_res)){
        if (pthread_join(threadList[i], NULL)){ 
            printf("Thread is not exit...\n");
            return -1;
        } 
        // if(*t_res == -1){ 
        //     printf("Thread failed to download file\n");
        //     return 0;
        // }
        free(attrilist[i]);// Free attri after thread over
    }

//==================================Thread End==========================================


    //close socket
    for(int i = 0; i < connectNum; ++i){
        close(socklist[i]);
    }

    // // Filecat Assembling 
    // char combinefilepath[MAX_PATH_LEN];
    // strcpy(combinefilepath,DEST_PATH);
    // strcat(combinefilepath,filename);
    // FILE *fcombine = fopen(combinefilepath, "w");
    // if(fcombine == NULL){
    //     printf("Create final file error!\n");
    //     return 0;
    // }
    // for(int i = 0; i < connectNum; ++i){
    //     FILE *fpcat;
    //     char filepath[MAX_PATH_LEN];
    //     getFilepath(i, filename, filepath);
    //     //printf("Cat file %s\n", filepath);
    //     fpcat = fopen(filepath, "r");
    //     if(fpcat == NULL){
    //         printf("Missing file part error!\n");
    //         return 0;
    //     }
    //     filecat(fcombine, fpcat);
    //     fclose(fpcat);
    //     if(remove(filepath))
    //         printf("Could not delete the temp file %s \n", filepath);
    //     }
    // fclose(fcombine);
    
    return 0; 
} 