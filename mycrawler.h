#ifndef _MYCRAWLER_
#define _MYCRAWLER_


typedef struct urllistnode{
    char* url;
    struct urllistnode *next;
}UrlListNode;

typedef struct urllist{
    UrlListNode *head;
    UrlListNode *end;
    int size;
}UrlList;

void crawlerEngine(char*,char*,char*,int,int,int);
int analyzeWebpageLinks(UrlList*,char*,int);
int addUrltoList(UrlList*,char*);
int downloadWebpage(int,UrlList*);
void *threadExecuteTask();
int writeFileToDisk(char*,char*,char*,int);

#endif
