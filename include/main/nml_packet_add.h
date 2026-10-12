#ifndef INCLUDE_MAIN_NML_PACKET_ADD_H
#define INCLUDE_MAIN_NML_PACKET_ADD_H

#include "shared.h"

#include "main/ssd_init.h"

/* RssdWork storage is defined by main/tu107; its layout is owned by main/tu110. */
extern RssdWorkFlags RssdWork;

typedef union GsTag {
    u64 value;
    u32 words[2];
} GsTag;

#endif /* INCLUDE_MAIN_NML_PACKET_ADD_H */
