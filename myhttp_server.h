#ifndef _MYHTTPSERVER_
#define _MYHTTPSERVER_

#define MAX_REQUESTS 100000

typedef struct httpRequestPool{
    int start;
    int end;
    int size;
    int data_array[MAX_REQUESTS];
}HTTPRequestPool;

typedef struct httpServer{
    int serv_port;
    int cmd_port;
    int nthreads;
    HTTPRequestPool* request_pool;
}HTTPServer;

int httpServerInit(HTTPServer*,int,int,int);
int httpServerListen(HTTPServer*);
int httpRequestAdd(HTTPServer*,int);
int httpRequestTake(HTTPServer*);
void *threadExecuteRequest(void*);
#endif
