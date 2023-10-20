CC=gcc
OBJ=winconfig
OBJS=upgrade.o xmodem.o serial_upgrade_example.o 
$(OBJ):$(OBJS)
	$(CC) -o $@ $^
%.o:%.c
	$(CC) -g -c $^


clean:
	rm *.o $(OBJ)
