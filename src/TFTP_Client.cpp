#include "TFTP_Client.h"

using namespace std;

int fileSize; //read only
int chunkSize; //read only
int numChunk;
int maxParallel;
vector<int> serverState; //use lock
vector<int> socklist;
vector<SimpleAddress> addList; //only read

pthread_mutex_t serverStateLock;

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

int fileAssemble(const char* filename, int numpart){
    // Filecat Assembling 
    char combinefilepath[MAX_PATH_LEN];
    strcpy(combinefilepath,DEST_PATH);
    strcat(combinefilepath,filename);
    FILE *fcombine = fopen(combinefilepath, "w");
    if(fcombine == NULL){
        printf("Create final file error!\n");
        return -1;
    }
    for(int i = 0; i < numpart; ++i){
        FILE *fpcat;
        char filepath[MAX_PATH_LEN];
        getFilepath(i, filename, filepath);
        //printf("Cat file %s\n", filepath);
        fpcat = fopen(filepath, "r");
        if(fpcat == NULL){
            printf("Missing file part error!\n");
            return -1;
        }
        filecat(fcombine, fpcat);
        fclose(fpcat);
        if(remove(filepath)){
            printf("Could not delete the temp file %s \n", filepath);
            return -1;
        }
    }
    fclose(fcombine);
    return 0;
}

int getActiveSockList(vector<SimpleAddress> &list, vector<int> &socklist, const char* filename){
    vector<SimpleAddress> activeList;
    for(auto add:list){
        struct sockaddr_in serv_addr;
        struct sockaddr_in* serv_addrp = &serv_addr;
        int sockfd = 0;
        // Create socket address
        if(createSocketAddr(serv_addrp, add.address, add.port) < 0){
            printf("\n Invalid address: %s\n", add.address);
            continue;
        }

        if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ){
            printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
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
        activeList.push_back(add);
        socklist.push_back(sockfd);
    }
    list = activeList;
    return 0;
}

int sendReadRequest(int sockfd, const char* filename, int offset){
    // Send read filename request
    struct SimpleUDPmsg sentbuf;
    struct SimpleUDPmsg *psentbuf = &sentbuf;
    // Build msg packet
    sentbuf.code = 2;
    sentbuf.offset = offset;
    sentbuf.chunksize = chunkSize;
    sentbuf.numchunk = numChunk;
    strcpy(sentbuf.filename,filename);
    // Send request
    if (send(sockfd, psentbuf, sizeof(sentbuf), 0) < 0)
    {
        return -1;
    }

    printf("*Sent file read request: %s\n", filename);
    return 0;
}

