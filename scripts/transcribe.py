import sys, os, json
try:
    from faster_whisper import WhisperModel
except ImportError:
    print("ERRO: faster-whisper nao instalado. pip install faster-whisper")
    sys.exit(1)

if len(sys.argv) < 2:
    print("ERRO: Informe o caminho do audio")
    sys.exit(1)

audio = sys.argv[1]
if not os.path.isfile(audio):
    print(f"ERRO: Arquivo nao encontrado: {audio}")
    sys.exit(1)

try:
    model = WhisperModel("base", device="cpu", compute_type="int8")
    segments, info = model.transcribe(audio, beam_size=5)
    result = {"language": info.language, "segments": []}
    for seg in segments:
        result["segments"].append({"start": seg.start, "end": seg.end, "text": seg.text.strip()})
    print(json.dumps(result, ensure_ascii=False))
except Exception as e:
    print(f"ERRO: {e}")
    sys.exit(1)
