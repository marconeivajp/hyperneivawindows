#ifndef BAIXAR_WIN_H
#define BAIXAR_WIN_H

#include <stdbool.h>

#define BAIXAR_MAX_LINKS 3000
#define BAIXAR_LINK_LEN 1024
#define BAIXAR_NOME_LEN 256

typedef struct {
    char nome[BAIXAR_NOME_LEN];
    char link[BAIXAR_LINK_LEN];
} BaixarItem;

extern char baixarUrlInput[2048];
extern int baixarUrlCursor;
extern bool baixarTyping;

extern char baixarStatus[512];
extern float baixarProgresso;
extern int baixarArquivoAtual;
extern int baixarTotalArquivos;
extern bool baixarBaixando;

extern BaixarItem baixarLista[BAIXAR_MAX_LINKS];
extern int baixarTotal;
extern char baixarLojaErro[256];

extern char baixarPastaRepos[128];

void baixar_init(void);
void baixar_update(void);

bool baixar_adicionar_link_fila(const char* nome, const char* link);
bool baixar_obter_primeiro_link_fila(char* nomeOut, char* linkOut);
void baixar_remover_primeiro_link_fila(void);
int baixar_contar_fila(void);
void baixar_iniciar_fila(void);
void baixar_fila_aguardar(void);
extern char baixarUltimoArquivo[1024];

void baixar_listar_repositorios(BaixarItem* out, int* total);
void baixar_entrar_repositorio(const char* pasta);
void baixar_listar_xmls_repositorio(BaixarItem* out, int* total);
void baixar_abrir_xml(const char* arquivoXml, BaixarItem* out, int* total);
void baixar_mostrar_links_jogo(int gameIndex, BaixarItem* out, int* total);

void baixar_listar_lojas(BaixarItem* out, int* total);
void baixar_listar_dropbox(BaixarItem* out, int* total);
void baixar_listar_ftp(BaixarItem* out, int* total);

void baixar_carregar_hbstore(void);
void baixar_carregar_apollo(void);
void baixar_carregar_retroarch(void);
bool baixar_loja_carregando(void);
void baixar_loja_cancelar(void);


void baixar_baixar_url_direta(const char* url);
void baixar_obter_ip_local(char* ipOut);

const char* baixar_dir_base(void);
const char* baixar_dir_fila(void);

#endif
