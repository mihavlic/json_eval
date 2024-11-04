# json-eval

## Building

This was being built on linux by gcc, C++20 is required. 

Dependencies: gcc, cmake, make

To run as a cli as shown in the assignment
```sh
cmake -B build
make -C build
./build/src/json_eval
```

## Testing

The tests are very crude, I apologize I was in a rush.

To run tests, build the binary in debug.
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -DTEST=1
./build/src/json_eval
```
Then run the testing script `tests/test.sh`. And visually inspect the results.
