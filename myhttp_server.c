#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "limits.h"
#include "fcntl.h"
#include "signal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "myhttp_server.h"
#include <errno.h>
#include <pthread.h>
#include "poll.h"
#include "time.h"

#define EXIT_CODE -2

void signal_handler(int sig){
    if(sig == SIGINT)
        printf("Received SIGINT signal. If you want to terminate this app send 'SHUTDOWN' in command port.\n");
}

int total_served;
long total_bytes_served;
int exit_flag;

pthread_mutex_t  mtx;
pthread_cond_t  cond_nonempty;
pthread_cond_t  cond_nonfull;

/* Http Server initialization */
int httpServerInit(HTTPServer* server,int serv_p,int cmd_p,int thread_num){
    server->serv_port = serv_p;
    server->cmd_port = cmd_p;
    server->nthreads = thread_num;
    server->request_pool = malloc(sizeof(HTTPRequestPool));
    server->request_pool->size = 0;
    server->request_pool->start = 0;
    server->request_pool->end = -1;
    pthread_mutex_init (&mtx , 0);
    pthread_cond_init (& cond_nonempty , 0);
    pthread_cond_init (& cond_nonfull , 0);

}

/* Add file descriptor waiting to be served to buffer*/
int httpRequestAdd(HTTPServer* server,int filed){
    pthread_mutex_lock (&mtx);
    while(server->request_pool->size  >= MAX_REQUESTS) {
        // If buffer full wait here until space available
        pthread_cond_wait (&cond_nonfull , &mtx);
    }
    server->request_pool->end = (server->request_pool->end + 1) % MAX_REQUESTS;
    server->request_pool->data_array[server->request_pool->end] = filed;
    server->request_pool->size++;
    pthread_mutex_unlock (&mtx);
}

/* Take file descriptor from buffer to serve request */
int httpRequestTake(HTTPServer* server){
    pthread_mutex_lock (&mtx);
    while(server->request_pool->size <= 0) {
        /* waits if buffer empty until a request arrives
        if exit flag is TRUE return EXIT_CODE to thread in order to exit */
        pthread_cond_wait (& cond_nonempty , &mtx);
        if(exit_flag == 1){
            pthread_mutex_unlock (&mtx);
            return EXIT_CODE;
        }
    }
    int fd = server->request_pool->data_array[server->request_pool->start];
    server->request_pool->start = (server->request_pool->start + 1) % MAX_REQUESTS;
    server->request_pool->size--;
    pthread_mutex_unlock (&mtx);
    return fd;
}

/* Server's main function, accepts connections stores them in a buffer
and then threads take the file descriptors and respond with the site requested */
int httpServerListen(HTTPServer* http_server){
    /* Create threads */
    pthread_t threadarr[http_server->nthreads];
    for(int i = 0; i < http_server->nthreads; i++)
        pthread_create (&threadarr[i], NULL , threadExecuteRequest , (void*) http_server);

    /* Initialize variables */
    signal(SIGINT,signal_handler);
    time_t start_time = time(NULL);
    total_served = 0;
    total_bytes_served = 0;
    exit_flag = 0;

    /* Initialize sockets */
    int sock_serv;
    int sock_cmd;
    int newsock;
    struct sockaddr_in  server_serv;
    struct sockaddr_in  server_cmd;
    struct sockaddr_in  client;
    socklen_t  clientlen;
    struct sockaddr *server_servptr = (struct sockaddr*) &server_serv;
    struct sockaddr *server_cmdptr = (struct sockaddr*) &server_cmd;
    struct sockaddr *clientptr = (struct sockaddr*) &client;
    struct hostent *rem;

    if((sock_serv = socket(AF_INET , SOCK_STREAM , 0)) < 0)
        perror("socket");

    if((sock_cmd = socket(AF_INET , SOCK_STREAM , 0)) < 0)
        perror("socket");

    server_serv.sin_family = AF_INET;
    server_serv.sin_addr.s_addr = htonl(INADDR_ANY);
    server_serv.sin_port = htons(http_server->serv_port);

    server_cmd.sin_family = AF_INET;
    server_cmd.sin_addr.s_addr = htonl(INADDR_ANY);
    server_cmd.sin_port = htons(http_server->cmd_port);

    if (bind(sock_serv , server_servptr ,sizeof(server_serv)) < 0)
        perror("bind");

    if (bind(sock_cmd , server_cmdptr ,sizeof(server_cmd)) < 0)
        perror("bind");

    if(listen(sock_serv , 100) < 0)
        perror("listen");

    if(listen(sock_cmd , 10) < 0)
        perror("listen");

    printf("Listening  for  connections  to port %d\n", http_server->serv_port);
    printf("Listening  for  commands  to port %d\n", http_server->cmd_port);


    /* Used for multiplexing connections from serving and command port */
    struct pollfd fds[2];
    fds[0].fd = sock_serv;
    fds[0].events = POLLIN;
    fds[1].fd = sock_cmd;
    fds[1].events = POLLIN;

    while(1){
        /* Whenever there is a connction pending poll returns and server accepts the connection */
        int retval = poll(fds,(unsigned long) 2,-1);

        /* This accepts connections from serving port and stores opened
        file descriptors to buffer for threads to take */
        if(fds[0].revents == POLLIN){
            if((newsock = accept(fds[0].fd , clientptr , &clientlen)) < 0)
                perror("accept");
            httpRequestAdd(http_server,newsock);
            pthread_cond_signal (& cond_nonempty);
        }
        /* This is for connections in command port */
        if(fds[1].revents == POLLIN){
            if((newsock = accept(fds[1].fd , clientptr , &clientlen)) < 0)
                perror("accept");

            printf("Accepted socket for command\n");
            char *buffer = malloc(128);
            read(newsock , buffer , 128);

            /* Extract command from buffer */
            char *command = malloc(128);
            sscanf(buffer,"%s",command);

            /* This command returns server stats, uptime pages served etc */
            if(strcmp(command,"STATS") == 0){
                /* Convert secs to hours:minutes.secs */
                char* str = malloc(64);
                time_t uptime = time(NULL) - start_time;
                int hours = uptime / 3600;
                int min = ( uptime - hours*3600 ) / 60;
                int sec =  uptime - hours*3600 - min*60;

                write(newsock,"Server Uptime ",14);
                sprintf(str,"%d",hours);
                write(newsock,str,strlen(str));
                write(newsock,":",1);
                sprintf(str,"%d",min);
                write(newsock,str,strlen(str));
                write(newsock,".",1);
                sprintf(str,"%d",sec);
                write(newsock,str,strlen(str));
                write(newsock," Served: ",9);
                sprintf(str,"%d",total_served);
                write(newsock,str,strlen(str));
                write(newsock," webpages,",10);
                sprintf(str,"%ld",total_bytes_served);
                write(newsock,str,strlen(str));
                write(newsock," bytes\n",7);
                free(str);
            }
            else if(strcmp(command,"SHUTDOWN") == 0){
                /* Set exit flag TRUE and signal threads which are blocked in
                httpRequestTake function waiting for file descriptors to exit
                Then wait for threads to exit and return */
                printf("Server Shutting down\n");
                close(newsock);
                exit_flag = 1;
                free(http_server->request_pool);
                pthread_cond_broadcast (& cond_nonempty);
                for(int i = 0; i < http_server->nthreads; i++)
                    pthread_join(threadarr[i],NULL);

                return 0;
            }
            free(buffer);
            free(command);
            printf("Closing Connection\n");
            close(newsock);
        }
        fds[0].revents = -1;
        fds[1].revents = -1;
    }
}

