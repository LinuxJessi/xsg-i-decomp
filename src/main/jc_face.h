#ifndef SRC_MAIN_JC_FACE_H
#define SRC_MAIN_JC_FACE_H

/* Partial defining-owner declarations of the JNT scratchpad and matrix table. */
typedef struct JntMatrix {
    unsigned char unmodeled_00[0x40];
} JntMatrix;

typedef struct JntHairIdSource {
    unsigned char unmodeled_00[6];
    unsigned short channel_limit;
    unsigned short primary_id;
    unsigned short secondary_id;
} JntHairIdSource;

typedef struct JntWork JntWork;

struct JntWork {
    unsigned char unmodeled_000[0x28];
    JntHairIdSource *hair_id_source;
    int current_index;
    unsigned char unmodeled_030[8];
    unsigned int flags;
    unsigned char unmodeled_03c[0x7a0 - 0x3c];
    JntMatrix *matrix_buffer;
};

typedef struct JntStaticChannel JntStaticChannel;

typedef struct JcChain {
    unsigned short index;
    unsigned short static_index;
    void **source;
} JcChain;

extern void *JNT_getElement(void *elements, int index);
extern void *JNT_nextElement(void *element);
extern JntStaticChannel *JNT_getStaticVal2(JntWork *work,
                                        JntStaticChannel *channel);

void *JC_copyChain(JntWork *work, void *element, JcChain *chain);

#endif
