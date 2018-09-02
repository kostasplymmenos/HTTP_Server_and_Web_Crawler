#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "limits.h"
#include "fcntl.h"
#include "signal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "mycrawler.h"
#include "errno.h"
#include "poll.h"
#include "time.h"

void signal_handler(int sig){
    if(sig == SIGINT)
        printf("Received SIGINT signal. If you want to terminate this app send 'SHUTDOWN' in command port.\n");
}

/* Sychronization */
pthread_mutex_t  mtx;
pthread_cond_t  cond_nonempty;

/* Global Parameters */
int t_port;
char* t_hostname;
char* save_dir;
int total_downloaded;
long total_bytes_dl;

/* Thread handling variables */
int threads_exited;
int links_cur_num;  /* number of currently processed links and not in the list */
char** links_cur_processing;    /* the above links */

int analyzeWebpageLinks(UrlList *urllist,char *text,int file_len){
    for(int i = 0; i < file_len - 8; i++){
        if(text[i] == '<' && text[i+1] == 'a' && text[i+2] == ' ' && text[i+3] == 'h' && text[i+4] == 'r' \
&& text[i+5] == 'e' && text[i+6] == 'f' && text[i+7] == '='){
            char* pend = strstr(&text[i],"\">");
            int linklen = pend-1-&text[i+9];
            char* link = malloc(linklen+1);
            strncpy(link,&text[i+9],linklen);
            link[linklen] = '\0';
            //printf("Link found: %s\n",link);
            char* path = malloc(strlen(link)+strlen(save_dir)+3);
            strcpy(path,"./");
            strcat(path,save_dir);
            strcat(path,link);
            if(access(path, F_OK ) == 0 ){
                //printf("File exists: %s\n",path);
            }
            else{
                //perror("access");
                int exists = 0;
                UrlListNode *temp1 = urllist->head;
                while(temp1 != NULL){
                    if(strcmp(link,temp1->url) == 0) {exists = 1; }
                    temp1 = temp1->next;
                }
                for(int l = 0; l < links_cur_num; l++){
                    if(strcmp(link,links_cur_processing[l]) == 0){exists = 1;}
                }
                if(exists == 0){
                    pthread_mutex_lock (&mtx);
                    addUrltoList(urllist,link);
                    pthread_mutex_unlock (&mtx);
                }
            }
            free(link);
            free(path);
        }
    }
    return 0;
}

