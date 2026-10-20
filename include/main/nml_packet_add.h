#ifndef INCLUDE_MAIN_NML_PACKET_ADD_H
#define INCLUDE_MAIN_NML_PACKET_ADD_H

#include "shared.h"

#include "main/ssd_init.h"
#include "main/xgl_2.h"

/* RssdWork storage is defined by main/tu107; its layout is owned by main/tu110. */
extern RssdWorkFlags RssdWork;

extern const char D_004DC4D0[4];

typedef union GsTag {
    u64 value;
    u32 words[2];
} GsTag;

#endif /* INCLUDE_MAIN_NML_PACKET_ADD_H */
