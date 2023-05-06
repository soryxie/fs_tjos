CC=g++
CFLAGS=-c -std=c++11 -g

SRC_DIR=src
TARGET=init client

INIT_SRCS=$(SRC_DIR)/init.cpp 
CLIENT_SRCS=$(SRC_DIR)/client.cpp $(SRC_DIR)/login.cpp $(SRC_DIR)/fs.cpp

INIT_OBJS=$(INIT_SRCS:.cpp=.o)
CLIENT_OBJS=$(CLIENT_SRCS:.cpp=.o)

DEPS=$(wildcard include/*.h)

all: $(TARGET)

init: $(INIT_OBJS)
	$(CC) $(INIT_OBJS) -o init
	rm -f ./src/*.o

client: $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o client
	rm -f ./src/*.o	

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f *.img
	rm -f ./src/*.o
