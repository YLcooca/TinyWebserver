CXX=g++
CFLAGS=-std=c++17 -O0 -Wall -g

TARGET=server
OBJS=../code/log/*.cpp     \
		 ../code/pool/*pp        \
		 ../code/utility/*.cpp \
		 ../code/http/*.cpp    \
		 ../code/server/epoller.cpp  \
		 ../code/server/webserver.cpp \
		 ../code/buffer/*.cpp  \
		 ../code/main.cpp
all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET) -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)
