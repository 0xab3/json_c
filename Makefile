build:
	gcc -c -Wall -Wextra -ggdb main.c json.c
	gcc  -ggdb -o main main.o json.o

