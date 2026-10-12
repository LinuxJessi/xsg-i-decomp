#ifndef INCLUDE_MAIN_JC_FACE_H
#define INCLUDE_MAIN_JC_FACE_H

typedef struct JntWork JntWork;

typedef struct JcChain {
    unsigned short index;
    unsigned short static_index;
    void **source;
} JcChain;

void *JC_copyChain(JntWork *work, void *element, JcChain *chain);

#endif /* INCLUDE_MAIN_JC_FACE_H */
