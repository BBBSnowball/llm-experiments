.PHONY: run-testperf-allcpu

run-testperf-allcpu: build/testperf-allcpu
	LD_LIBRARY_PATH=./llama.cpp/build/ggml/src $<

build/testperf-allcpu: testperf-allcpu.c ./llama.cpp/build/ggml/src/libggml.so
	mkdir -p build
	gcc -o $@ testperf-allcpu.c -L ./llama.cpp/build/ggml/src/ -l ggml

./llama.cpp/build/ggml/src/libggml.so:
	cd llama.cpp && cmake -B build $CMAKE_ARGS && cmake --build build --config Release

