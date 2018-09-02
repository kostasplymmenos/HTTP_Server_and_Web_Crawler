
jobExecutor: main.o jobExecutor.o worker.o trie.o
	@echo "Compiling jobExecutor..."
	gcc -o ./build/jobExecutor ./bin/main.o ./bin/jobExecutor.o ./bin/worker.o ./bin/trie.o
	@echo "Done. Executable is ./build/jobExecutor"

main.o: ./src/main.c
	gcc -I ./include/ -c ./src/main.c -o ./bin/main.o

jobExecutor.o: ./src/jobExecutor.c
	gcc -I ./include/ -c ./src/jobExecutor.c -o ./bin/jobExecutor.o

worker.o: ./src/worker.c
	gcc -I ./include/ -c ./src/worker.c -o ./bin/worker.o

trie.o: ./src/trie.c
	gcc -I ./include/ -c ./src/trie.c -o ./bin/trie.o

clean:
	rm ./build/jobExecutor ./bin/*.o je_* ./log/Worker_*
