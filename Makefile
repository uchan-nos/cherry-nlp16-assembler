TARGET = nlpasm
OBJS   = main.o
CFLAGS = -Wall -Wextra -g

all: $(TARGET)

$(TARGET): $(OBJS) Makefile
	$(CC) -o $@ $(OBJS)
