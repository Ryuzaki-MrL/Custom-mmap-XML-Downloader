#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <3ds.h>
#include <jansson.h>

typedef struct {
    char name[21];
    char url[92];
} MMAP_LIST;

void gfxEndFrame() {
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

u32 waitKey() {
    u32 kDown = 0;
    while (aptMainLoop()) {
        hidScanInput();
        kDown = hidKeysDown();
        if (kDown) break;
        gfxEndFrame();
    }
    consoleClear();
    return kDown;
}

Result downloadFile(char* url, char* filename) {
    Result ret = 0;
    u32 statuscode = 0;
    u32 contentsize = 0;
    httpcContext context;

    ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
    if (R_FAILED(ret)) return ret;

    ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
    if(R_FAILED(ret))
    {
        httpcCloseContext(&context);
        return ret;
    }

    httpcAddRequestHeaderField(&context, "User-Agent", "mmapdownloader"); 

    ret = httpcBeginRequest(&context);
    if(R_FAILED(ret))
    {
        httpcCloseContext(&context);
        return ret;
    }

    ret = httpcGetResponseStatusCode(&context, &statuscode);
    if (statuscode != 200)
    {
        httpcCloseContext(&context);
        return statuscode;
    }

    ret = httpcGetDownloadSizeState(&context, NULL, &contentsize);
    if(R_FAILED(ret))
    {
        httpcCloseContext(&context);
        return ret;
    }

    FILE* file = fopen(filename, "wb");
    if (file == NULL)
    {
        httpcCloseContext(&context);
        return 0xFFFFFFFD;
    }

    u8* buffer = (u8*)malloc(contentsize);
    if (buffer == NULL)
    {
        httpcCloseContext(&context);
        fclose(file);
        return 0xFFFFFFFE;
    }

    ret = httpcDownloadData(&context, buffer, contentsize, &contentsize);
    if(R_FAILED(ret))
    {
        httpcCloseContext(&context);
        fclose(file);
        free(buffer);
        return ret;
    }

    fwrite(buffer, 1, contentsize, file);

    httpcCloseContext(&context);
    fclose(file);
    free(buffer);

    return 0;
}

MMAP_LIST* getJSON(u32* size) {
    Result res;
    printf("Downloading XML list... ");
    gfxEndFrame();
    res = downloadFile("https://api.github.com/repos/ihaveamac/9.6-dbgen-xmls/contents/mmap", "/mmap/cache.json");
    printf("%s %lx.\n", R_FAILED(res) ? "ERROR" : "OK", res);

    FILE* json = fopen("/mmap/cache.json", "rb");
    printf("Loading XML list... ");
    json_t* mmapjson = json_loadf(json, JSON_DECODE_ANY, NULL);
    printf("%s.\n", (mmapjson==NULL) ? "ERROR" : "OK");

    *size = json_array_size(mmapjson);
    printf("Found %lu files.\n", *size);
    MMAP_LIST* dest = (MMAP_LIST*)calloc(*size, sizeof(MMAP_LIST));
    printf("Allocating memory for XML list... %s.\n", (dest==NULL) ? "ERROR" : "OK");

    for (u16 i = 0; i < (*size); i++) {
        json_t* jsonObject = json_array_get(mmapjson, i);

        const char* NAME = json_string_value(json_object_get(jsonObject, "name"));
        memcpy(dest[i].name, NAME, 20);
        printf("%u %s\n", i, NAME);

        const char* URL = json_string_value(json_object_get(jsonObject, "download_url"));
        memcpy(dest[i].url, URL, 91);
        // printf("%u %s\n", i, URL);
    }

    fclose(json);

    return dest;
}

void downloadMissingMMAP(MMAP_LIST* source, u32 total) {
    consoleClear();
    for (u16 i = 0; i < total; i++) {
        char path[32] = {'\0'};
        sprintf(path, "/mmap/%s", source[i].name);
        FILE* f = fopen(path, "rb");
        if (f==NULL) {
            printf("Downloading %s... ", path);
            gfxEndFrame();
            Result res = downloadFile(source[i].url, path);
            printf("%s %lu\n", R_FAILED(res) ? "ERROR" : "OK", res);
        } else fclose(f);
    }
    consoleClear();
    printf("Done. Press any key to continue.\n");
    waitKey();
}

void downloadAllMMAP(MMAP_LIST* source, u32 total) {
    consoleClear();
    for (u16 i = 0; i < total; i++) {
        char path[32] = {'\0'};
        sprintf(path, "/mmap/%s", source[i].name);
        printf("Downloading %s... ", path);
        gfxEndFrame();
        Result res = downloadFile(source[i].url, path);
        printf("%s %lu\n", R_FAILED(res) ? "ERROR" : "OK", res);
    }
    consoleClear();
    printf("Done. Press any key to continue.\n");
    waitKey();
}

int main() {
    gfxInitDefault();
    httpcInit(0);
    consoleInit(GFX_TOP, NULL);

    mkdir("/mmap", 0777);

    u32 size;
    MMAP_LIST* mmaplist = getJSON(&size);

    consoleClear();

    while (aptMainLoop()) {
        printf("\x1b[0;0H\x1b[30;47m%-50s", " ");
        printf("\x1b[0;8H%s\x1b[0;0m", "*hax 2.7 mmap XML Downloader v1.0");
        printf("\x1b[1;0HPress A to download missing XML files.");
        printf("\x1b[2;0HPress B to download all XML files.");

        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_A) downloadMissingMMAP(mmaplist, size);
        else if (kDown & KEY_B) downloadAllMMAP(mmaplist, size);

        if (kDown & KEY_START) break;

        gfxEndFrame();
    }

    free(mmaplist);

    httpcExit();
    gfxExit();

    return 0;
}
