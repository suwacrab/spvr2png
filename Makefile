CC := clang
CXX := clang++

CWARN := -Wall -Wshadow
CBASEFLAGS := $(CWARN) -g
CXXFLAGS := $(CBASEFLAGS) -std=c++20
CFLAGS := $(CBASEFLAGS)
LDFLAGS =

SOURCES_CPP = main.cpp
SOURCES_C = SegaPVRImage.c
OBJECTS := $(SOURCES_CPP:.cpp=.o)
OBJECTS += $(SOURCES_C:.c=.o)
EXECUTABLE = spvr2png

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
