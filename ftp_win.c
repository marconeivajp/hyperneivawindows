#include "ftp_win.h"

#include <pthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <io.h>
#include <sys/stat.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")

extern void exec_async_silent(const char* cmd);

static void bytes_to_hex(const unsigned char* in, int inLen, char* out, int outLen) {
    const char hex[] = "0123456789abcdef";
    int pos = 0;
    for (int i = 0; i < inLen && pos < outLen - 2; i++) {
        out[pos++] = hex[in[i] >> 4];
        out[pos++] = hex[in[i] & 0x0f];
    }
    out[pos] = 0;
}

static int hex_to_bytes(const char* in, unsigned char* out, int outLen) {
    int len = (int)strlen(in), pos = 0;
    for (int i = 0; i < len - 1 && pos < outLen; i += 2) {
        char hi = in[i], lo = in[i+1];
        unsigned char b = 0;
        if (hi >= '0' && hi <= '9') b = (hi - '0') << 4;
        else if (hi >= 'a' && hi <= 'f') b = (hi - 'a' + 10) << 4;
        else if (hi >= 'A' && hi <= 'F') b = (hi - 'A' + 10) << 4;
        if (lo >= '0' && lo <= '9') b |= (lo - '0');
        else if (lo >= 'a' && lo <= 'f') b |= (lo - 'a' + 10);
        else if (lo >= 'A' && lo <= 'F') b |= (lo - 'A' + 10);
        out[pos++] = b;
    }
    return pos;
}

static const BYTE g_entropy[] = { 0x4d, 0x61, 0x72, 0x63, 0x6f, 0x54, 0x6f, 0x6f, 0x6c, 0x73, 0x32, 0x30, 0x32, 0x36 };

void ftp_encrypt(const char* plain, char* out, int outLen) {
    if (!plain || !plain[0]) { out[0] = 0; return; }
    DATA_BLOB inBlob = { (DWORD)strlen(plain) + 1, (BYTE*)plain };
    DATA_BLOB outBlob = { 0, NULL };
    DATA_BLOB entropy = { sizeof(g_entropy), (BYTE*)g_entropy };
    if (CryptProtectData(&inBlob, L"", &entropy, NULL, NULL, 0, &outBlob)) {
        bytes_to_hex(outBlob.pbData, (int)outBlob.cbData, out, outLen);
        LocalFree(outBlob.pbData);
    } else {
        out[0] = 0;
    }
}

void ftp_decrypt(const char* cipher, char* out, int outLen) {
    if (!cipher || !cipher[0]) { out[0] = 0; return; }
    unsigned char bin[4096];
    int binLen = hex_to_bytes(cipher, bin, sizeof(bin));
    if (binLen <= 0) { out[0] = 0; return; }
    DATA_BLOB inBlob = { (DWORD)binLen, bin };
    DATA_BLOB outBlob = { 0, NULL };
    DATA_BLOB entropy = { sizeof(g_entropy), (BYTE*)g_entropy };
    if (CryptUnprotectData(&inBlob, NULL, &entropy, NULL, NULL, 0, &outBlob)) {
        int clen = (int)outBlob.cbData;
        if (clen > outLen - 1) clen = outLen - 1;
        memcpy(out, outBlob.pbData, clen);
        out[clen] = 0;
        LocalFree(outBlob.pbData);
    } else {
        out[0] = 0;
    }
}

FtpServer ftpServers[FTP_MAX_SERVERS];
int ftpTotalServers = 0;
int ftpServerIdx = 0;
char ftpRemotePath[FTP_PATH_LEN] = "/";
char ftpLocalPath[FTP_PATH_LEN] = ".";
FtpItem ftpRemoteList[2000];
int ftpRemoteTotal = 0;
FtpItem ftpLocalList[2000];
int ftpLocalTotal = 0;
bool ftpSelecionandoUpload = false;
char ftpStatus[256] = "Pronto.";
float ftpProgresso = 0.0f;
bool ftpTransferindo = false;

static char g_dirBase[FTP_PATH_LEN] = "";
static char g_serversFile[FTP_PATH_LEN] = "";

static pthread_t g_ftpThread;
static bool g_ftpThreadAtiva = false;

static const char* ftp_proto(void) {
    int t = ftpServers[ftpServerIdx].tipo;
    return t == 2 ? "" : t == 1 ? "sftp" : "ftp";
}

void ftp_init(void) {
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    char exePath[FTP_PATH_LEN] = ".";
    DWORD n = GetModuleFileNameA(NULL, exePath, FTP_PATH_LEN);
    if (n > 0) {
        char* slash = strrchr(exePath, '\\');
        if (!slash) slash = strrchr(exePath, '/');
        if (slash) *slash = 0;
    }
    sprintf(g_dirBase, "%s/data/HyperNeiva/baixar", exePath);
    sprintf(g_serversFile, "%s/ftp_servers_v2.txt", g_dirBase);
    _mkdir(g_dirBase);
    ftp_carregar_servidores();
}