/* Thread takes accepted connection's file desc and serves the requested file */
void *threadExecuteRequest(void* http_serv){
    HTTPServer* http_server = http_serv;
    char *buf = malloc(4096);
    while(1){
        /* Take file descriptor to serve request if exists else block in condition */
        int newfd = httpRequestTake(http_server);
        /* Request taken so buffer not full */
        pthread_cond_signal (& cond_nonfull);
        if(newfd == EXIT_CODE){
            printf("Thread Received Command to Exit\n");
            free(buf);
            pthread_exit(0);
        }
        /* Read request header */
        if(read(newfd , buf , 4096) < 0)
            perror("read");

        /* err == 404 not found , err == 403 permission denied */
        int err = 200;
        buf[4095] = '\0';
        printf("MESSAGE :\n%s\n",buf);

        /* Extract file wanted */
        char *path_wanted = malloc(512);
        path_wanted[0] = '.';
        sscanf(buf+4,"%s",path_wanted+1);

        /* Open file, if not exists error 404, if permission denied error 403 */
        FILE* file;
        if((file = fopen(path_wanted,"r")) == NULL){
            perror("fopen");
            if(errno == EACCES) err = 403;
            else if(errno == ENOENT) err = 404;
        }
        int fsize = 0;
        char *buf2 = NULL;
        if(err == 200){
            /* Count file length, read it from disk and concat it with http response header */
            fseek(file, 0, SEEK_END);
            fsize = ftell(file);
            fseek(file, 0, SEEK_SET);

            buf2 = malloc(fsize + 1024);
            int len = strlen("HTTP/1.1 200 OK\nDate: Mon, 27 May 2018 12:28:53 GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: ");
            strcpy(buf2,"HTTP/1.1 200 OK\nDate: Mon, 27 May 2018 12:28:53 GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: ");
            char *content_buf = malloc(fsize);
            fread(content_buf, fsize, 1, file);
            sprintf(buf2+len,"%d",fsize);

            strcat(buf2,"\nContent-Type: text/html\nConnection: Closed\n\n");
            strcat(buf2,content_buf);
            free(content_buf);
            fclose(file);

        }
        else if(err == 403){
            /* Send Error 403 Response */
            buf2 = malloc(1024);
            strcpy(buf2,"HTTP/1.1 403 Forbidden \nDate: Mon, 27 May 2018 12:28:53 GMT\nServer: my\
    httpd/1.0.0 (Ubuntu64)\nContent-Length: 63\nContent-Type: text/html\nConnection: Closed\n\n<html><body><h1>Permission Denied Error 403</h1></body></html>");
        }
        else if(err == 404){
            /* Send Error 404 Response */
            buf2 = malloc(1024);
            strcpy(buf2,"HTTP/1.1 404 Not Found\nDate: Mon, 27 May 2018 12:28:53 GMT\nServer: my\
    httpd/1.0.0 (Ubuntu64)\nContent-Length: 60\nContent-Type: text/html\nConnection: Closed\n\n<html><body><h1>File not found Error 404</h1></body></html>");
        }
        else{
            printf("Unknown Error\n");
        }

        printf("Response code HTTP %d\n",err);
        buf2[fsize+1023] = '\0';
        /* Write appropriate response to socket */
        if(write(newfd , buf2 , fsize+1024) < 0)
            perror("write");

        /* Update global variables */
        pthread_mutex_lock (&mtx);
        total_served++;
        total_bytes_served += fsize;
        pthread_mutex_unlock (&mtx);

        printf("Server Responded\n");
        if(buf2 != NULL)
            free(buf2);
        free(path_wanted);
        printf("Closing  connection\n=========================\n\n");
        close(newfd);
    }
}
