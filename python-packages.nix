# Generated by pip2nix 0.8.0.dev1
# See https://github.com/nix-community/pip2nix

{ pkgs, fetchurl, fetchgit, fetchhg }:

self: super: {
  "certifi" = super.buildPythonPackage rec {
    pname = "certifi";
    version = "2024.7.4";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/1c/d5/c84e1a17bf61d4df64ca866a1c9a913874b4e9bdc131ec689a0ad013fb36/certifi-2024.7.4-py3-none-any.whl";
      sha256 = "140y22hj8bv2bf6im42r424x1spk9r5vnrsfxs2sphl928dy5661";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "charset-normalizer" = super.buildPythonPackage rec {
    pname = "charset-normalizer";
    version = "3.3.2";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/63/09/c1bc53dab74b1816a00d8d030de5bf98f724c52c1635e07681d312f20be8/charset-normalizer-3.3.2.tar.gz";
      sha256 = "1m9g0f513ng4dp2vd3smi4g2nmhqkjqh3bzcza14li947frkq37k";
    };
    format = "setuptools";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "filelock" = super.buildPythonPackage rec {
    pname = "filelock";
    version = "3.15.4";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/ae/f0/48285f0262fe47103a4a45972ed2f9b93e4c80b8fd609fa98da78b2a5706/filelock-3.15.4-py3-none-any.whl";
      sha256 = "1rv8wi6m0845l0alx9627s6mdllcybsc9wgaqssdl9b2x7xgz8bc";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "fsspec" = super.buildPythonPackage rec {
    pname = "fsspec";
    version = "2024.6.1";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/5e/44/73bea497ac69bafde2ee4269292fa3b41f1198f4bb7bbaaabde30ad29d4a/fsspec-2024.6.1-py3-none-any.whl";
      sha256 = "07hhxrn9x5fhlsbqzll05i2xi0gf5aqgvfd5jl9b7vyjpkw47d1w";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "huggingface-hub" = super.buildPythonPackage rec {
    pname = "huggingface-hub";
    version = "0.24.5";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/0b/05/31b21998f68c31e7ffcc27ff08531fb9af5506d765ce8d661fb0036e6918/huggingface_hub-0.24.5-py3-none-any.whl";
      sha256 = "1vdcsgl3qd3jfpz4sd5z213gr0afjwc4b8cirqi9m48s3wxvcgyr";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [
      self."filelock"
      self."fsspec"
      self."packaging"
      self."pyyaml"
      self."requests"
      self."tqdm"
      self."typing-extensions"
    ];
  };
  "idna" = super.buildPythonPackage rec {
    pname = "idna";
    version = "3.7";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/e5/3e/741d8c82801c347547f8a2a06aa57dbb1992be9e948df2ea0eda2c8b79e8/idna-3.7-py3-none-any.whl";
      sha256 = "180sfq3qsycfxn1zc9w4gp4lr44adpx8p2d1sf939m5dg3yf3zl2";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "mistral-inference" = super.buildPythonPackage rec {
    pname = "mistral-inference";
    version = "0.0.0";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/64/52/052cfcf3e9ebf55a74a9b92b99208068fa3363492b94df5d1019f2656dfc/mistral_inference-0.0.0-py3-none-any.whl";
      sha256 = "1ja4j523ghnd0qkzp87n5s19j708dn75lbb9knj98gabfclqxmkz";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "packaging" = super.buildPythonPackage rec {
    pname = "packaging";
    version = "24.1";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/08/aa/cc0199a5f0ad350994d660967a8efb233fe0416e4639146c089643407ce6/packaging-24.1-py3-none-any.whl";
      sha256 = "097igwfmvak1xb1x1ijk1wnjzmg68j2n4764hkrzglnvvcbj53sv";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "pyyaml" = super.buildPythonPackage rec {
    pname = "pyyaml";
    version = "6.0.2";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/54/ed/79a089b6be93607fa5cdaedf301d7dfb23af5f25c398d5ead2525b063e17/pyyaml-6.0.2.tar.gz";
      sha256 = "0gmwggzm0j0iprx074g5hah91y2f68sfhhldq0f8crddj7ndk16m";
    };
    format = "setuptools";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "requests" = super.buildPythonPackage rec {
    pname = "requests";
    version = "2.32.3";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/f9/9b/335f9764261e915ed497fcdeb11df5dfd6f7bf257d4a6a2a686d80da4d54/requests-2.32.3-py3-none-any.whl";
      sha256 = "1inwsrhx0m16q0wa1z6dfm8i8xkrfns73xm25arcwwy70gz1qxkh";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [
      self."certifi"
      self."charset-normalizer"
      self."idna"
      self."urllib3"
    ];
  };
  "tqdm" = super.buildPythonPackage rec {
    pname = "tqdm";
    version = "4.66.5";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/48/5d/acf5905c36149bbaec41ccf7f2b68814647347b72075ac0b1fe3022fdc73/tqdm-4.66.5-py3-none-any.whl";
      sha256 = "1kf7ba00wnyqz6rhx8ssj88j302r462n80sa374sygkmf0vrl9wh";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "typing-extensions" = super.buildPythonPackage rec {
    pname = "typing-extensions";
    version = "4.12.2";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/26/9f/ad63fc0248c5379346306f8668cda6e2e2e9c95e01216d2b8ffd9ff037d0/typing_extensions-4.12.2-py3-none-any.whl";
      sha256 = "03bhjivpvdhn4c3x0963z89hv7b5vxr415akd1fgiwz0a41wmr84";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
  "urllib3" = super.buildPythonPackage rec {
    pname = "urllib3";
    version = "2.2.2";
    src = fetchurl {
      url = "https://files.pythonhosted.org/packages/ca/1c/89ffc63a9605b583d5df2be791a27bc1a42b7c32bab68d3c8f2f73a98cd4/urllib3-2.2.2-py3-none-any.whl";
      sha256 = "0wil7vvi3lw00ihlc2kzw5v9f6fjyblsrq9ph135aqb89pvb4j54";
    };
    format = "wheel";
    doCheck = false;
    buildInputs = [];
    checkInputs = [];
    nativeBuildInputs = [];
    propagatedBuildInputs = [];
  };
}