int addUrltoList(UrlList *urllist,char* newurl){
    UrlListNode *newnode = malloc(sizeof(UrlListNode));
    newnode->url = malloc(strlen(newurl)+1);
    strcpy(newnode->url,newurl);
    newnode->next = NULL;
    if(urllist->size == 0){
        urllist->end = newnode;
        urllist->end->next = NULL;
        urllist->head = urllist->end;
    }
    else{
        urllist->end->next = newnode;
        urllist->end = newnode;
    }
    urllist->size++;
    pthread_cond_signal(&cond_nonempty);
}
int downloadWebpage(int sock, UrlList* urllist){
    pthread_mutex_lock (&mtx);

    /* Crawling completion condition empty list AND no thread alive*/
    while(urllist->size <= 0 && links_cur_num <= 0){
        pthread_mutex_unlock (&mtx);
        return 0;
    }

    /* If list empty but threads processing pages wait for signal of non empty list */
    while(urllist->size <= 0){
        printf("Found list empty.Waiting...\n");
        pthread_cond_wait(&cond_nonempty,&mtx);
        if(links_cur_num <= 0){
            pthread_mutex_unlock (&mtx);
            return 0;
        }
    }

    /* Take a url from the list */
    char* url = malloc(strlen(urllist->head->url)+1);
    strcpy(url,urllist->head->url);
    printf("%lu> Downloading page: %s\n",pthread_self(),url);
    /* Save it as currently processing in order ot not download it again */
    links_cur_processing[links_cur_num] = url;
    links_cur_num++;

    UrlListNode* temp = urllist->head;
    urllist->head = urllist->head->next;
    urllist->size--;
    free(temp->url);
    free(temp);
    pthread_mutex_unlock (&mtx);

    /* Send GET request of the url to server */
    char* http_get = malloc(512);
    strcpy(http_get,"GET ");
    strcat(http_get,url);
    strcat(http_get," HTTP/1.1\n");
    strcat(http_get,"User-Agent: MyCrawler/1.0\nHost: ");
    strcat(http_get,t_hostname);
    strcat(http_get,"\n\n");

    if(write(sock , http_get , 512) < 0)
        perror("write");

    //printf("MESSAGE SENT:\n%s\n",http_get);

    char* header_buf = malloc(512);

    /* Read the HTTP Header to determine file length */
    int nread = 0;
    int nr;
    while((nr = read(sock , header_buf+nread , 511-nread)) != 0)
        nread += nr;
    header_buf[511] = '\0';

    //printf("MESSAGE RECEIVED:\n%s\n",header_buf);
    char* occur = strstr(header_buf,"Content-Length");
    int file_len = atoi(&occur[16]);

    /* Read file from socket chunk by chunk and store it to memory */

    char* file_content = malloc(file_len+1);
    char* end_header = strstr(header_buf,"\n\n");
    nread = header_buf+507 - end_header+2;
    strncpy(file_content,end_header+2,nread);
    nr = 0;
    while(nread != file_len){
        if((nr = read(sock , file_content+nread, file_len-nread)) < 0)
            perror("read");
        nread += nr;
    }
    file_content[file_len] = '\0';
    close(sock);
    /* Write the file to disk */

    int res1 = writeFileToDisk(save_dir,url,file_content,file_len);


    /* Analyze downloaded page for further links and store them to urllist */
    int res2;

    if(res1 == 0)
        res2 = analyzeWebpageLinks(urllist,file_content,file_len);

    /* Done with the currently processing link */
    pthread_mutex_lock (&mtx);
    links_cur_num--;
    pthread_mutex_unlock (&mtx);


    free(header_buf);
    free(http_get);
    free(url);
    free(file_content);

}

void *threadExecuteTask(void* v_urllist){
    /* Initialize socket and structures */
    UrlList *urllist = (UrlList*) v_urllist;
    int sock;
    struct sockaddr_in  server;
    struct sockaddr *serverptr = (struct sockaddr *)&server;
    struct hostent *rem;

    if((rem = gethostbyname(t_hostname)) == NULL) {
        herror("gethostbyname"); exit(1);
    }

    server.sin_family = AF_INET;
    memcpy (& server.sin_addr , rem ->h_addr , rem ->h_length);
    server.sin_port = htons(t_port);

    printf("Thread Connecting to %s port %d\n",t_hostname,t_port);

    while(1){

        /* Assuming connections are not persistent */
        if((sock = socket(AF_INET , SOCK_STREAM , 0)) < 0)
            perror("socket");

        if(connect(sock , serverptr ,sizeof(server)) < 0)
            perror("connect");

        /* Pick a url from urllist, download the page
        and analyze it for further links */
        if(downloadWebpage(sock,urllist) == 0)
		      break;
    }

    printf("Thread Completed Crawling\n");
    //Signal other threads to check for condition in order to terminate */
    pthread_cond_signal(&cond_nonempty);
    threads_exited++;
    pthread_exit(NULL);
}