// ===================== BASE DE REDE =====================
static int send_ftp_cmd(SOCKET sock, const char* cmd, char* response) {
    if (cmd) { send(sock, cmd, (int)strlen(cmd), 0); send(sock, "\r\n", 2, 0); }
    if (response) {
        memset(response, 0, 2048); int total = 0;
        char buf[512];
        while (total < 2047) {
            int nr = recv(sock, buf, sizeof(buf) - 1, 0);
            if (nr <= 0) break;
            for (int k = 0; k < nr && total < 2047; k++) response[total++] = buf[k];
            response[total] = 0;
            // a resposta do comando e' a ULTIMA linha de status (3 digitos + espaco, nao hifen).
            // se ja lemos uma linha de status final, paramos. mas pode haver resposta pendente
            // anterior; entao so' paramos quando a linha de status final estiver presente.
            char* ultima = NULL;
            char* p = response;
            while ((p = strstr(p, "\n"))) {
                char* lin = p; while (lin > response && lin[-1] != '\n') lin--;
                int ll = (int)(p - lin);
                if (ll >= 4 && isdigit((unsigned char)lin[0]) && isdigit((unsigned char)lin[1]) && isdigit((unsigned char)lin[2]) && lin[3] == ' ' && lin[3] != '-') {
                    ultima = lin;
                }
                p++;
            }
            if (ultima) return 0;
        }
    }
    return 0;
}

static SOCKET ftp_connect_control(int srvIdx) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return -1;
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons(ftpServers[srvIdx].port);
    inet_pton(AF_INET, ftpServers[srvIdx].ip, &addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) { closesocket(sock); return -1; }
    struct timeval tv; tv.tv_sec = 5; tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
    char resp[2048]; send_ftp_cmd(sock, NULL, resp);
    return (int)sock;
}

