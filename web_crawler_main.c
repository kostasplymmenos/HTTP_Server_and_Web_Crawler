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
#include "mycrawler.h"

#define NO_DIR -1
#define DIR_ERR -2
#define T_ERR -3
#define NO_PORT -4


int main(int argc,char* argv[]){

    /* Args Management */
    char dir_name[NAME_MAX];
    int optchar;
    int num_threads = 3;
    short dir_given = 0;
    short host_given = 0;
    int cmd_port = 0;
    int serv_port = 0;
    char host_name[NAME_MAX];
    char starting_url[NAME_MAX];
    while((optchar = getopt(argc,argv,"h:p:c:t:d:")) != -1){
        if(optchar == 'd'){
            strcpy(dir_name,optarg);
            dir_given = 1;
        }
        if(optchar == 'h'){
            strcpy(host_name,optarg);
            host_given = 1;
        }
        if(optchar == 't')
            num_threads = atoi(optarg);
        if(optchar == 'p')
            serv_port = atoi(optarg);
        if(optchar == 'c')
            cmd_port = atoi(optarg);
    }
    if(argv[11] != NULL){
        strcpy(starting_url,argv[11]);
    }
    else{
        printf("No starting url\nUsage: ./mycrawler -h hostname -p http_port -c command_port -t num_of_threads -d saving_dir -u starting_url\nExiting...\n");
        exit(7);
    }
    if(cmd_port == 0 || serv_port == 0){
        printf("Must specify both http and command ports\nUsage: ./mycrawler -h hostname -p http_port -c command_port -t num_of_threads -d saving_dir -u starting_url\nExiting...\n");
        exit(NO_PORT);
    }
    if(dir_given == 0){
        printf("No saving dir specified\nUsage: ./mycrawler -h hostname -p http_port -c command_port -t num_of_threads -d saving_dir -u starting_url\nExiting...\n");
        exit(NO_DIR);
    }
    if(host_given == 0){
        printf("No host specified\nUsage: ./mycrawler -h hostname -p http_port -c command_port -t num_of_threads -d saving_dir -u starting_url\nExiting...\n");
        exit(6);
    }
    if(num_threads <= 0){
        printf("Argument -t <= 0\nUsage: ./mycrawler -h hostname -p http_port -c command_port -t num_of_threads -d saving_dir -u starting_url\nExiting...\n");
        exit(T_ERR);
    }
    if(num_threads > 30){
        printf("Too many Threads\nExiting...\n");
        exit(T_ERR);
    }
    printf("=== Starting Web Crawler ===\nThreads Serving: %d\nCommand Port: %d\nHttp Port: %d\nSaving web into directory: %s\nStarting at url: %s\n\n",num_threads,cmd_port,serv_port,dir_name,starting_url);


    crawlerEngine(host_name,dir_name,starting_url,serv_port,cmd_port,num_threads);
    printf("Received command to shutdown.Bye!\n");
    exit(0);
}
