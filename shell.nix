# copied from https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/python.section.md#how-to-consume-python-modules-using-pip-in-a-virtual-environment-like-i-am-used-to-on-other-operating-systems-how-to-consume-python-modules-using-pip-in-a-virtual-environment-like-i-am-used-to-on-other-operating-systems
with import <nixpkgs> { };

let
  pythonPackages = python3Packages;
in pkgs.mkShell rec {
  name = "impurePythonEnv";
  venvDir = "./.venv";
  buildInputs = [
    pythonPackages.python

    # This executes some shell code to initialize a venv in $venvDir before
    # dropping into the shell
    pythonPackages.venvShellHook

    # Those are dependencies that we would like to use from nixpkgs, which will
    # add them to PYTHONPATH and thus make them accessible from within the venv.
    pythonPackages.numpy
    pythonPackages.requests
    pythonPackages.sentencepiece

    # In this particular example, in order to compile any binary extensions they may
    # require, the Python modules listed in the hypothetical requirements.txt need
    # the following packages to be installed locally:
    #taglib
    #openssl
    #git
    #libxml2
    #libxslt
    #libzip
    #zlib
  ];

  # Run this command, only after creating the virtual environment
  postVenvCreation = ''
    unset SOURCE_DATE_EPOCH
    pip install -r requirements.txt
  '';

  # Now we can execute any commands within the virtual environment.
  # This is optional and can be left out to run pip manually.
  postShellHook = ''
    # allow pip to install wheels
    unset SOURCE_DATE_EPOCH

    # see https://github.com/abetlen/llama-cpp-python
    export CMAKE_ARGS="-DGGML_BLAS=ON -DGGML_BLAS_VENDOR=OpenBLAS -DGGML_F16C=ON -DGGML_AVX=ON -DGGML_AVX2=ON -DGGML_CCACHE=OFF"
    #export CMAKE_ARGS="-DGGML_BLAS=ON -DGGML_BLAS_VENDOR=Intel10_64lp -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icpx -DGGML_NATIVE=ON"

    # We have to replace `-march=native` by `-march=icelake-client` to avoid this error:
    #   https://github.com/ggerganov/llama.cpp/issues/107
    # -> or allow using the native CPU: https://discourse.nixos.org/t/gcc-march-native-work-incorrectly-in-devshell/33154/2
    #export NIX_ENFORCE_NO_NATIVE=0
    export NIX_CFLAGS_COMPILE="$NIX_CFLAGS_COMPILE -march=icelake-client"
  '';

  # Other packages that we want to use in our shell.
  packages = [
    linuxPackages.perf  # needs root
    stress-ng
    bandwidth
    jq
    #NOTE gguf-dump is available from gguf package in venv.
    llama-cpp
    cmake
    gnumake
    openblas
    pkg-config
    sycl-info
    opensycl
    #mkl
    gdb
  ];
}
