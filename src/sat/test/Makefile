satTest: clean File.o Proof.o Solver.o satTest.o
	g++ -o $@ -std=c++11 -g File.o Proof.o Solver.o satTest.o

File.o: File.cpp
	g++ -c -std=c++11 -g File.cpp

Proof.o: Proof.cpp
	g++ -c -std=c++11 -g Proof.cpp

Solve.o: Solver.cpp
	g++ -c -std=c++11 -g Solver.cpp

satTest.o: satTest.cpp
	g++ -c -std=c++11 -g satTest.cpp

clean:
	rm -f *.o satTest tags
