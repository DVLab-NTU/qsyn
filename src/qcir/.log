CC=g++
LDFLAGS=-std=c++11 -g -lm
SOURCES=qcirGate.cpp qcirMgr.cpp main.cpp
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=cir
INCLUDES=qcirGate.h qcirMgr.h

# all: $(SOURCES) $(EXECUTABLE)

cir: main.o qcirGate.o qcirMgr.o
	g++ -o $@ main.o qcirGate.o qcirMgr.o $(LDFLAGS)
# $(EXECUTABLE): $(OBJECTS)
# 	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

main.o: main.cpp qcirMgr.cpp
	g++ -c main.cpp $(LDFLAGS)

qcirGate.o: qcirGate.cpp qcirGate.h
	g++ -c qcirGate.cpp $(LDFLAGS)

qcirMgr.o: qcirMgr.cpp qcirMgr.h
	g++ -c qcirMgr.cpp $(LDFLAGS)

clean:
	rm -rf *.o $(EXECUTABLE)