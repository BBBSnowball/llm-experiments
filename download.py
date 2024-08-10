# https://huggingface.co/mistralai/Mistral-Large-Instruct-2407
from huggingface_hub import snapshot_download
from pathlib import Path

mistral_models_path = Path.home().joinpath('mistral_models', 'Large')
mistral_models_path.mkdir(parents=True, exist_ok=True)

snapshot_download(repo_id="mistralai/Mistral-Large-Instruct-2407", allow_patterns=["params.json", "consolidated-*.safetensors", "tokenizer.model.v3"], local_dir=mistral_models_path)

