# Hyper Neiva Windows

Versão para PC do Hyper Neiva, feita em C com raylib: mídia, rádio, ferramentas, downloads (FTP/SFTP/SSH, Dropbox, Google Drive), IA com Gemini.

## Compilar

Requer MSYS2 com MinGW64 em `C:\msys64\mingw64` e as bibliotecas raylib, MuPDF, FFmpeg e SDL2.

```bat
cmd /c build.bat
```

## Chaves

As chaves não ficam no repositório. Copie `segredos_exemplo.h` para `segredos_local.h` e preencha. Sem esse arquivo o app compila, mas Dropbox, Google Drive e TMDB ficam sem as chaves padrão.

A pasta `data/` guarda dados pessoais e não é versionada.
## Licença

Este projeto é distribuído sob a GNU Affero General Public License, versão 3 (AGPL-3.0). Veja o arquivo [LICENSE](LICENSE).

O app usa bibliotecas de terceiros com licenças próprias, como MuPDF (AGPL-3.0) e FFmpeg (GPL-3.0).
