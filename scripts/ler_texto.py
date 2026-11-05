import sys, os
out_mp3 = ""
texto = ""
if len(sys.argv) > 1:
    arg = sys.argv[1]
    if arg.startswith("@"):
        fpath = arg[1:]
        if os.path.isfile(fpath):
            with open(fpath, "r", encoding="utf-8") as f: texto = f.read()
        else:
            msg = f"ERRO: arquivo nao encontrado: {fpath}"
            if len(sys.argv) > 2:
                out_mp3 = sys.argv[2]
                open(out_mp3,"w",encoding="utf-8").write(msg)
            else: print(msg)
            sys.exit(1)
    else:
        texto = arg
    if len(sys.argv) > 2:
        out_mp3 = sys.argv[2]
else:
    import sys
    texto = sys.stdin.read()
if not texto.strip():
    msg = "ERRO: Nenhum texto informado"
    if out_mp3: open(out_mp3,"w",encoding="utf-8").write(msg)
    else: print(msg)
    sys.exit(1)
try:
    from gtts import gTTS
    tts = gTTS(text=texto, lang="pt-br", slow=False)
    tts.save(out_mp3)
    print(f"OK: {out_mp3}")
except ImportError:
    msg = "ERRO: gtts nao instalado (pip install gtts)"
    if out_mp3: open(out_mp3,"w",encoding="utf-8").write(msg)
    else: print(msg)
    sys.exit(1)
except Exception as e:
    msg = f"ERRO no TTS: {e}"
    if out_mp3: open(out_mp3,"w",encoding="utf-8").write(msg)
    else: print(msg)
    sys.exit(1)
