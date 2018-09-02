#include "stdio.h"
#include "stdlib.h"
#include "trie.h"
#include "string.h"

/* Initializes a trie data structure */
int trieInit(Trie** trie){
    *trie = malloc(sizeof(Trie));
    (*trie)->head = NULL;
    (*trie)->wordCount = 0;
    (*trie)->lineCount = 0;
    (*trie)->keyCount = 0;
    return 0;
}

/* Inserts a text in trie and returns the number of words inserted */
int trieInsertLine(Trie* trie,char* line,int pid,int fid,int lnum,int loff){
    int wc = 0;
    const char delim[4] = " \t\n";
    char* token;
    char* str = malloc(strlen(line)+1);   // create duplicate so strtok doesnt mess with the original string
    strcpy(str,line);
    str[strlen(line)] = '\0';
    token = strtok(str, delim);
    while(token != NULL) {
        wc++;
        trieInsertWord(trie,token,pid,fid,lnum,loff);
        token = strtok(NULL, delim);
    }
    free(str);
    trie->lineCount++;
    return wc;
}

int initTrieNode(Trie* trie,TrieNode* tnode,char key){
    tnode= malloc(sizeof(TrieNode));
    tnode->key = key;
    trie->keyCount++;
    tnode->next = NULL;
    tnode->child = NULL;
    tnode->list = NULL;
}

/* Inserts a word in trie Returns 0 if successful*/    //TODO initTrieNode()
int trieInsertWord(Trie* trie,char* word,int pid,int fid,int lnum,int loff){
    int chIndex = 0;
    int flag = 0;
    //printf("\nInserting word : %s in id: %d\n",word,id);
    /* If trie is empty */
    if(trie->head == NULL){
        trie->head = malloc(sizeof(TrieNode));
        trie->head->key = word[chIndex];
        trie->keyCount++;
        trie->head->next = NULL;
        trie->head->child = NULL;
        trie->head->list = NULL;
        TrieNode* temp = trie->head;
        for(int j = 1; j<strlen(word);j++){ //allocate the remaining children
            temp->child = malloc(sizeof(TrieNode));
            temp->child->key = word[j];
            trie->keyCount++;
            temp->child->next = NULL;
            temp->child->child = NULL;
            temp->child->list = NULL;
            temp = temp->child;
        }
        /* If head = NULL there isn't any posting lists */
        postingListInit(&(temp->list));
        postingListInsert(temp->list,pid,fid,lnum,loff);
        trie->wordCount++;
        return 0;
    }
    else{
        TrieNode* current = trie->head;
        TrieNode* curalloc = trie->head;
        TrieNode* curprev = NULL;
        while((current != NULL) || (chIndex == strlen(word)+1)){
            flag = 0;
            if(current->key == word[chIndex]){
                chIndex++;
                curprev = current;
                curalloc = current;
                current = current->child;
                flag = 1;
                continue; // Continue with the next char
            }
            if(current->next == NULL){  // If there arent any chars left to check break and allocate
                curalloc = current;
                flag = 0;
                break;
            }
            else{
                current = current->next;
            }
        }
        if(chIndex == strlen(word)){
            if(curprev->list == NULL)
                postingListInit(&(curprev->list));
            postingListInsert(curprev->list,pid,fid,lnum,loff);
            trie->wordCount++;
            return 0;
        }
        if(flag == 1){
            curalloc->child = malloc(sizeof(TrieNode));
            curalloc->child->key = word[chIndex];
            trie->keyCount++;
            curalloc->child->next = NULL;
            curalloc->child->child = NULL;
            curalloc->child->list = NULL;
            chIndex++;
            current = curalloc->child;
            for(int i = chIndex; i<strlen(word);i++){
                current->child = malloc(sizeof(TrieNode));
                current->child->key = word[i];
                trie->keyCount++;
                current->child->next = NULL;
                current->child->child = NULL;
                current->child->list = NULL;
                current = current->child;
            }
            postingListInit(&(current->list));
            postingListInsert(current->list,pid,fid,lnum,loff);
            trie->wordCount++;
            return 0;
        }
        curalloc->next = malloc(sizeof(TrieNode));
        curalloc->next->key = word[chIndex];
        trie->keyCount++;
        curalloc->next->next = NULL;
        curalloc->next->child = NULL;
        curalloc->next->list = NULL;
        chIndex++;
        current = curalloc->next;
        for(int i = chIndex; i<strlen(word);i++){
            current->child = malloc(sizeof(TrieNode));
            current->child->key = word[i];
            trie->keyCount++;
            current->child->next = NULL;
            current->child->child = NULL;
            current->child->list = NULL;
            current = current->child;
        }
        postingListInit(&(current->list));
        postingListInsert(current->list,pid,fid,lnum,loff);
        trie->wordCount++;
        return 0;

    }
}

/* Searches a given word in trie Returns 0 if successful and populates argument plist */
int trieSearchWord(Trie* trie,char* word,PostingList** plist){
    //printf("Searching word: %s\n",word);
    TrieNode* current = trie->head;
    TrieNode* curprev = NULL;
    int chIndex = 0;
    while(current != NULL){
        if(current->key == word[chIndex]){
            chIndex++;
            curprev = current;
            current = current->child;
            continue; // Continue with the next char
        }
        if(current->next == NULL){
            break;
        }
        else{
            current = current->next;
        }
    }
    if(chIndex == strlen(word) && curprev->list != NULL){
        //printf("Word found Document freq : %d\n",curprev->list->totalfreq);

        *plist = curprev->list;
        return 0;
    }
    else{
        *plist = NULL;

        //printf("Word not found!\n");
        return -1;
    }
}

