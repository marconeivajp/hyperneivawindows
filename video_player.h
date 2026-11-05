#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include "raylib.h"
#include <stdbool.h>

bool  VideoPlayer_Open(const char* filepath);
void  VideoPlayer_Update(void);
void  VideoPlayer_Draw(void);
void  VideoPlayer_Close(void);
bool  VideoPlayer_IsPlaying(void);
bool  VideoPlayer_WantsClose(void);   // returns true when user clicked X
void  VideoPlayer_SetVolume(float v); // 0.0 - 1.0
const char* VideoPlayer_LastError(void); // ultima mensagem de erro (vazio se ok)
bool  VideoPlayer_HouveErro(void);      // true se a ultima sessao teve erro

#endif // VIDEO_PLAYER_H
