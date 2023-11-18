myshell: main.o LineParser.o
	gcc -m32 -g -Wall main.o LineParser.o -o myshell
main.o: main.c
	gcc -m32 -g -Wall -c main.c -o main.o
LineParser.o: LineParser.c
	gcc -m32 -g -Wall -c LineParser.c -o LineParser.o
clean:
	rm myshell *.o
looper: looper.c
	gcc -m32 -g -Wall looper.c -o looper
clooper:
	rm looper
