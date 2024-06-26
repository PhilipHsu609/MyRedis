CMAKE_FLAGS=-DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug
CMAKE_COMPILER_FLAGS=-DCMAKE_CXX_COMPILER:STRING=clang++ -DCMAKE_C_COMPILER:STRING=clang

.PHONY: all config build clean test

all: config build test

config:
	@cmake ${CMAKE_FLAGS} ${CMAKE_COMPILER_FLAGS} -S . -B build

build: config
	@cmake --build build

test:
	@cmake ${CMAKE_FLAGS} ${CMAKE_COMPILER_FLAGS} -S test -B build/test
	@cmake --build build/test
	@cd build/test && ctest

clean:
	@rm -rf bin
	@rm -rf build