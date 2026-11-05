## Objective
- App utilitário (origem PS4) em `C:\Users\Marco\Desktop\player` com ferramentas, seção **Baixar** (FTP/SFTP/SSH), criptografia de credenciais via DPAPI, Docker (Radarr/Sonarr/Jackett/Bazarr), e **IA** (transcrição de áudio, TTS, visão com IA).

## Important Details
- Pasta: `C:\Users\Marco\Desktop\player`; build: `cmd /c build.bat` (gcc mingw64, `-lpthread -lcrypt32`). `Stop-Process player_build` antes de rebuildar se rodando.
- `ferramentas.c` centraliza ferramentas. `radio.c` exporta `exec_silent` (bloqueante, `CREATE_NO_WINDOW`) e `exec_async_silent`.
- **NÃO incluir `<windows.h>`** em player.c (conflita com raylib).
- **Dropbox token exchange**: `dropbox_exchange()` em `player.c` escreve `.bat` temporário em `%TEMP%` com comando curl e `--data-urlencode`, executa com `_popen`, lê stdout direto. URL correta: `api.dropbox.com/oauth2/token` (NÃO `api.dropboxapi.com`). Token salvo em `data/HyperNeiva/configuracao/dropbox_token.txt` (apenas token puro, formato PS4).
- **FTP/SFTP/SSH**: módulo em `ftp_win.c`/`ftp_win.h`. `FtpServer.tipo`: 0=FTP, 1=SFTP, 2=SSH. Servidores em `ftp_servers_v2.txt` com senha/chave SSH criptografados via DPAPI. SSH abre terminal interativo (`ssh -i ...`).
- **AI Integration** (nativo em `player.c`):
  - **TRANSCREVER** (botão no player de áudio): `py scripts\transcrever.py caminho_audio saida.txt` — usa SpeechRecognition (Google API).
  - **LER TEXTO** (botão no bloco de notas): escreve texto em temp file, chama `py scripts\ler_texto.py @temp.txt saida.txt` — usa gTTS.
  - **VISÃO IA** (botão no visualizador de imagens): `py scripts\visao.py caminho_imagem saida.txt` — usa ollama.
  - Resultado exibido em overlay semi-transparente com scroll e botão FECHAR / ESC.
  - Variáveis globais: `aiResultado[8192]`, `aiTitulo`, `aiMostrando`, `aiScroll`, `aiTempPath`.
  - Função `ai_executar(cmd)`: roda via `exec_silent`, lê resultado de `aiTempPath`.
- Scripts Python em `scripts/`: `transcrever.py`, `ler_texto.py`, `visao.py`.
- Docker: Radarr (`:7878`), Sonarr (`:8989`), Jackett (`:9117`), Bazarr (`:6767`) em `C:\docker\starr-apps\`.
- Player de áudio: `optAudio[11]` — índice 9 = TRANSCREVER, 10 = VOLTAR.

## Work State
### Completed
- FTP/SFTP/SSH completo com criptografia DPAPI, preset HostGator.
- SSH terminal interativo.
- Docker (Radarr, Sonarr, Jackett, Bazarr) instalado e rodando.
- Scripts Python (transcrever, TTS, visão).
- **AI Integration**: botões TRANSCREVER (áudio), LER TEXTO (notepad), VISÃO IA (imagem) com overlay de resultado.
- **Chaves API**: menu CONFIGURACOES > Chaves API. Estrutura `ApiKey { nome, desc, chave, app_id }`. Suporte a adicionar/editar/deletar chaves. Presets: Google, Dropbox, Gemini, ChatGPT. Salvo em `data/HyperNeiva/api_keys.dat` (criptografado DPAPI). `optMain[8]` — CONFIGURACOES no índice 6, INFORMACOES no 7.
- **Gerar Token Dropbox**: botão na edição da chave Dropbox. Abre navegador, coleta código de autorização, troca por refresh_token via API, salva automaticamente na chave. Agora com 3 abas: Auto (chaves padrão), Avançado (suas chaves), Manual (access token direto).
- **Dropbox API**: módulo `baixar_dropbox_win.c` com navegação de pastas (list_folder), download (content.downloadapi), OAuth2 token refresh usando as credenciais das Chaves API.
- **Navegação Dropbox (MENU_BAIXAR_DROPBOX_NAV)**: renderização e clique para navegar pastas, baixar arquivos, subir diretório.
- **Filmes** adicionado ao menu Baixar > Mídia.
- **IA / GEMINI (Ferramentas)**: novo card/menu `TOOL_IA` em `ferramentas.c` com 5 abas: Texto, Visão, Gerar Imagem, Áudio→Texto, Texto→Áudio. Módulo `gemini_ia.c`/`gemini_ia.h` com async via pthread (`gemini_busy()`, `GeminiResposta{texto,mediaPath,temMedia,ok}`). Requer chave "Gemini" nas Chaves API. Arquivos de áudio/imagem listados de `GRAV_DIR`. Imagens geradas e áudio TTS salvos em `GRAV_DIR`.

### Active
- (none)

### Blocked
- `faster-whisper` não instalado (SSL/network). Fallback: SpeechRecognition (Google API, requer internet).
- Ollama não instalado. `visao.py` emitirá erro até instalar (`winget install Ollama.Ollama` e `ollama pull llava`).
- SSH HostGator timeout — possível firewall ou SSH desabilitado no cPanel.
