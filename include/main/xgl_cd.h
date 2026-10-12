#ifndef INCLUDE_MAIN_XGL_CD_H
#define INCLUDE_MAIN_XGL_CD_H

#include "shared.h"

extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);

void xglClockRead(XglClock *clock);

extern int xglCdGetFileSize(const char *name);

typedef void CdCompletionCallback(int event, int value);

extern int xglCdReadFilePart(const char *name, void *buffer, int mode,
                             CdCompletionCallback *callback, int offset, int length);

#endif /* INCLUDE_MAIN_XGL_CD_H */