static SOCKET ftp_enter_pasv(int ctrl_sock) {
    char resp[2048]; send_ftp_cmd((SOCKET)ctrl_sock, "PASV", resp);
    int h1, h2, h3, h4, p1, p2; char* start = strchr(resp, '(');
    if (!start) return -1;
    if (sscanf(start, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2) != 6) return -1;
    int data_port = (p1 << 8) | p2; char ip[32]; sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    SOCKET data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock == INVALID_SOCKET) return -1;
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons(data_port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    struct timeval tv; tv.tv_sec = 8; tv.tv_usec = 0;
    setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    if (connect(data_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) { closesocket(data_sock); return -1; }
    return (int)data_sock;
}

static void ftp_login(SOCKET ctrl_sock, int srvIdx) {
    char resp[2048]; char cmdUser[128], cmdPass[128];
    sprintf(cmdUser, "USER %s", ftpServers[srvIdx].user); send_ftp_cmd(ctrl_sock, cmdUser, resp);
    if (strlen(ftpServers[srvIdx].pass) > 0) sprintf(cmdPass, "PASS %s", ftpServers[srvIdx].pass);
    else sprintf(cmdPass, "PASS hyperneiva@ps4.com");
    send_ftp_cmd(ctrl_sock, cmdPass, resp);
}

static bool parse_ftp_line(char* line, char* nameOut, bool* isDirOut) {
    *isDirOut = false; nameOut[0] = 0;
    if (line[0] == 'd' || line[0] == '-') {
        *isDirOut = (line[0] == 'd'); char* p = line; int spaces = 0;
        while (*p && spaces < 8) { if (*p == ' ') { spaces++; while (*(p + 1) == ' ') p++; } p++; }
        if (*p) { strcpy(nameOut, p); return true; }
    }
    else if (isdigit((unsigned char)line[0])) {
        if (strstr(line, "<DIR>")) *isDirOut = true; char* p = line; int spaces = 0;
        while (*p && spaces < 3) { if (*p == ' ') { spaces++; while (*(p + 1) == ' ') p++; } p++; }
        if (*p) { strcpy(nameOut, p); return true; }
    }
    return false;
}

// ===================== SERVIDORES =====================
void ftp_carregar_servidores(void) {
    ftpTotalServers = 0;
    FILE* f = fopen(g_serversFile, "r");
    if (!f) return;
    char linha[2048];
    while (fgets(linha, sizeof(linha), f) && ftpTotalServers < FTP_MAX_SERVERS) {
        linha[strcspn(linha, "\r\n")] = 0;
        char* p = linha; char* parts[7]; int c = 0;
        while (c < 7) { parts[c] = p; char* bar = strchr(p, '|'); if (!bar) break; *bar = 0; p = bar + 1; c++; }
        if (c >= 4) {
            strncpy(ftpServers[ftpTotalServers].name, parts[0], FTP_NAME_LEN - 1); ftpServers[ftpTotalServers].name[FTP_NAME_LEN - 1] = 0;
            strncpy(ftpServers[ftpTotalServers].ip, parts[1], 63); ftpServers[ftpTotalServers].ip[63] = 0;
            ftpServers[ftpTotalServers].port = atoi(parts[2]);
            strncpy(ftpServers[ftpTotalServers].user, parts[3], 127); ftpServers[ftpTotalServers].user[127] = 0;
            if (c >= 4 && parts[4] && parts[4][0]) {
                char dec[256]; ftp_decrypt(parts[4], dec, sizeof(dec));
                strncpy(ftpServers[ftpTotalServers].pass, dec[0] ? dec : parts[4], 127);
            } else ftpServers[ftpTotalServers].pass[0] = 0;
            ftpServers[ftpTotalServers].tipo = 0;
            if (c >= 5 && parts[5]) ftpServers[ftpTotalServers].tipo = atoi(parts[5]);
            ftpServers[ftpTotalServers].ssh_key[0] = 0;
            if (c >= 6 && parts[6] && parts[6][0]) {
                char dec[FTP_PATH_LEN]; ftp_decrypt(parts[6], dec, sizeof(dec));
                strncpy(ftpServers[ftpTotalServers].ssh_key, dec[0] ? dec : parts[6], FTP_PATH_LEN - 1);
            }
                if (ftpServers[ftpTotalServers].port <= 0) { int t = ftpServers[ftpTotalServers].tipo; ftpServers[ftpTotalServers].port = (t == 2) ? 22 : (t == 1) ? 2222 : 21; }
                if (ftpServers[ftpTotalServers].tipo == 2 && ftpServers[ftpTotalServers].port == 21) ftpServers[ftpTotalServers].port = 22;
            ftpTotalServers++;
        }
    }
    fclose(f);
}

void ftp_salvar_servidores(void) {
    FILE* f = fopen(g_serversFile, "w");
    if (!f) return;
    for (int i = 0; i < ftpTotalServers; i++) {
        char encPass[512] = "", encKey[2048] = "";
        if (ftpServers[i].pass[0]) ftp_encrypt(ftpServers[i].pass, encPass, sizeof(encPass));
        if (ftpServers[i].ssh_key[0]) ftp_encrypt(ftpServers[i].ssh_key, encKey, sizeof(encKey));
        fprintf(f, "%s|%s|%d|%s|%s|%d|%s\n", ftpServers[i].name, ftpServers[i].ip, ftpServers[i].port, ftpServers[i].user, encPass, ftpServers[i].tipo, encKey);
    }
    fclose(f);
}

void ftp_adicionar_servidor(const char* nome, const char* ip, int porta, const char* user, const char* pass, int tipo, const char* ssh_key) {
    if (ftpTotalServers >= FTP_MAX_SERVERS) return;
    int i = ftpTotalServers++;
    strncpy(ftpServers[i].name, nome, FTP_NAME_LEN - 1); ftpServers[i].name[FTP_NAME_LEN - 1] = 0;
    strncpy(ftpServers[i].ip, ip, 63); ftpServers[i].ip[63] = 0;
    ftpServers[i].tipo = tipo;
    ftpServers[i].port = porta > 0 ? porta : (tipo == 2 ? 22 : tipo == 1 ? 2222 : 21);
    strncpy(ftpServers[i].user, user, 127); ftpServers[i].user[127] = 0;
    strncpy(ftpServers[i].pass, pass, 127); ftpServers[i].pass[127] = 0;
    if (ssh_key) { strncpy(ftpServers[i].ssh_key, ssh_key, FTP_PATH_LEN - 1); ftpServers[i].ssh_key[FTP_PATH_LEN - 1] = 0; }
    else ftpServers[i].ssh_key[0] = 0;
    ftp_salvar_servidores();
}

void ftp_editar_servidor(int idx, const char* nome, const char* ip, int porta, const char* user, const char* pass, int tipo, const char* ssh_key) {
    if (idx < 0 || idx >= ftpTotalServers) return;
    strncpy(ftpServers[idx].name, nome, FTP_NAME_LEN - 1); ftpServers[idx].name[FTP_NAME_LEN - 1] = 0;
    strncpy(ftpServers[idx].ip, ip, 63); ftpServers[idx].ip[63] = 0;
    ftpServers[idx].tipo = tipo;
    ftpServers[idx].port = porta > 0 ? porta : (tipo == 2 ? 22 : tipo == 1 ? 2222 : 21);
    strncpy(ftpServers[idx].user, user, 127); ftpServers[idx].user[127] = 0;
    strncpy(ftpServers[idx].pass, pass, 127); ftpServers[idx].pass[127] = 0;
    if (ssh_key) { strncpy(ftpServers[idx].ssh_key, ssh_key, FTP_PATH_LEN - 1); ftpServers[idx].ssh_key[FTP_PATH_LEN - 1] = 0; }
    else ftpServers[idx].ssh_key[0] = 0;
    ftp_salvar_servidores();
}

void ftp_deletar_servidor(int idx) {
    if (idx < 0 || idx >= ftpTotalServers) return;
    for (int i = idx; i < ftpTotalServers - 1; i++) ftpServers[i] = ftpServers[i + 1];
    ftpTotalServers--;
    ftp_salvar_servidores();
}

// ===================== LISTAR =====================
static int ftp_run_cmd(const char* cmd);
static void ftp_exec_curl(const char* cmd);
static int ftp_ssh_rodar(const char* remoteCmd, const char* redirect);

static void ftp_ordenar_pastas_primeiro(FtpItem* list, int total) {
    if (total <= 1) return;
    FtpItem* tmp = (FtpItem*)malloc(sizeof(FtpItem) * total);
    if (!tmp) return;
    int n = 0;
    for (int i = 0; i < total; i++) if (list[i].isDir) tmp[n++] = list[i];
    for (int i = 0; i < total; i++) if (!list[i].isDir) tmp[n++] = list[i];
    for (int i = 0; i < total; i++) list[i] = tmp[i];
    free(tmp);
}

void ftp_listar_remoto(const char* path) {
    strcpy(ftpRemotePath, path);
    char tmpList[1024]; sprintf(tmpList, "%s/ftp_tmp_listing_%lu.txt", g_dirBase, GetTickCount());
    int ec = 0;
    bool sshList = false;
    if (ftpServers[ftpServerIdx].tipo == 2) {
        sshList = true;
        char remoteCmd[1400]; sprintf(remoteCmd, "\"ls -Fp %s\"", path);
        char redirect[1200]; sprintf(redirect, "> \"%s\"", tmpList);
        ec = ftp_ssh_rodar(remoteCmd, redirect);
    } else {
        const char* proto = ftp_proto();
        char lst[1500]; sprintf(lst, "%s://%s:%d/%s", proto, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port, path);
        char cmd[2000];
        if (ftpServers[ftpServerIdx].tipo == 1)
            sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -l -u \"%s:%s\" \"%s\" > \"%s\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, lst, tmpList);
        else
            sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --ftp-pasv --list-only -u \"%s:%s\" \"%s\" > \"%s\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, lst, tmpList);
        ec = ftp_run_cmd(cmd);
    }

    ftpRemoteTotal = 0;
    FILE* lf = fopen(tmpList, "rb");
    if (lf) {
        fseek(lf, 0, SEEK_END); long sz = ftell(lf); fseek(lf, 0, SEEK_SET);
        char* ld = (char*)malloc(sz + 1); if (fread(ld, 1, sz, lf) == (size_t)sz) {} ld[sz] = 0; fclose(lf);
        char* l = strtok(ld, "\r\n");
        if (sshList) {
            while (l && ftpRemoteTotal < 1999) {
                while (*l == ' ' || *l == '\t') l++;
                int nl = (int)strlen(l); while (nl > 0 && (l[nl-1]==' ' || l[nl-1]=='\t')) l[--nl] = 0;
                if (!*l) { l = strtok(NULL, "\r\n"); continue; }
                bool isDir = false;
                int n = (int)strlen(l);
                if (n > 0 && l[n-1] == '/') { isDir = true; l[n-1] = 0; }
                else if (n > 0 && (l[n-1] == '*' || l[n-1] == '@' || l[n-1] == '=' || l[n-1] == '|')) l[n-1] = 0;
                if (!*l || strcmp(l, ".") == 0 || strcmp(l, "..") == 0) { l = strtok(NULL, "\r\n"); continue; }
                strncpy(ftpRemoteList[ftpRemoteTotal].name, l, FTP_NAME_LEN - 1); ftpRemoteList[ftpRemoteTotal].name[FTP_NAME_LEN - 1] = 0;
                ftpRemoteList[ftpRemoteTotal].isDir = isDir;
                sprintf(ftpRemoteList[ftpRemoteTotal].path, "%s%s%s", path, strcmp(path, "/") == 0 ? "" : "/", l);
                ftpRemoteTotal++;
                l = strtok(NULL, "\r\n");
            }
        } else {
            while (l && ftpRemoteTotal < 1999) {
                while (*l == ' ' || *l == '\t') l++;
                if (!*l) { l = strtok(NULL, "\r\n"); continue; }
                bool isDir = (l[0] == 'd');
                char filename[256];
                if (isDir) { int sp = 0; char* p = l; while (*p && sp < 8) { if (*p == ' ') { sp++; while (*(p+1)==' ') p++; } p++; } if (*p) strcpy(filename, p); else strcpy(filename, l); }
                else { char* p = l; while (*p && *p != ' ' && *p != '\t') p++; while (*p == ' ' || *p == '\t') p++; if (*p) strcpy(filename, p); else strcpy(filename, l); }
                if (filename[0] && strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0) {
                    strncpy(ftpRemoteList[ftpRemoteTotal].name, filename, FTP_NAME_LEN - 1); ftpRemoteList[ftpRemoteTotal].name[FTP_NAME_LEN - 1] = 0;
                    ftpRemoteList[ftpRemoteTotal].isDir = isDir;
                    sprintf(ftpRemoteList[ftpRemoteTotal].path, "%s%s%s", path, strcmp(path, "/") == 0 ? "" : "/", filename);
                    ftpRemoteTotal++;
                }
                l = strtok(NULL, "\r\n");
            }
        }
        free(ld);
    }
    remove(tmpList);
    ftp_ordenar_pastas_primeiro(ftpRemoteList, ftpRemoteTotal);
    if (ftpRemoteTotal == 0) { strcpy(ftpRemoteList[0].name, "Pasta Vazia"); ftpRemoteList[0].path[0] = 0; ftpRemoteList[0].isDir = false; ftpRemoteTotal = 1; }
    sprintf(ftpStatus, ec == 0 ? "Conectado: %s" : "Erro ao listar.", ftpServers[ftpServerIdx].name);
}

static void listar_local_rec(const char* path) {
    strcpy(ftpLocalPath, path);
    ftpLocalTotal = 0;
    struct _finddata_t fd; char busca[1024]; sprintf(busca, "%s/*", path);
    intptr_t h = _findfirst(busca, &fd);
    if (h == -1) { strcpy(ftpLocalList[0].name, "Pasta Vazia"); ftpLocalList[0].path[0] = 0; ftpLocalList[0].isDir = false; ftpLocalTotal = 1; return; }
    do {
        if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
        if (ftpLocalTotal >= 1999) break;
        strncpy(ftpLocalList[ftpLocalTotal].name, fd.name, FTP_NAME_LEN - 1); ftpLocalList[ftpLocalTotal].name[FTP_NAME_LEN - 1] = 0;
        ftpLocalList[ftpLocalTotal].isDir = (fd.attrib & _A_SUBDIR) != 0;
        sprintf(ftpLocalList[ftpLocalTotal].path, "%s/%s", path, fd.name);
        ftpLocalTotal++;
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
    if (ftpLocalTotal == 0) { strcpy(ftpLocalList[0].name, "Pasta Vazia"); ftpLocalList[0].path[0] = 0; ftpLocalList[0].isDir = false; ftpLocalTotal = 1; }
    ftp_ordenar_pastas_primeiro(ftpLocalList, ftpLocalTotal);
}

void ftp_listar_local(const char* path) { listar_local_rec(path); }

// ============ LISTAGEM REMOTA ASSINCRONA ============
volatile bool ftpListandoRemoto = false;
static char ftpListPendente[FTP_PATH_LEN];
static pthread_t ftpListThread;
static bool ftpListThreadAtiva = false;

static void* thread_listar_remoto(void* arg) {
    (void)arg;
    char path[FTP_PATH_LEN];
    for (;;) {
        while (!ftpListPendente[0]) { Sleep(30); }
        strncpy(path, ftpListPendente, FTP_PATH_LEN - 1); path[FTP_PATH_LEN - 1] = 0;
        ftpListPendente[0] = 0;
        ftp_listar_remoto(path);
        if (!ftpListPendente[0]) break;
    }
    ftpListandoRemoto = false;
    return NULL;
}

void ftp_listar_remoto_async(const char* path) {
    strncpy(ftpListPendente, path, FTP_PATH_LEN - 1); ftpListPendente[FTP_PATH_LEN - 1] = 0;
    if (!ftpListandoRemoto) {
        ftpListandoRemoto = true;
        if (pthread_create(&ftpListThread, NULL, thread_listar_remoto, NULL) != 0) {
            ftpListandoRemoto = false; ftpListPendente[0] = 0;
        } else ftpListThreadAtiva = true;
    }
}

void ftp_aguardar_listagem(void) {
    if (ftpListThreadAtiva) { pthread_join(ftpListThread, NULL); ftpListThreadAtiva = false; }
    ftpListandoRemoto = false;
}

void ftp_entrar_pasta_remota(const char* nome) {
    char novo[FTP_PATH_LEN]; sprintf(novo, "%s%s%s", ftpRemotePath, strcmp(ftpRemotePath, "/") == 0 ? "" : "/", nome);
    ftp_listar_remoto_async(novo);
}

void ftp_voltar_remota(void) {
    char* bar = strrchr(ftpRemotePath, '/');
    if (bar && bar != ftpRemotePath) { *bar = 0; ftp_listar_remoto_async(ftpRemotePath); }
    else ftp_listar_remoto_async("/");
}

void ftp_entrar_pasta_local(const char* nome) {
    char novo[FTP_PATH_LEN]; sprintf(novo, "%s/%s", ftpLocalPath, nome);
    listar_local_rec(novo);
}

void ftp_voltar_local(void) {
    char* bar = strrchr(ftpLocalPath, '/');
    if (bar && bar != ftpLocalPath) { *bar = 0; listar_local_rec(ftpLocalPath); }
    else listar_local_rec(ftpLocalPath);
}

// ===================== TRANSFERENCIAS =====================
typedef struct {
    char src[FTP_PATH_LEN];
    char dst[FTP_PATH_LEN];
    bool isUpload;
    bool isDir;
} FtpTransfer;

static FtpTransfer ftpFila[2000];
static int ftpFilaCount = 0;
static int ftpFilaCurrent = 0;
static bool ftpFilaRodando = false;

static void ftp_adicionar_fila(const char* src, const char* dst, bool isUpload, bool isDir) {
    if (ftpFilaCount >= 1999) return;
    strncpy(ftpFila[ftpFilaCount].src, src, FTP_PATH_LEN - 1); ftpFila[ftpFilaCount].src[FTP_PATH_LEN - 1] = 0;
    strncpy(ftpFila[ftpFilaCount].dst, dst, FTP_PATH_LEN - 1); ftpFila[ftpFilaCount].dst[FTP_PATH_LEN - 1] = 0;
    ftpFila[ftpFilaCount].isUpload = isUpload; ftpFila[ftpFilaCount].isDir = isDir;
    ftpFilaCount++;
}

static int ftp_run_cmd(const char* cmd) {
    char full[4096]; sprintf(full, "cmd /c \"%s\"", cmd);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    
    HANDLE hJob = CreateJobObjectA(NULL, NULL);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
        memset(&jeli, 0, sizeof(jeli));
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    if (!CreateProcessA(NULL, full, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        if (hJob) CloseHandle(hJob);
        return -1;
    }
    
    if (hJob) AssignProcessToJobObject(hJob, pi.hProcess);

    DWORD wr = WaitForSingleObject(pi.hProcess, 20000);
    if (wr == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
    }
    DWORD ec = 0; GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    if (hJob) CloseHandle(hJob);
    return (int)ec;
}

static void ftp_exec_curl(const char* cmd) { ftp_run_cmd(cmd); }

static void* thread_ftp(void* arg) {
    (void)arg;
    ftpTransferindo = true; ftpFilaRodando = true;

    while (ftpFilaCurrent < ftpFilaCount) {
        FtpTransfer* t = &ftpFila[ftpFilaCurrent];
        if (t->isDir) {
            if (t->isUpload) {
                const char* proto = ftp_proto();
                char mkurl[1200]; sprintf(mkurl, "%s://%s:%d/%s", proto, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port, t->dst);
                char cmd[1800];
                if (ftpServers[ftpServerIdx].tipo == 1)
                    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -u \"%s:%s\" -Q \"mkdir %s\" \"%s://%s:%d/\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, t->dst, proto, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port);
                else
                    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --ftp-create-dirs -u \"%s:%s\" -T NUL \"%s/\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, mkurl);
                ftp_exec_curl(cmd);
                struct _finddata_t fd; intptr_t h = _findfirst(t->src, &fd);
                if (h != -1) {
                    do {
                        if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
                        char ns[1024], nd[1024];
                        sprintf(ns, "%s/%s", t->src, fd.name); sprintf(nd, "%s/%s", t->dst, fd.name);
                        bool d = (fd.attrib & _A_SUBDIR) != 0;
                        ftp_adicionar_fila(ns, nd, true, d);
                    } while (_findnext(h, &fd) == 0);
                    _findclose(h);
                }
            } else {
                _mkdir(t->dst);
                const char* proto = ftp_proto();
                char lst[1500]; sprintf(lst, "%s://%s:%d/%s", proto, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port, t->src);
                char tmpList[1024]; sprintf(tmpList, "%s/ftp_tmp_listing.txt", g_dirBase);
                char cmd[2000];
                if (ftpServers[ftpServerIdx].tipo == 1)
                    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -l -u \"%s:%s\" \"%s\" > \"%s\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, lst, tmpList);
                else
                    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --ftp-pasv --list-only -u \"%s:%s\" \"%s\" > \"%s\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, lst, tmpList);
                ftp_exec_curl(cmd);
                FILE* lf = fopen(tmpList, "rb");
                if (lf) {
                    fseek(lf, 0, SEEK_END); long sz = ftell(lf); fseek(lf, 0, SEEK_SET);
                    char* ld = (char*)malloc(sz + 1); fread(ld, 1, sz, lf); ld[sz] = 0; fclose(lf);
                    char* l = strtok(ld, "\r\n");
                    while (l) {
                        while (*l == ' ' || *l == '\t') l++;
                        bool d = (l[0] == 'd');
                        char fn[256];
                        if (d) { char* p = l; int sp = 0; while (*p && sp < 8) { if (*p == ' ') { sp++; while (*(p+1)==' ') p++; } p++; } if (*p) strcpy(fn, p); }
                        else { char* p = l; while (*p && *p != ' ' && *p != '\t') p++; while (*p == ' ' || *p == '\t') p++; if (*p) strcpy(fn, p); else strcpy(fn, l); }
                        if (fn[0] && strcmp(fn, ".") != 0 && strcmp(fn, "..") != 0) {
                            char ns[1024], nd[1024];
                            sprintf(ns, "%s/%s", t->src, fn); sprintf(nd, "%s/%s", t->dst, fn);
                            ftp_adicionar_fila(ns, nd, false, d);
                        }
                        l = strtok(NULL, "\r\n");
                    }
                    free(ld);
                }
                remove(tmpList);
            }
        } else {
            char* bn = strrchr(t->dst, '/'); if (!bn) bn = strrchr(t->dst, '\\'); if (bn) bn++; else bn = t->dst;
            ftpProgresso = 0;
            if (t->isUpload) {
                const char* proto = ftp_proto();
                char upurl[1500]; sprintf(upurl, "%s://%s:%d/%s", proto, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port, t->dst);
                char cmd[2200]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s %s-u \"%s:%s\" -T \"%s\" \"%s\"", ftpServers[ftpServerIdx].tipo == 1 ? "" : "--ftp-pasv ", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, t->src, upurl);
                sprintf(ftpStatus, "Enviando %s", bn);
                ftp_exec_curl(cmd);
            } else {
                const char* proto = ftp_proto();
                char dlurl[1500]; sprintf(dlurl, "%s://%s:%d/%s", proto, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port, t->src);
                char cmd[2200]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s %s-u \"%s:%s\" -o \"%s\" \"%s\"", ftpServers[ftpServerIdx].tipo == 1 ? "" : "--ftp-pasv ", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, t->dst, dlurl);
                sprintf(ftpStatus, "Baixando %s", bn);
                ftp_exec_curl(cmd);
                if (t->dst[0]) { struct stat st; if (stat(t->dst, &st) == 0) ftpProgresso = 1.0f; }
            }
        }
        ftpFilaCurrent++;
    }
    ftpTransferindo = false; ftpFilaRodando = false; ftpFilaCount = 0; ftpFilaCurrent = 0; ftpProgresso = 0;
    g_ftpThreadAtiva = false;
    sprintf(ftpStatus, "Transferencia concluida!");
    return NULL;
}

static void criar_dir_rec(const char* p) {
    char tmp[1024];
    strcpy(tmp, p);
    for (char* c = tmp + 1; *c; c++) {
        if (*c == '/' || *c == '\\') {
            char save = *c; *c = 0; _mkdir(tmp); *c = save;
        }
    }
    _mkdir(tmp);
}

void ftp_baixar_item(int remoteIdx) {
    if (remoteIdx < 0 || remoteIdx >= ftpRemoteTotal) return;
    if (!ftpRemoteList[remoteIdx].path[0]) return;
    char dstDir[1024]; sprintf(dstDir, "%s/ftp", g_dirBase);
    criar_dir_rec(dstDir);
    char dst[1024]; sprintf(dst, "%s/ftp/%s", g_dirBase, ftpRemoteList[remoteIdx].name);
    ftp_adicionar_fila(ftpRemoteList[remoteIdx].path, dst, false, ftpRemoteList[remoteIdx].isDir);
    if (!ftpFilaRodando) { pthread_create(&g_ftpThread, NULL, thread_ftp, NULL); g_ftpThreadAtiva = true; }
}

void ftp_enviar_item(int localIdx) {
    if (localIdx < 0 || localIdx >= ftpLocalTotal) return;
    if (!ftpLocalList[localIdx].path[0]) return;
    char dst[1024];
    if (strcmp(ftpRemotePath, "/") == 0) sprintf(dst, "/%s", ftpLocalList[localIdx].name);
    else sprintf(dst, "%s/%s", ftpRemotePath, ftpLocalList[localIdx].name);
    ftp_adicionar_fila(ftpLocalList[localIdx].path, dst, true, ftpLocalList[localIdx].isDir);
    if (!ftpFilaRodando) { pthread_create(&g_ftpThread, NULL, thread_ftp, NULL); g_ftpThreadAtiva = true; }
}

void ftp_criar_pasta_remota(const char* nome) {
    char rpath[1024];
    if (strcmp(ftpRemotePath, "/") == 0) sprintf(rpath, "/%s", nome);
    else sprintf(rpath, "%s/%s", ftpRemotePath, nome);
    const char* proto = ftp_proto();
    char mkurl[1500]; sprintf(mkurl, "%s://%s:%d/%s", proto, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port, rpath);
    char cmd[1800];
    if (ftpServers[ftpServerIdx].tipo == 1)
        sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -u \"%s:%s\" -Q \"mkdir %s\" \"%s://%s:%d/\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, rpath, proto, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port);
    else
        sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --ftp-create-dirs -u \"%s:%s\" -T NUL \"%s/\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, mkurl);
    ftp_run_cmd(cmd);
    ftp_listar_remoto(ftpRemotePath);
}

void ftp_criar_pasta_local(const char* nome) {
    char novo[1024]; sprintf(novo, "%s/%s", ftpLocalPath, nome);
    _mkdir(novo);
    listar_local_rec(ftpLocalPath);
}

void ftp_renomear_remota(const char* antigo, const char* novo) {
    char oldp[1024];
    if (strcmp(ftpRemotePath, "/") == 0) sprintf(oldp, "/%s", antigo);
    else sprintf(oldp, "%s/%s", ftpRemotePath, antigo);
    char newp[1024];
    if (strcmp(ftpRemotePath, "/") == 0) sprintf(newp, "/%s", novo);
    else sprintf(newp, "%s/%s", ftpRemotePath, novo);
    const char* proto = ftp_proto();
    char cmd[3000];
    if (ftpServers[ftpServerIdx].tipo == 1)
        sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -u \"%s:%s\" -Q \"rename %s %s\" \"%s://%s:%d/\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, oldp, newp, proto, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port);
    else
        sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --ftp-pasv -u \"%s:%s\" -Q \"RNFR %s\" -Q \"RNTO %s\" \"ftp://%s:%d%s\"", ftpServers[ftpServerIdx].user, ftpServers[ftpServerIdx].pass, oldp, newp, ftpServers[ftpServerIdx].ip, ftpServers[ftpServerIdx].port, oldp);
    ftp_run_cmd(cmd);
    ftp_listar_remoto(ftpRemotePath);
}

