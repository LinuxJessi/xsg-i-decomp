#ifndef INCLUDE_MAIN_JC_FACE_H
#define INCLUDE_MAIN_JC_FACE_H

#include "main/jnt.h"
#include "main/xgl_2.h"

typedef struct JntWork JntWork;

typedef struct JcChain {
    unsigned short index;
    unsigned short static_index;
    void **source;
} JcChain;

void *JC_copyChain(JntWork *work, void *element, JcChain *chain);

#endif /* INCLUDE_MAIN_JC_FACE_H */
