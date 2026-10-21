# Create and navigate to the build directory
mkdir build && cd build

# 1. Configure the project (generates Makefiles)
cmake ..

# 2. Build the project (calls make internally)
cmake --build .

# 3. Execute the resulting binary
./demo
