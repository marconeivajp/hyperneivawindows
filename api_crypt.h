#ifndef API_CRYPT_H
#define API_CRYPT_H

#include <stdbool.h>

#define API_KEY_TOTAL 16

typedef struct {
    char nome[64];
    char desc[256];
    char chave[512];
    char app_id[256];
} ApiKey;

extern ApiKey apiKeys[API_KEY_TOTAL];
extern int apiKeyTotal;
extern int apiKeySel;

void apiKeys_carregar();
void apiKeys_salvar();
static inline void apiKeys_salvar_raw() { apiKeys_salvar(); }

#endif