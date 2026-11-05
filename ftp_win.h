#ifndef FTP_WIN_H
#define FTP_WIN_H

#include <stdbool.h>

#define FTP_MAX_SERVERS 100
#define FTP_PATH_LEN 1024
#define FTP_NAME_LEN 128

typedef struct {
    char name[FTP_NAME_LEN];
    char ip[64];
    int port;
    char user[128];
    char pass[128];
    int tipo; // 0 = FTP, 1 = SFTP, 2 = SSH
    char ssh_key[FTP_PATH_LEN];
} FtpServer;

typedef struct {
    char name[FTP_NAME_LEN];
    char path[FTP_PATH_LEN];
    bool isDir;
} FtpItem;

extern FtpServer ftpServers[FTP_MAX_SERVERS];
extern int ftpTotalServers;
extern int ftpServerIdx;
extern char ftpRemotePath[FTP_PATH_LEN];
extern char ftpLocalPath[FTP_PATH_LEN];
extern FtpItem ftpRemoteList[2000];
extern int ftpRemoteTotal;
extern FtpItem ftpLocalList[2000];
extern int ftpLocalTotal;
extern bool ftpSelecionandoUpload;
extern char ftpStatus[256];
extern float ftpProgresso;
extern bool ftpTransferindo;
extern volatile bool ftpListandoRemoto;

void ftp_init(void);
void ftp_carregar_servidores(void);
void ftp_salvar_servidores(void);
void ftp_adicionar_servidor(const char* nome, const char* ip, int porta, const char* user, const char* pass, int tipo, const char* ssh_key);
void ftp_editar_servidor(int idx, const char* nome, const char* ip, int porta, const char* user, const char* pass, int tipo, const char* ssh_key);
void ftp_deletar_servidor(int idx);
void ftp_conectar_ssh(int idx);

void ftp_listar_remoto(const char* path);
void ftp_listar_remoto_async(const char* path);
void ftp_aguardar_listagem(void);
void ftp_listar_local(const char* path);
void ftp_entrar_pasta_remota(const char* nome);
void ftp_voltar_remota(void);
void ftp_entrar_pasta_local(const char* nome);
void ftp_voltar_local(void);

void ftp_baixar_item(int remoteIdx);
void ftp_enviar_item(int localIdx);
void ftp_criar_pasta_remota(const char* nome);
void ftp_criar_pasta_local(const char* nome);
void ftp_renomear_remota(const char* antigo, const char* novo);
void ftp_renomear_local(const char* antigo, const char* novo);
void ftp_aguardar(void);

void ftp_encrypt(const char* plain, char* out, int outLen);
void ftp_decrypt(const char* cipher, char* out, int outLen);

#endif
