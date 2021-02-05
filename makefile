CC= gcc
CFLAGS= -g -Wall
TARGET= mycli
OBJS= mycli.o modules/tokenizer.o modules/rcreader.o modules/executor.o modules/internal.o

all: $(TARGET)

mycli: $(OBJS)
	$(CC) $(CFLAGS) -o mycli $(OBJS)

run: $(TARGET)
	./mycli

clean:
	rm -f *.o modules/*.o $(TARGET)
