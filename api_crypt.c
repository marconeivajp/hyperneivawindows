#include "api_crypt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dpapi.h>
#pragma comment(lib, "crypt32.lib")

ApiKey apiKeys[API_KEY_TOTAL];
int apiKeyTotal = 0;
int apiKeySel = 0;

void apiKeys_salvar() {
    char path[512];
    sprintf(path, "%s/api_keys.dat", "data/HyperNeiva");
    FILE* f = fopen(path, "wb");
    if (f) {
        DATA_BLOB inBlob;
        inBlob.pbData = (BYTE*)apiKeys;
        inBlob.cbData = apiKeyTotal * sizeof(ApiKey);
        DATA_BLOB outBlob;
        if (CryptProtectData(&inBlob, NULL, NULL, NULL, NULL, 0, &outBlob)) {
            fwrite(outBlob.pbData, 1, outBlob.cbData, f);
            LocalFree(outBlob.pbData);
        } else {
            fwrite(apiKeys, sizeof(ApiKey), apiKeyTotal, f);
        }
        fclose(f);
    }
}

void apiKeys_carregar() {
    apiKeyTotal = 0;
    const char* dir = "data/HyperNeiva";
    _mkdir(dir);
    char path[512];
    sprintf(path, "%s/api_keys.dat", dir);
    FILE* f = fopen(path, "rb");
    if (!f) {
        strcpy(apiKeys[apiKeyTotal].nome, "Google");
        strcpy(apiKeys[apiKeyTotal].desc, "API Google (chave generica)");
        apiKeys[apiKeyTotal].chave[0] = 0;
        apiKeyTotal++;
        strcpy(apiKeys[apiKeyTotal].nome, "Dropbox");
        strcpy(apiKeys[apiKeyTotal].desc, "API Dropbox (refresh token)");
        apiKeys[apiKeyTotal].chave[0] = 0;
        apiKeyTotal++;
        strcpy(apiKeys[apiKeyTotal].nome, "Gemini");
        strcpy(apiKeys[apiKeyTotal].desc, "API Gemini (Google)");
        apiKeys[apiKeyTotal].chave[0] = 0;
        apiKeyTotal++;
        strcpy(apiKeys[apiKeyTotal].nome, "ChatGPT");
        strcpy(apiKeys[apiKeyTotal].desc, "API OpenAI ChatGPT");
        apiKeys[apiKeyTotal].chave[0] = 0;
        apiKeyTotal++;
        apiKeys_salvar();
        return;
    }
    DATA_BLOB inBlob;
    fseek(f, 0, SEEK_END); long fsz = ftell(f); fseek(f, 0, SEEK_SET);
    if (fsz > 0) {
        BYTE* encBuf = (BYTE*)malloc(fsz);
        if (encBuf) {
            fread(encBuf, 1, fsz, f);
            DATA_BLOB outBlob;
            inBlob.cbData = fsz;
            inBlob.pbData = encBuf;
            if (CryptUnprotectData(&inBlob, NULL, NULL, NULL, NULL, 0, &outBlob)) {
                int decCount = outBlob.cbData / sizeof(ApiKey);
                if (decCount > API_KEY_TOTAL) decCount = API_KEY_TOTAL;
                memcpy(apiKeys, outBlob.pbData, decCount * sizeof(ApiKey));
                apiKeyTotal = decCount;
                LocalFree(outBlob.pbData);
            } else {
                fseek(f, 0, SEEK_SET);
                int readSize;
                while (apiKeyTotal < API_KEY_TOTAL && (readSize = (int)fread(&apiKeys[apiKeyTotal], 1, sizeof(ApiKey), f)) > 0) {
                    if (readSize == sizeof(ApiKey)) apiKeyTotal++;
                    else break;
                }
            }
            free(encBuf);
        }
    }
    fclose(f);
    if (apiKeyTotal == 0) {
        strcpy(apiKeys[apiKeyTotal].nome, "Padrao");
        apiKeys[apiKeyTotal].chave[0] = 0;
        apiKeyTotal++;
    }
}