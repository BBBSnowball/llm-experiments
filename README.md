```
nix-shell

cd llama.cpp
patch -p1 <../01-llama-cpp--print-graphs.patch
cmake -B build $CMAKE_ARGS
cmake --build build --config Release
cd ..

for x in gemma2:2b-instruct-fp16 gemma2:2b-instruct-q4_0 mistral-large:latest gemma2:latest mistral-nemo:latest mistral-large:123b-instruct-2407-fp16 ; do
    ollama pull $x
done

gguf-dump models/mistral-nemo--latest.gguf
sudo perf stat -a -e cycles,faults,node-loads,node-load-misses,offcore_requests.all_data_rd,fp_arith_inst_retired.scala
llama.cpp/build/bin/llama-cli -m models/mistral-nemo--latest.gguf -c 4096
# -> graph0.dot is different because that's for figuring out the size of some vectors. The others are the same.
```

https://www.omrimallis.com/posts/understanding-how-llm-inference-works-with-llama-cpp/
https://kipp.ly/transformer-param-count/

