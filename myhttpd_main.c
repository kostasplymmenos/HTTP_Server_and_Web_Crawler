#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "limits.h"
#include "fcntl.h"
#include "signal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "myhttp_server.h"
#include <pthread.h>

#define NO_DIR -1
#define DIR_ERR -2
#define T_ERR -3
#define NO_PORT -4

int main(int argc,char* argv[]){

    /* Args Management */
    char dir_name[NAME_MAX];
    int optchar;
    int num_threads = 3;
    char dirgiven = 0;
    int cmd_port = 0;
    int serv_port = 0;
    while((optchar = getopt(argc,argv,"p:c:t:d:")) != -1){
        if(optchar == 'd'){
            strcpy(dir_name,optarg);
            dirgiven = 1;
        }
        if(optchar == 't')
            num_threads = atoi(optarg);
        if(optchar == 'p')
            serv_port = atoi(optarg);
        if(optchar == 'c')
            cmd_port = atoi(optarg);
    }
    if(cmd_port == 0 || serv_port == 0){
        printf("Must specify both serving and command ports\nUsage: /myhttpd -p serving_port -c command_port -t num_of_threads -d root_dir\nExiting...\n");
        exit(NO_PORT);
    }
    if(dirgiven == 0){
        printf("No input dir\nUsage: /myhttpd -p serving_port -c command_port -t num_of_threads -d root_dir\nExiting...\n");
        exit(NO_DIR);
    }
    if(num_threads <= 0){
        printf("Argument -t <= 0\nUsage: /myhttpd -p serving_port -c command_port -t num_of_threads -d root_dir\nExiting...\n");
        exit(T_ERR);
    }
    if(num_threads > 30){
        printf("Too many Threads\nExiting...\n");
        exit(T_ERR);
    }
    printf("=== Starting HTTP Server ===\nThreads Serving: %d\nCommand Port: %d\nServing Port: %d\nServing directory: %s\n\n",num_threads,cmd_port,serv_port,dir_name);


    /* Server Sockets Initialization */
    printf("=== Server Initializing ===\n");
    HTTPServer* http_server = malloc(sizeof(HTTPServer));
    httpServerInit(http_server,serv_port,cmd_port,num_threads);

    printf("=== Server is up ===\n");
    httpServerListen(http_server);
    free(http_server);

    printf("=== Server Quit Safely Bye! ===\n");
    exit(0);
}

/* //Index Websites for cache
DIR* serving_dir;
struct dirent* pDirent;
int total_websites = 0;
int total_webpages = 0;
printf("=== Indexing WebSites ===\n");
if(dir_name != NULL){
    if((serving_dir = opendir(dir_name)) == NULL) {
       perror("1.opendir");
       exit(6);
    }
    while((pDirent = readdir(serving_dir)) != NULL) {
        if(pDirent->d_type == DT_DIR){
            if((strcmp(pDirent->d_name, ".") == 0) || ((strcmp(pDirent->d_name, "..") == 0)))
                continue;
            total_websites++;
            DIR* sub_dir;
            char sub_dir_path[PATH_MAX];
            strcpy(sub_dir_path,dir_name);
            strcat(sub_dir_path,pDirent->d_name);
            printf("sub dir path: %s\n",sub_dir_path);
            if((sub_dir = opendir(sub_dir_path)) == NULL) {
               perror("2.opendir");
               exit(6);
            }
            struct dirent* pDirent2;
            while((pDirent2 = readdir(sub_dir)) != NULL) {
                if(pDirent2->d_type == DT_REG){
                    total_webpages++;
                }
            }
            closedir(sub_dir);
        }
    }
    closedir(serving_dir);
}
else{
    printf("Dir Error Quiting.\n");
    exit(5);
}

int* website_map = malloc(total_websites*sizeof(int));
char** webpage_map = malloc(total_webpages*sizeof(char*));
if((serving_dir = opendir(dir_name)) == NULL) {
   perror("3.opendir");
   exit(6);
}
int dir_i = 0;
int page_i = 0;
while((pDirent = readdir(serving_dir)) != NULL) {
    if(pDirent->d_type == DT_DIR){
        if((strcmp(pDirent->d_name, ".") == 0) || ((strcmp(pDirent->d_name, "..") == 0)))
            continue;
        DIR* sub_dir;
        char sub_dir_path[PATH_MAX];
        strcpy(sub_dir_path,dir_name);
        strcat(sub_dir_path,pDirent->d_name);
        if((sub_dir = opendir(sub_dir_path)) == NULL) {
           perror("4.opendir");
           exit(6);
        }
        struct dirent* pDirent2;
        while((pDirent2 = readdir(sub_dir)) != NULL) {
            if(pDirent2->d_type == DT_REG){
                webpage_map[page_i] = malloc(strlen(pDirent2->d_name));
                strcpy(webpage_map[page_i],pDirent2->d_name);
                page_i++;
            }
        }
        closedir(sub_dir);
        website_map[dir_i] = page_i -1;
        dir_i++;
    }
}
closedir(serving_dir);
printf("Sites: %d   Pages: %d\n",total_websites,total_webpages);
int j = 0;
for(int i = 0; i < total_websites; i++){
    for(; j <= website_map[i]; j++){
        printf("site%d/%s\n",i,webpage_map[j]);
    }
}
*/
