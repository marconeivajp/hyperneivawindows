@echo off
setlocal
set MINGW=C:\msys64\mingw64
set PATH=%MINGW%\bin;%PATH%

gcc -o player_build.exe ^
  player.c radio.c instrumentos.c controle_teste.c api_crypt.c ^
  teclado_teste.c ferramentas.c    video_player.c baixar_win.c ftp_win.c tmdb_win.c torrent_win.c midia_win.c baixar_dropbox_win.c baixar_google_drive_win.c gemini_ia.c ^
  -I"%MINGW%\include" -L"%MINGW%\lib" ^
  -lraylib -lmupdf -lavformat -lavcodec -lswscale ^
   -lswresample -lavutil -lopengl32 -lgdi32 -luser32 ^
   -lshell32 -lwinmm -lcomdlg32 -lSDL2 -lws2_32 -lcrypt32 -lpthread -lm -lole32 -luuid

if %errorlevel%==0 (
  echo.
  echo Build OK: player_build.exe
) else (
  echo.
  echo Build FALHOU
)
endlocal
