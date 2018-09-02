all: http_server web_crawler

http_server: myhttpd_main.o myhttp_server.o
	@echo "Compiling HttpServer..."
	gcc -o myhttpd myhttpd_main.o myhttp_server.o -lpthread
	@echo "Done. Executable is ./myhttp"
	@echo "Usage ./myhttpd -p serving_port -c command_port -t threads_number -d serving_dir\n"

web_crawler: web_crawler_main.o mycrawler.o
	@echo "Compiling Web Crawler..."
	gcc -o mycrawler web_crawler_main.o mycrawler.o -lpthread
	@echo "Done. Executable is ./mycrawler"
	@echo "Usage ./mycrawler -h host -p server_port -c command_port -t threads_number -d saving_dir starting_url\n"
	@echo "Compiling JobExecutor used by web crawler..."
	cd jobExecutor && $(MAKE)

myhttpd_main.o: ./myhttpd_main.c
	gcc -c ./myhttpd_main.c

myhttpd_server.o: ./myhttpd_server.c
	gcc -c ./myhttpd_server.c
	
web_crawler_main.o: ./web_crawler_main.c
	gcc -c ./web_crawler_main.c

mycrawler.o: ./mycrawler.c
	gcc -c ./mycrawler.c

clean:
	rm *.o ./mycrawler ./myhttpd ./jobExecutor/je_fifo* _crawler_je*
