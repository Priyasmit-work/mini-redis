CC= gcc
CFLAGS= -Wall -Wextra -g
TARGET=Redis_clone
SRC=src/main.c

$(TARGET) : $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)
	
clean:
	rm -f $(TARGET)




