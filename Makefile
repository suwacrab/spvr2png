# compiler ------------------------------------------------------------------@/
CXX	:= clang++
CC	:= clang

CBASEFLAGS := -Wall -Wshadow -Iinclude
CFLAGS	 := $(CBASEFLAGS)
CXXFLAGS := $(CBASEFLAGS)
LDFLAGS  := -static

# output --------------------------------------------------------------------@/
OBJ_DIR := build
SRC_DIR := source
OUTPUT  := bin/spvr2png.exe

SRCS_C   := $(shell find $(SRC_DIR) -name *.c)
SRCS_CPP := $(shell find $(SRC_DIR) -name *.cpp)

OBJS := $(subst $(SRC_DIR),$(OBJ_DIR),$(SRCS_C:.c=.o))
OBJS += $(subst $(SRC_DIR),$(OBJ_DIR),$(SRCS_CPP:.cpp=.o))

# building ------------------------------------------------------------------@/
all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

clean:
	rm -rf $(OBJS) $(OUTPUT)

