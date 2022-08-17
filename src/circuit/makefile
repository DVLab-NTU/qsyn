CC=g++
LDFLAGS=-std=c++11 -O3 -lm
SOURCES=main.cpp
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=cir
INCLUDES=

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o:  %.c  ${INCLUDES}
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)