void crawlerEngine(char* hostname,char* sav_dir,char* starting_url,int http_port,int cmd_port,int nthreads){
    /* Initialize variables, locks, structures etc */
    signal(SIGINT,signal_handler);
    time_t start_time = time(NULL);
    total_downloaded = 0;
    total_bytes_dl = 0;
    links_cur_num = 0;
    save_dir = sav_dir;
    links_cur_processing = malloc(nthreads*sizeof(char*));

    UrlList *urllist = malloc(sizeof(UrlList));
    urllist->size = 0;
    urllist->head = NULL;
    urllist->end = NULL;

    pthread_t threadarr[nthreads];
    pthread_mutex_init (&mtx , 0);
    pthread_cond_init (& cond_nonempty , 0);
    t_port = http_port;
    t_hostname = hostname;
    threads_exited = 0;

    /* Send GET http request to download the starting webpage */
    char* http_get = malloc(512);
    strcpy(http_get,"GET ");
    strcat(http_get,starting_url);
    strcat(http_get," HTTP/1.1\n");
    strcat(http_get,"User-Agent: MyCrawler/1.0\nHost: ");
    strcat(http_get,hostname);
    strcat(http_get,"\n\n");
    //printf("MESSAGE SENT:\n%s\n",http_get);

    /* Initialize socket */
    int sock;
    struct sockaddr_in  server;
    struct sockaddr *serverptr = (struct sockaddr *)&server;
    struct hostent *rem;

    if((sock = socket(AF_INET , SOCK_STREAM , 0)) < 0)
        perror("socket");
    /*  Find  server  address  */
    if((rem = gethostbyname(hostname)) == NULL) {
        herror("gethostbyname"); exit(1);
    }

    server.sin_family = AF_INET;
    memcpy (& server.sin_addr , rem ->h_addr , rem ->h_length);
    server.sin_port = htons(http_port);

    /* Connect to server and write GET request to socket */
    if(connect(sock , serverptr ,sizeof(server)) < 0)
        perror("connect");

    printf("Connecting to %s port %d\n",hostname,http_port);

    if(write(sock , http_get , 512) < 0)
        perror("write");

    /* Read the http response header to determine file length */
    char* header_buf = malloc(512);

    int nread = 0;
    int nr;
    while((nr = read(sock , header_buf+nread , 511-nread)) != 0)
        nread += nr;
    header_buf[511] = '\0';

    char* occur = strstr(header_buf,"Content-Length");
    int file_len = atoi(&occur[16]);

    /* Read the file from the socket and store it to memory */
    char* file_content = malloc(file_len+2);
    char* end_header = strstr(header_buf,"\n\n");
    nread = header_buf+507 - end_header+2;
    strncpy(file_content,end_header+2,nread);
    nr = 0;
    while(nread != file_len){
        if((nr = read(sock , file_content+nread, file_len-nread)) < 0)
            perror("read");
        nread += nr;
    }

    file_content[file_len] = '\0';

    /* Make the save directory */
    mkdir(save_dir,0777);

    /* Write the file from memory to disk */
    writeFileToDisk(sav_dir,starting_url,file_content,file_len);

    /* Analyze file for other links to be explored */
    analyzeWebpageLinks(urllist,file_content,file_len);

    /* Create threads to explore the web */
    for(int i = 0; i < nthreads; i++)
        pthread_create (&threadarr[i], NULL , (void*) threadExecuteTask , (void*) urllist);

    free(file_content);
    free(header_buf);
    free(http_get);

    /* Initialize another socket for commanding the crawler */
    int sock_cmd;
    int newsock;
    struct sockaddr_in  server_cmd;
    struct sockaddr_in  client;
    socklen_t  clientlen;
    struct sockaddr *server_cmdptr = (struct sockaddr*) &server_cmd;
    struct sockaddr *clientptr = (struct sockaddr*) &client;

    if((sock_cmd = socket(AF_INET , SOCK_STREAM , 0)) < 0)
        perror("socket");

    server_cmd.sin_family = AF_INET;
    server_cmd.sin_addr.s_addr = htonl(INADDR_ANY);
    server_cmd.sin_port = htons(cmd_port);

    if (bind(sock_cmd , server_cmdptr ,sizeof(server_cmd)) < 0)
        perror("bind");

    if(listen(sock_cmd , 100) < 0)
        perror("listen");

    printf("Listening  for  commands  to port %d\n", cmd_port);

    /* When first search command occurs, jobExecutor is created and jobExecOn == 1 */
    int jobExecOn = 0;

    struct pollfd fds;
    fds.fd = sock_cmd;
    fds.events = POLLIN;

    char* fifo_name = malloc(strlen("_crawler_je_fifo_")+1);
    strcpy(fifo_name,"_crawler_je_fifo_");
    int fifo_fd;
    pid_t jobExecutor_pid;

    while(1){
        /* Whenever there is a connection request unblock and read command */
        int retval = poll(&fds,(unsigned long) 1,-1);

        if(fds.revents == POLLIN){
            if((newsock = accept(sock_cmd , clientptr , &clientlen)) < 0)
                perror("accept");
            printf("Accepted Connection\n");

            char *buffer = malloc(128);
            int msg_size = read(newsock , buffer , 128);
            buffer[127] = '\0';

            char *command = malloc(128);
            sscanf(buffer,"%s",command);

            /* Return uptime , pages downloaded etc */
            if(strcmp(command,"STATS") == 0){
                char* str = malloc(64);
                time_t uptime = time(NULL) - start_time;
                int hours = uptime / 3600;
                int min = ( uptime - hours*3600 ) / 60;
                int sec =  uptime - hours*3600 - min*60;

                write(newsock,"Crawler Uptime ",15);
                sprintf(str,"%d",hours);
                write(newsock,str,strlen(str));
                write(newsock,":",1);
                sprintf(str,"%d",min);
                write(newsock,str,strlen(str));
                write(newsock,":",1);
                sprintf(str,"%d",sec);
                write(newsock,str,strlen(str));
                write(newsock," Downloaded: ",13);
                sprintf(str,"%d",total_downloaded);
                write(newsock,str,strlen(str));
                write(newsock," webpages,",10);
                sprintf(str,"%ld",total_bytes_dl);
                write(newsock,str,strlen(str));
                write(newsock," bytes\n",7);
                free(str);

            }
            /* Shutdowns crawler deallocating resources */
            else if(strcmp(command,"SHUTDOWN") == 0){
                close(newsock);
                close(sock);
                /* If JobExecutor is on send /exit command through pipe */
                if(jobExecOn == 1){
                    if((fifo_fd = open(fifo_name, O_WRONLY)) < 0){
                        perror("open");
                    }
                    char *exit_msg = malloc(32);
                    strcpy(exit_msg,"/exit ");
                    int msg_len = strlen(exit_msg);
                    write(fifo_fd,&msg_len,sizeof(int));

                    write(fifo_fd,exit_msg,msg_len);
                    close(fifo_fd);
                    free(exit_msg);
                }

                free(urllist);
                free(links_cur_processing);
                return ;
            }
            /* If crawling has completed search the downloaded pages with the help
            of JobExecutor */
            else if(strcmp(command,"SEARCH") == 0){

                if(threads_exited < nthreads){
                    write(newsock,"CRAWLING IN PROGRESS\n",21);
                }
                else{
                    /* If JobExecutor isn't on start it */
                    if(jobExecOn == 0){
                        /* Make fifo for the two processes to communicate */

                        if(mkfifo(fifo_name, 0666) == -1){
                            perror("mkfifo");
                        }
                        /* Fork and exec JobExecutor user for search queries*/
                        jobExecutor_pid = fork();
                        if(jobExecutor_pid == 0){
                            if(execl("./jobExecutor/build/jobExecutor", "./jobExecutor/build/jobExecutor", "-d", "docfile.txt", "-w", "3","-p",fifo_name, (char *) 0) == -1)
                                perror("execl");
                        }
                        jobExecOn = 1;
                    }
                    /* If it's on continue sending queries */
                    if((fifo_fd = open(fifo_name, O_WRONLY)) < 0){
                        perror("open");
                    }
                    /* Send the first query to JobExecutor through pipe*/
                    char *msg_to_send = malloc(128);
                    strcpy(msg_to_send,"/search ");
                    printf("Read size: %d\n",msg_size);
                    strncpy(msg_to_send+8,buffer+strlen(command),msg_size-strlen(command)-1);


                    int msg_len_send = 8+msg_size-strlen(command);
                    //printf("Query size: %d\n",msg_len_send);
                    msg_to_send[msg_len_send-1] = '\0';
                    write(fifo_fd,&msg_len_send,sizeof(int));

                    write(fifo_fd,msg_to_send,msg_len_send);
                    close(fifo_fd);
                    int msg_len_rcv = 0;

                    if((fifo_fd = open(fifo_name, O_RDONLY)) < 0){
                        perror("open");
                    }
                    read(fifo_fd,&msg_len_rcv,sizeof(int));
                    char* rcv_buf = malloc(msg_len_rcv+1);
                    int nread = 0;
                    int nr;
                    while(nread != msg_len_rcv){
                        nr = read(fifo_fd,rcv_buf,msg_len_rcv);
                        nread += nr;
                    }
                    close(fifo_fd);
                    rcv_buf[msg_len_rcv] = '\0';

                    int nwrite = 0;
                    int nw;
                    while(nwrite != msg_len_rcv){
                        nw = write(newsock,rcv_buf+nwrite,msg_len_rcv-nwrite);
                        nwrite += nw;
                    }
                    write(newsock,"DONE\n\0",6);
                    free(rcv_buf);
                    free(msg_to_send);
                }
            }
            free(buffer);
            free(command);
            printf("Closing Connection\n");
            fds.revents = -1;
            if(close(newsock) == -1)
                perror("close");
        }
    }
}


