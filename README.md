# Systems Programming Course Project - NKUA 2018 

This project includes:
- A multithreaded HTTP Server written in C from scratch, using unix system calls and socket library. Implements only the GET HTTP operation.

- A web creator script that creates a connected graph of webpages with random links to each other and mock content.

- A multithreaded Web Crawler App that searches the connected graph of web pages and downloads them through our own http server.

- Each web crawler worker is integrated with a previous project that takes some text files (web pages) and stores their data to it's own trie data structure.

- Workers expose telnet ports that a user can connect to and query the data of the downloaded web pages very fast. Data is organised in a trie data structure and a BM25 search engine is implemented 

