nix-build pip2nix/release.nix -A pip2nix.python39 -o pip2nix-result
./pip2nix-result/bin/pip2nix generate -r requirements.txt
