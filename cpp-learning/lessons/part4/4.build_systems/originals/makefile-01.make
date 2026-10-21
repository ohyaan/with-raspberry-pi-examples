# Define variables
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

# The default target
all: my_program

# Rule to link the final executable
my_program: main.o utils.o
	$(CXX) $(CXXFLAGS) -o my_program main.o utils.o

# Rule to compile main.cpp to object file
main.o: main.cpp utils.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

# Rule to compile utils.o
utils.o: utils.cpp utils.hpp
	$(CXX) $(CXXFLAGS) -c utils.cpp

# Cleanup rule
clean:
	rm -f *.o my_program
