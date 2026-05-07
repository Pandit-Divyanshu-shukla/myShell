CC = gcc
CFLAGS = -Wall

OBJS = shell.o input.o parser.o background.o builtin.o execute.o pipe.o redirection.o history.o 

shell: $(OBJS)
	$(CC) $(CFLAGS) -o shell $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o shell