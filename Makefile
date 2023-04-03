CC=g++
CFLAGS=-c -std=c++11

TARGET=init

SRCS=init.cpp init_dir.cpp

OBJS=$(SRCS:.cpp=.o)

DEPS=dinode.h sb.h 

all: $(TARGET)
	rm -f *.o

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f *.img