void ftp_renomear_local(const char* antigo, const char* novo) {
    char src[1024]; sprintf(src, "%s/%s", ftpLocalPath, antigo);
    char dest[1024]; sprintf(dest, "%s/%s", ftpLocalPath, novo);
    rename(src, dest);
    listar_local_rec(ftpLocalPath);
}

void ftp_aguardar(void) {
    if (g_ftpThreadAtiva) { pthread_join(g_ftpThread, NULL); g_ftpThreadAtiva = false; }
}

// ===================== SSH =====================
static void ftp_escape_bat(const char* in, char* out, int outLen) {
    int o = 0;
    for (const char* p = in; *p && o < outLen - 4; p++) {
        char c = *p;
        if (c == '%') { out[o++] = '%'; out[o++] = '%'; }
        else if (c == '^') { out[o++] = '^'; out[o++] = '^'; }
        else if (c == '&' || c == '|' || c == '<' || c == '>' || c == '(' || c == ')') { out[o++] = '^'; out[o++] = c; }
        else out[o++] = c;
    }
    out[o] = 0;
}

static const char* ftp_ssh_askpass(void) {
    static char askPath[1200];
    char tmpDir[1024]; GetTempPathA(sizeof(tmpDir), tmpDir);
    sprintf(askPath, "%shyperneiva_ssh_ask.bat", tmpDir);
    char passEsc[600]; ftp_escape_bat(ftpServers[ftpServerIdx].pass, passEsc, sizeof(passEsc));
    FILE* f = fopen(askPath, "w");
    if (f) { fprintf(f, "@echo %s\r\n", passEsc); fclose(f); }
    return askPath;
}

