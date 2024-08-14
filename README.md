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

sudo mount --bind /var/lib/ollama /home/user2/mistral/ollama

sudo -i
echo 1 > /sys/bus/event_source/devices/cpu/rdpmc
echo 2 > /proc/sys/kernel/perf_event_paranoid
exit
sudo perf stat -a -e cycles,faults,node-loads,node-load-misses,offcore_requests.all_data_rd,fp_arith_inst_retired.scala

gguf-dump models/mistral-nemo--latest.gguf
llama.cpp/build/bin/llama-cli -m models/mistral-nemo--latest.gguf -c 4096
# -> graph0.dot is different because that's for figuring out the size of some vectors. The others are the same.
```

https://www.omrimallis.com/posts/understanding-how-llm-inference-works-with-llama-cpp/
https://kipp.ly/transformer-param-count/
https://www.theregister.com/2024/07/14/quantization_llm_feature/

For mistral-nemo:
```
llama_kv_cache_init:        CPU KV buffer size =   640.00 MiB
llama_new_context_with_model: KV self size  =  640.00 MiB, K (f16):  320.00 MiB, V (f16):  320.00 MiB
llama_new_context_with_model:        CPU  output buffer size =     0.50 MiB
llama_new_context_with_model:        CPU compute buffer size =   300.01 MiB

Size of temporary storage is up to 192512 bytes. This is without KV cache, which is usually much larger!
Total cost:
  read: 6.232 GiB
  fmul: 1.15893e+10
  fdiv: 573521
  sqrt: 81
  fsin: 204800
  write: 0.001 GiB
  fpow: 1280
  fadd: 1.67936e+06
  fexp: 614400
```

For mistral-large:

```
llama_kv_cache_init:        CPU KV buffer size = 11264.00 MiB
llama_new_context_with_model: KV self size  = 11264.00 MiB, K (f16): 5632.00 MiB, V (f16): 5632.00 MiB
llama_new_context_with_model:        CPU  output buffer size =     0.13 MiB
llama_new_context_with_model:        CPU compute buffer size =  6304.01 MiB

Size of temporary storage is up to 393216 bytes. This is without KV cache, which is usually much larger!
Total cost:
  read: 64.135 GiB
  fmul: 1.22288e+11
  fdiv: 2.52331e+06
  sqrt: 177
  fsin: 1.17146e+06
  write: 0.003 GiB
  fpow: 8448
  fadd: 8.01997e+06
  fexp: 2.79347e+06
```

