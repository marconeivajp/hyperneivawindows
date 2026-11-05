#ifndef GEMINI_IA_H
#define GEMINI_IA_H

#include <stdbool.h>

#define GEMINI_RESPOSTA_TAM 16384

typedef struct GeminiResposta {
    char texto[GEMINI_RESPOSTA_TAM]; // texto de resposta (ou mensagem de erro)
    char mediaPath[512];             // caminho do arquivo gerado (imagem/audio)
    bool temMedia;                   // true quando mediaPath valido
    bool ok;                         // true em sucesso, false em erro
} GeminiResposta;

// Obtem a chave da API Gemini salva nas Chaves API (apiKeys).
int gemini_obter_chave(char* outKey, int maxLen);

// Ultimo erro em formato de string (thread-safe simples).
const char* gemini_ultimo_erro(void);

// ===== Async: retornam true se a thread foi iniciada. Poll com gemini_busy() =====

// Geracao de texto (e multimodal quando mediaPaths != NULL / mediaCount > 0).
// outPath: arquivo texto de saida opcional (NULL = nao salva arquivo).
bool gemini_gerar_texto_async(const char* prompt, const char* modelo,
                              const char* mediaPaths[], int mediaCount,
                              const char* outPath, GeminiResposta* resp);

// Geracao de imagem. aspect: "1:1", "16:9", "9:16", "3:4", "4:3".
// outPng: caminho onde salvar o PNG gerado. resp->mediaPath = outPng em caso de sucesso.
bool gemini_gerar_imagem_async(const char* prompt, const char* modelo,
                               const char* aspect, const char* outPng, GeminiResposta* resp);

// Audio -> texto (transcricao). audioPath: .wav/.mp3/.mp4/.ogg/.m4a/.aac/.flac.
bool gemini_audio_para_texto_async(const char* audioPath, const char* modelo, GeminiResposta* resp);

// Texto -> audio (TTS). voice: nome da voz. Salva WAV em outMp3. resp->mediaPath = outMp3.
bool gemini_texto_para_audio_async(const char* texto, const char* voice, const char* outMp3, GeminiResposta* resp);

// true enquanto uma thread async ainda esta rodando.
bool gemini_busy(void);

// ===== Sincronos (usados pelas threads async) =====

bool gemini_gerar_texto(const char* apiKey, const char* model, const char* prompt,
                        const char* mediaPaths[], int mediaCount, char* outText, int outSz);

bool gemini_gerar_imagem(const char* apiKey, const char* model, const char* prompt,
                         const char* aspectRatio, const char* outPath);

bool gemini_audio_para_texto(const char* apiKey, const char* audioPath, const char* instrucao,
                             char* outText, int outSz);

bool gemini_texto_para_audio(const char* apiKey, const char* text, const char* voice,
                             const char* outPath);

void gemini_base64_encode_file(const char* path, char* out, int maxOut);
int gemini_base64_decode(const char* in, unsigned char* out, int maxOut);

#endif
