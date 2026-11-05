import sys, os
audio = sys.argv[1] if len(sys.argv) > 1 else ""
out = sys.argv[2] if len(sys.argv) > 2 else ""
if not audio or not os.path.isfile(audio):
    msg = "ERRO: Arquivo de audio nao encontrado"
    if out: open(out,"w",encoding="utf-8").write(msg)
    else: print(msg)
    sys.exit(1)
try:
    import speech_recognition as sr
    r = sr.Recognizer()
    with sr.AudioFile(audio) as src:
        data = r.record(src)
    texto = r.recognize_google(data, language="pt-BR")
except ImportError:
    texto = "ERRO: speech_recognition nao instalado (pip install SpeechRecognition)"
except Exception as e:
    texto = f"ERRO na transcricao: {e}"
if out:
    with open(out, "w", encoding="utf-8") as f: f.write(texto)
else:
    print(texto)
