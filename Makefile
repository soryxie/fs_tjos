CC=g++
CFLAGS=-c -std=c++17 -g

SRC_DIR=src
TARGET=init server client

INIT_SRCS=$(SRC_DIR)/init.cpp 
CLIENT_SRCS=$(SRC_DIR)/client.cpp 
SERVER_SRCS=$(SRC_DIR)/server.cpp $(SRC_DIR)/fs.cpp $(SRC_DIR)/directory.cpp $(SRC_DIR)/inode.cpp $(SRC_DIR)/BlockCache.cpp

INIT_OBJS=$(INIT_SRCS:.cpp=.o)
CLIENT_OBJS=$(CLIENT_SRCS:.cpp=.o)
SERVER_OBJS=$(SERVER_SRCS:.cpp=.o)

DEPS=$(wildcard include/*.h)

all: $(TARGET)

init: $(INIT_OBJS)
	$(CC) $(INIT_OBJS) -o init
	rm -f ./src/*.o

client: $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o client
	rm -f ./src/*.o

server: $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o server
	rm -f ./src/*.o	

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f *.img
	rm -f ./src/*.o
