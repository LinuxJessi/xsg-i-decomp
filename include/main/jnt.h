#ifndef INCLUDE_MAIN_JNT_H
#define INCLUDE_MAIN_JNT_H

typedef struct JntWork JntWork;

typedef struct JntStaticChannel JntStaticChannel;

typedef struct JntProducer JntProducer;

typedef void *(*JntProductionConsumer)(JntWork *work,
                                      JntStaticChannel *channels,
                                      int argument);

int JNT_addConsumer(JntProducer *producer, int consumer_id,
                    JntProductionConsumer consumer, int argument);

typedef struct JntMoveResource {
    unsigned char unmodeled_00[0x10];
    int accessory_size;
} JntMoveResource;

/* Static-value copying uses floating point, while the matrix API transports
 * these same angle representations as raw words into VU0. */
typedef union JntRotationPayload {
    float values[3];
    int bits[3];
} JntRotationPayload;

struct JntStaticChannel {
    unsigned short type;           /* 0x00 */
    unsigned char unmodeled_02[2];
    int rotation_limits_offset;
    unsigned char unmodeled_08[6];
    unsigned short matrix_index;   /* 0x0E */
    float translate[3];            /* 0x10 */
    unsigned char unmodeled_1c[4];
    JntRotationPayload rotate;     /* 0x20 */
    unsigned char unmodeled_2C[0x14];
};

#endif /* INCLUDE_MAIN_JNT_H */
