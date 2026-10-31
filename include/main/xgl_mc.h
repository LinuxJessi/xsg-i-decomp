#ifndef INCLUDE_MAIN_XGL_MC_H
#define INCLUDE_MAIN_XGL_MC_H

extern unsigned char SaveData[];

extern void xglMcSetMapName(const unsigned char *primary_euc_name,
                            const unsigned char *secondary_euc_name);

void xglMcWriteMapName(char *buffer, int slot);

typedef struct McRequestArguments {
    int slot;
    const unsigned char *path;
    int buffer;
    int size;
} McRequestArguments;

int xglMcRequest(int card, int operation, const McRequestArguments *arguments,
                 int *result);

#endif /* INCLUDE_MAIN_XGL_MC_H */