int writeFileToDisk(char* d_save,char* url,char* content,int file_len){
    char* saveptr; //for strtok_r (safe)
    char* curpath = malloc(512);
    strcpy(curpath,"./");
    strcat(curpath,d_save);

    char* url_dupl = malloc(strlen(url)+2);
    strcpy(url_dupl,url);
    url_dupl[strlen(url)] = '\n';
    url_dupl[strlen(url)+1] = '\0';
    char delim[2] = "/";
    char* token = strtok_r(url_dupl,delim,&saveptr);
    while(token != NULL){
        struct stat st;
        strcat(curpath,"/");
        strcat(curpath,token);
        if(stat(curpath, &st) == -1){
            if(mkdir(curpath,0777) == -1)
                perror("mkdir");
            printf("Making dir: %s\n",curpath);
            int docfile;
            pthread_mutex_lock (&mtx);
            if((docfile = open("./docfile.txt",O_CREAT|O_APPEND|O_WRONLY,0777)) < 0 )
                perror("open");
            if(write(docfile , curpath+2 , strlen(curpath)-2) < 0)
                perror("write");
            if(write(docfile , "\n" , 1) < 0)
                perror("write");
            close(docfile);
            pthread_mutex_unlock (&mtx);
        }

        token = strtok_r(NULL,delim,&saveptr);
        if(strstr(token,"\n") != NULL){ //means that we reached end of path
            break;
        }
    }
    char* outpath = malloc(512);
    strcpy(outpath,"./");
    strcat(outpath,d_save);
    strcat(outpath,url);

    int nwrite = 0;
    int nw;
    int outfile;
    if((outfile = open(outpath,O_CREAT|O_WRONLY,0777)) < 0 )
        perror("open");

    while(nwrite != file_len){
        if((nw = write(outfile , content+nwrite , file_len-nwrite)) < 0)
            perror("write");
        nwrite += nw;
    }
    int f;
    if((f = fsync(outfile)) == -1){
        perror("fsync");
    }

    pthread_mutex_lock (&mtx);
    total_downloaded++;
    total_bytes_dl += file_len;
    pthread_mutex_unlock (&mtx);

    if (close(outfile) < 0){
        perror("close");
    }
    free(outpath);
    free(url_dupl);
    free(curpath);
    return 0;
}
