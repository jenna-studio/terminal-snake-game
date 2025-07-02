# Makefile for Snake Game (macOS)
# 컴파일: make
# 실행: make run
# 삭제: make clean

CXX = clang++
CXXFLAGS = -std=c++17 -g -Wall
LDFLAGS = -lncurses

TARGET = snake_game
SRC = snake_game.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
