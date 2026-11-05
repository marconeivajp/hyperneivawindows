#ifndef TMDB_WIN_H
#define TMDB_WIN_H

#include <stdbool.h>

#define TMDB_MAX_RESULTADOS 100
#define TMDB_PATH_LEN 1024

typedef struct {
    char titulo[256];
    char tituloOrig[256];
    char data[32];
    char overview[2048];
    char poster[512];
    char backdrop[512];
    char id[32];
    int ehSerie;
} TmdbItem;

extern TmdbItem tmdbResultados[TMDB_MAX_RESULTADOS];
extern int tmdbTotal;
extern char tmdbStatus[256];
extern bool tmdbBaixando;
extern float tmdbProgresso;

void tmdb_init(void);
void tmdb_buscar(const char* termo);
void tmdb_baixar_item(int idx);
void tmdb_aguardar(void);
void tmdb_listar_baixados(char nomes[][256], int* total);
const char* tmdb_ultimo_caminho(void);
const char* tmdb_ultimo_caminho_base(void);
void tmdb_baixar_capa_preview(int idx, const char* dst);

#endif
