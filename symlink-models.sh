#find ollama/models/manifests/registry.ollama.ai/library/ -type f -exec jq -r '.layers[] | select(.mediaType=="application/vnd.ollama.image.model") | .digest | sub(":";"-")' {} \;
set -eo pipefail
mkdir -p models
find ollama/models/manifests/registry.ollama.ai/library/ -type f | while read f ; do
  blob="$(jq -r '.layers[] | select(.mediaType=="application/vnd.ollama.image.model") | .digest | sub(":";"-")' <"$f")"
  name="$(basename $(dirname $f))--$(basename $f).gguf"
  echo "$name:"
  ls -lh ollama/models/blobs/$blob
  ln -sfT ../ollama/models/blobs/$blob models/"$name"
done
