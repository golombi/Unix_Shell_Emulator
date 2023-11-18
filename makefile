all: myshell looper

myshell: myshell.c LineParser.c
	gcc -o myshell myshell.c LineParser.c

looper: looper.c
	gcc -o looper looper.c
