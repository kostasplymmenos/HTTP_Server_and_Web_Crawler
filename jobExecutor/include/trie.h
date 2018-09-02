#ifndef TRIE_H
#define TRIE_H

typedef struct listNode{
    int linenum;
    int linefreq;
    int lineoffset;
    struct listNode* next;
}ListNode;

typedef struct postNode{
    int pathid;
    int fileid;
    ListNode* lhead;
    struct postNode* next;
}PostingListNode;

typedef struct postingList{
    int totalfreq;
    int nodesnum;
    PostingListNode* head;
}PostingList;

typedef struct trieNode{
    char key;
    struct trieNode* next;
    struct trieNode* child;
    PostingList* list;
}TrieNode;

typedef struct trie{
    TrieNode* head;
    int keyCount;
    int wordCount;
    int lineCount;
}Trie;


int trieInit(Trie** trie);
int initTrieNode(Trie* trie,TrieNode* tnode,char key);
int trieInsertWord(Trie* trie,char* word,int pid,int fid,int lnum,int loff);
int trieInsertLine(Trie* trie,char* line,int pid,int fid,int lnum,int loff);
int trieDelete(TrieNode* head);
int trieSearchWord(Trie* trie,char* word,PostingList** plist);
int trieTraverse(TrieNode* tnode,char*);

int findMaxKeyword(Trie*,char*,int*,int*);
int findMinKeyword(Trie*,char*,int*,int*);

int postingListInit(PostingList** pl);
int postingListInsert(PostingList* pl,int pid,int fid,int lnum,int loff);
int postingListDelete(PostingList* pl);


#endif