int downloadFile(int sockfd, const char* filename, int offset){
    printf("Download File \n");
    int recvsock;
    struct SimpleUDPmsg recvbuf;
    struct SimpleUDPmsg *precvbuf = &recvbuf;

    struct SimpleUDPmsg sentbuf;
    struct SimpleUDPmsg *psentbuf = &sentbuf;

    struct sockaddr_in server;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in* pserver = &server;
    //recv ACK new port from orignal port
    int recvPort;
    if( recv(sockfd, precvbuf, sizeof(recvbuf), 0) < 0 ){
            printf("Socket error %s(errno: %d)\n", strerror(errno),errno);
            return -1;
    }
    if(recvbuf.code == 3){
        recvPort = recvbuf.serverPort;
    }else{
        printf("Got new port error\n");
        return -1;
    }

    // Build new socket with new port 
    // Get the address
    char* address;
    for(int i = 0; i < socklist.size(); i++){
        if(socklist[i] == sockfd){
            address = addList[i].address;
        }
    }
    if( (recvsock = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ){
        printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    if(createSocketAddr(pserver, address, recvPort) < 0){
        printf("\n Invalid address: %s\n", address);
        return -1;
    }

    // Set timeout
    setTimeout(recvsock, 1, 1);

    if (connect(recvsock, (struct sockaddr *)&server, sizeof(server)) < 0) 
    { 
        printf("\n Connection Failed %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    } 
    printf("*New port is: %d\n", recvPort);
    printf("*New address is: %s\n", address);
    // Send ACK new port
    sentbuf.code = 3;
    if (send(recvsock, psentbuf, sizeof(sentbuf), 0) < 0){
        printf("Send ACK new error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    // Construct file path
    char filepath[MAX_PATH_LEN]; //*bug didn't check the len if filename too long 
    getFilepath(offset, filename, filepath);
    FILE* fp = fopen(filepath, "w");
    if(fp == NULL){
        printf("Create file error!\n");
        return -1;
    }
    while(1){
        memset(&recvbuf,0,sizeof(recvbuf));
        // if(recv(sockfd, precvbuf, sizeof(recvbuf), 0) < 0){
        //     printf("Socket recv error %s(errno: %d)\n", strerror(errno),errno);
        //     return -1;
        // }
        int recv_size =recvfrom(recvsock,&recvbuf,sizeof(recvbuf),0,NULL, NULL);
        if(recv_size < 0){
            printf("Socket recvfrom error %s(errno: %d)\n", strerror(errno),errno);
            return -1;
        }
        fwrite(recvbuf.buffer, sizeof(char), strlen(recvbuf.buffer), fp);

        // Send ACK
        sentbuf.code = 3;
        if (send(recvsock, psentbuf, sizeof(sentbuf), 0) < 0){
            printf("Send ACK error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }
        if(recvbuf.code == 3){//last packet close the connection
            printf("*End of file chunk!\n");
            break;
        }
    }
    // End of connection
    fclose(fp);
}

void *downloadThread(void *arg){
    struct ThreadAttri *temp;
    temp = (struct ThreadAttri *)arg;
    // Get Attri
    int t_idx = temp -> t_idx;
    char* filename = temp -> filename;

    // Find a Server *suppose all server will not down
    int sockfd;
    pthread_mutex_lock(&serverStateLock);
    int serveridx = 0;
    for(serveridx = 0; serveridx < serverState.size(); ++serveridx){
        if(serverState[serveridx] > 0 && serverState[serveridx] < maxParallel){
            serverState[serveridx]++;
            sockfd = socklist[serveridx];
        }
    }
    pthread_mutex_unlock(&serverStateLock);
    // Create socket
    
    printf("Thread %d start Using sock: %d\n", t_idx, serveridx);

    // Send read filename request will return new port and download part use
    if(sendReadRequest(sockfd, filename, t_idx) == -1){
        printf("Send data error: %s(errno: %d)\n", strerror(errno), errno);
        pthread_exit((void*)-1);
    }
    //downloadFile
    if(downloadFile(sockfd, filename, t_idx) == -1){
        printf("download file error: %s(errno: %d)\n", strerror(errno), errno);
        pthread_exit((void*)-1);
    }

    // End download
    pthread_mutex_lock(&serverStateLock);
    serverState[serveridx]--;
    pthread_mutex_unlock(&serverStateLock);

    pthread_exit((void*)1);
}


//start client
int main(int argc, char const *argv[]) 
{ 
    int sockfd = 0;
    struct sockaddr_in serv_addr; 
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

    numChunk = atoi(argv[2]); //set connection num
    const char* filename = argv[3]; //set file name
    FILE *serverFp = fopen(serverFilename, "r");
    if(serverFp == NULL){
        printf("open server-info file error! No such file: %s\n", argv[1]);
        exit(1);
    }

    // Read address list
    //struct SimpleAddress addList[MAX_SERVER];
    
    char serverBuffer[1024];
    int add_index = 0;
    //add all address
    while(fgets(serverBuffer, 1024, serverFp) != NULL){
        printf("*address line: %s\n", serverBuffer);
        addList.push_back(getAddressbyLine(serverBuffer));
    }
    //add_index - 1 is the last address

    int port = addList[0].port;
    char* address = addList[0].address;

    fclose(serverFp);
    printf("\n*Test server availability:\n");
    if(getActiveSockList(addList, socklist, filename) < 0){
        printf("Test server availability error\n");
        return -1;
    }

    //set connection num
    int const activeNum = socklist.size();
    if(activeNum == 0){
        printf("No server available! Check server-info.");
        return 0;
    }
    maxParallel = numChunk / activeNum + 1;
    //vector to int []
    printf("\n*Avaliable server list:\n");
    for(int i =0; i < socklist.size(); ++i){
        serverState.push_back(1);// servers green
        printf("Socket id: %i \n", socklist[i]);
        printf("Address: %s Port: %d \n", addList[i].address, addList[i].port);
    }

    // Get the file size
    fileSize = getFilesize(socklist[0], filename);
    if(fileSize == -2){
        printf("*File not found: %s\n", filename);
    }else if(fileSize == -1){
        printf("Socket connection error, server failed\n");
    }
    printf("*Got file size: %d\n", fileSize);
    chunkSize = fileSize / numChunk;
//==================================Thread Start==========================================
    //create thread
    vector<pthread_t> threadList;
    vector<struct ThreadAttri*> attrilist;
    struct ThreadAttri *ptAttri;
    for(int i = 0; i < numChunk; ++i){//i >= number of server
        pthread_t thid;
        ptAttri = (struct ThreadAttri*)malloc(sizeof(struct ThreadAttri));
        attrilist.push_back(ptAttri);
        ptAttri->filesize = fileSize;
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
    for(int i = 0; i < numChunk; ++i){
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
    for(int i = 0; i < numChunk; ++i){
        close(socklist[i]);
    }

    // Filecat Assembling 
    if(fileAssemble(filename, numChunk) < 0){
        printf("File assembling error!\n");
    }
    
    return 0; 
} 