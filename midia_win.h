#ifndef MIDIA_WIN_H
#define MIDIA_WIN_H

#include <stdbool.h>

#define MIDIA_MAX 80
#define MIDIA_PATH_LEN 1024

typedef struct {
    char nome[256];
    char link[1024];
    char info[128];
    int tipo; /* 0=anime 1=musica 2=livro 3=legenda 4=torrent-anime */
} MidiaItem;

extern MidiaItem midiaResultados[MIDIA_MAX];
extern int midiaTotal;
extern char midiaStatus[256];
extern bool midiaBuscando;
extern char midiaTermo[256];

void midia_init(void);
void midia_buscar_anime(const char* termo);
void midia_buscar_musica(const char* termo);
void midia_buscar_livro(const char* termo);
void midia_buscar_legenda(const char* termo);
void midia_buscar_anime_torrent(const char* termo);
void midia_aguardar(void);
void midia_abrir(int idx);
void midia_baixar(int idx);

#endif