int findMaxKeyword(Trie* trie,char* keyword,int* pathid,int* fileid){
    PostingList* plist = NULL;
    trieSearchWord(trie,keyword,&plist);
    if(plist == NULL)
        return -1;
    PostingListNode* current = plist->head;
    int maxall = 0;
    int maxpid = -1;
    int maxfid = -1;
    while(current != NULL){
        ListNode* lcur = current->lhead;
        int max = 0;
        while(lcur != NULL){
            max += lcur->linefreq;
            lcur = lcur->next;
        }
        if(max > maxall){
            maxall = max;
            maxpid = current->pathid;
            maxfid = current->fileid;
        }
        current = current->next;
    }
    *pathid = maxpid;
    *fileid = maxfid;
    if(maxall == 0)
        return maxall -1;
    else return maxall;
}

int findMinKeyword(Trie* trie,char* keyword,int* pathid,int* fileid){
    PostingList* plist = NULL;
    trieSearchWord(trie,keyword,&plist);
    if(plist == NULL)
        return -1;
    PostingListNode* current = plist->head;
    int minall = 0;
    ListNode* lcur0 = current->lhead;
    while(lcur0 != NULL){
        minall += lcur0->linefreq;
        lcur0 = lcur0->next;
    }
    int minpid = current->pathid;
    int minfid = current->fileid;
    if(current->next != NULL)
        current = current->next;
    else{
        *pathid = minpid;
        *fileid = minfid;
        if(minall == 0)
            return minall -1;
        else return minall;
    }

    while(current != NULL){
        ListNode* lcur = current->lhead;
        int min = 0;
        while(lcur != NULL){
            min += lcur->linefreq;
            lcur = lcur->next;
        }
        if(min < minall){
            minall = min;
            minpid = current->pathid;
            minfid = current->fileid;
        }
        current = current->next;
    }
    *pathid = minpid;
    *fileid = minfid;
    if(minall == 0)
        return minall -1;
    else return minall;
}


/* Traverses a trie and prints all the words */
int trieTraverse(TrieNode* tnode,char* prefix){
    int length = strlen(prefix);
    char newprefix[length+1];
    strcpy(newprefix,prefix);

    if(tnode->child != NULL){
        newprefix[length] = tnode->key;
        newprefix[length+1] = '\0';
        trieTraverse(tnode->child,newprefix);
    }
    if(tnode->next != NULL){
        trieTraverse(tnode->next,prefix);
    }

    if(tnode->list != NULL){
        newprefix[length] = tnode->key;
        newprefix[length+1] = '\0';
        printf("%s %d\n",newprefix,tnode->list->totalfreq);
    }
    return 0;
}

/* Deletes a trieNode, if called from root deletes the trie */
int trieDelete(TrieNode* tnode){
    if(tnode->child != NULL){
        trieDelete(tnode->child);
        free(tnode->child);
    }
    if(tnode->next != NULL){
        trieDelete(tnode->next);
        free(tnode->next);
    }
    if(tnode->list != NULL){
        postingListDelete(tnode->list);
        free(tnode->list);
    }
    return 0;
}

/* Initializes Posting List */
int postingListInit(PostingList** plist){
    *plist = malloc(sizeof(PostingList));
    (*plist)->totalfreq = 0;
    (*plist)->nodesnum = 0;
    (*plist)->head = NULL;
    return 0;
}

/* Inserts or updates a posting list */
int postingListInsert(PostingList* plist,int pid,int fid,int lnum,int loff){
    if(plist->head == NULL){
        plist->head = malloc(sizeof(PostingListNode));
        plist->nodesnum++;
        plist->head->pathid = pid;
        plist->head->fileid = fid;
        plist->head->lhead = malloc(sizeof(ListNode));
        plist->head->lhead->linenum = lnum;
        plist->head->lhead->linefreq = 1;
        plist->head->lhead->lineoffset = loff;
        plist->head->lhead->next = NULL;
        plist->head->next = NULL;
        plist->totalfreq = 1;
        return 0;
    }
    PostingListNode* current = plist->head;
    PostingListNode* curprev = plist->head;
    while(current != NULL){
        if(current->pathid == pid && current->fileid == fid){
            ListNode* cur = current->lhead;
            ListNode* cprev = current->lhead;
            while(cur != NULL){
                if(cur->linenum == lnum){
                    cur->linefreq++;
                    plist->totalfreq++;
                    return 0;
                }
                cprev = cur;
                cur = cur->next;
            }
            cprev->next = malloc(sizeof(ListNode));
            cprev->next->linenum = lnum;
            cprev->next->linefreq = 1;
            cprev->next->lineoffset = loff;
            cprev->next->next = NULL;
            plist->totalfreq++;
            return 0;
        }
        curprev = current;
        current = current->next;
    }
    current = curprev;
    current->next = malloc(sizeof(PostingListNode));
    plist->nodesnum++;
    current->next->pathid = pid;
    current->next->fileid = fid;
    current->next->lhead = malloc(sizeof(ListNode));
    current->next->lhead->linenum == lnum;
    current->next->lhead->linefreq = 1;
    current->next->lhead->lineoffset = loff;
    current->next->lhead->next = NULL;
    current->next->next = NULL;
    plist->totalfreq++;
    return 0;
}

/* Deletes a posting list */
int postingListDelete(PostingList* plist){
    PostingListNode* current = plist->head;
    PostingListNode* temp;
    ListNode* lnext;
    ListNode* ltemp;
    while(current != NULL){
        temp = current->next;
        lnext = current->lhead;
        while(lnext != NULL){
            ltemp = lnext->next;
            free(lnext);
            lnext = ltemp;
        }
        free(current);
        current = temp;
    }
    return 0;
}
