.PHONY: run-testperf-allcpu

run-testperf-allcpu: build/testperf-allcpu
	LD_LIBRARY_PATH=./llama.cpp/build/ggml/src $<

build/testperf-allcpu: testperf-allcpu.c ./llama.cpp/build/ggml/src/libggml.so build
	gcc -o $@ testperf-allcpu.c -L ./llama.cpp/build/ggml/src/ -l ggml

./llama.cpp/build/ggml/src/libggml.so:
	cd llama.cpp && cmake -B build $CMAKE_ARGS && cmake --build build --config Release

build:
	mkdir -p build

build/measure_perf.o: measure_perf.c measure_perf.h
	gcc -c -g -o $@ $<

build/measure_perf_test: measure_perf_test.c build/measure_perf.o
	gcc -g -o $@ $< build/measure_perf.o

.PHONY: measure_perf_test
measure_perf_test: build/measure_perf_test
	build/measure_perf_test

