import sys, os, subprocess, tempfile
modo = sys.argv[1] if len(sys.argv) > 1 else ""  # "tela" ou imagem_path
out = sys.argv[2] if len(sys.argv) > 2 else ""
modelo = "llava"

if not modo:
    msg = "ERRO: use 'tela' ou caminho da imagem"
    if out: open(out,"w",encoding="utf-8").write(msg)
    else: print(msg)
    sys.exit(1)

imagem = modo
if modo == "tela":
    # captura tela via PowerShell
    ps_script = """
Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::new([System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Width,
    [System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen(0,0,0,0,$bmp.Size)
$bmp.Save('%s', [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
"""
    img_path = os.path.join(tempfile.gettempdir(), "opencode_vision.png")
    ps_cmd = ps_script % img_path.replace("\\", "/")
    subprocess.run(["powershell", "-Command", ps_cmd], capture_output=True, timeout=30)
    if not os.path.isfile(img_path):
        msg = "ERRO: Falha ao capturar tela"
        if out: open(out,"w",encoding="utf-8").write(msg)
        else: print(msg)
        sys.exit(1)
    imagem = img_path
elif not os.path.isfile(imagem):
    msg = f"ERRO: Imagem nao encontrada: {imagem}"
    if out: open(out,"w",encoding="utf-8").write(msg)
    else: print(msg)
    sys.exit(1)

try:
    r = subprocess.run(["ollama", "run", modelo, "Descreva detalhadamente esta imagem em portugues."],
                       input=open(imagem,"rb").read(), capture_output=True, timeout=120)
    saida = r.stdout.decode("utf-8", errors="replace").strip()
    if not saida:
        saida = r.stderr.decode("utf-8", errors="replace").strip()
    if not saida:
        saida = "(sem resposta do modelo)"
except FileNotFoundError:
    saida = "ERRO: ollama nao encontrado (instale ollama.ai e faca: ollama pull llava)"
except subprocess.TimeoutExpired:
    saida = "ERRO: ollama excedeu o tempo limite"
except Exception as e:
    saida = f"ERRO na visao: {e}"

if modo == "tela" and os.path.isfile(img_path):
    os.remove(img_path)

if out:
    with open(out, "w", encoding="utf-8") as f: f.write(saida)
else:
    print(saida)