static int ftp_ssh_rodar(const char* remoteCmd, const char* redirect) {
    char tmpDir[1024]; GetTempPathA(sizeof(tmpDir), tmpDir);
    char batPath[1200]; sprintf(batPath, "%shyperneiva_ssh_cmd.bat", tmpDir);
    const char* user = ftpServers[ftpServerIdx].user[0] ? ftpServers[ftpServerIdx].user : "root";
    int port = ftpServers[ftpServerIdx].port;
    if (port <= 0 || port == 21) port = 22;
    FILE* f = fopen(batPath, "w");
    if (!f) return -1;
    fprintf(f, "@echo off\r\n");
    fprintf(f, "set \"SSH_ASKPASS=%s\"\r\n", ftp_ssh_askpass());
    fprintf(f, "set \"SSH_ASKPASS_REQUIRE=force\"\r\n");
    fprintf(f, "set \"DISPLAY=localhost:0\"\r\n");
    fprintf(f, "ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=20 -p %d %s@%s %s %s\r\n",
            port, user, ftpServers[ftpServerIdx].ip, remoteCmd, redirect ? redirect : "");
    fclose(f);
    return ftp_run_cmd(batPath);
}

void ftp_conectar_ssh(int idx) {
    if (idx < 0 || idx >= ftpTotalServers) return;
    FtpServer* s = &ftpServers[idx];
    const char* user = s->user[0] ? s->user : "root";
    int port = s->port;
    if (port <= 0 || port == 21) port = 22;
    char cmd[3200];

    if (s->ssh_key[0]) {
        sprintf(cmd, "cmd /c start \"SSH: %s\" cmd /k \"ssh -i \"\"%s\"\" -p %d %s@%s\"",
                s->name, s->ssh_key, port, user, s->ip);
        exec_async_silent(cmd);
        return;
    }

    if (s->pass[0]) {
        char tmpDir[1024]; GetTempPathA(sizeof(tmpDir), tmpDir);
        char connPath[1200]; sprintf(connPath, "%shyperneiva_ssh_conn.bat", tmpDir);
        FILE* f = fopen(connPath, "w");
        if (f) {
            fprintf(f, "@echo off\r\n");
            fprintf(f, "set \"SSH_ASKPASS=%s\"\r\n", ftp_ssh_askpass());
            fprintf(f, "set \"SSH_ASKPASS_REQUIRE=force\"\r\n");
            fprintf(f, "set \"DISPLAY=localhost:0\"\r\n");
            fprintf(f, "echo Conectando a %s@%s (porta %d)...\r\n", user, s->ip, port);
            fprintf(f, "ssh -o StrictHostKeyChecking=accept-new -p %d %s@%s\r\n", port, user, s->ip);
            fclose(f);
        }
        sprintf(cmd, "cmd /c start \"SSH: %s\" cmd /k \"\"%s\"\"", s->name, connPath);
        exec_async_silent(cmd);
        return;
    }

    sprintf(cmd, "cmd /c start \"SSH: %s\" cmd /k \"ssh -p %d %s@%s\"", s->name, port, user, s->ip);
    exec_async_silent(cmd);
}

