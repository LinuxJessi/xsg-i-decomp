#include "common.h"
#include "main/xgl_studio.h"

#include "shared.h"

#include "main/xgl_packet.h"

#include "main/srs.h"

void SGsInitEnv(void);

typedef unsigned int Quadword __attribute__((mode(TI)));


typedef struct Matrix { float elements[16]; } Matrix;


typedef union SGsGifRecord {
    Quadword quad;
    unsigned long long dword[2];
    int word[4];
    unsigned short halfword[8];
    float floatValue[4];
} SGsGifRecord;


typedef struct SGsPacket {
    int count;              /* 0x0 */
    int tagIndex;           /* 0x4 */
    int loopCount;          /* 0x8 */
    union SGsGifRecord *base;     /* 0xC, starts at the scratchpad (0x70000000) */
} SGsPacket;


extern SGsPacket _packet;

typedef struct SvModelPosition {
    int x;
    int y;
    int z;
    int alpha;
} SvModelPosition;


typedef struct SvModelDrawItem {
    Matrix4 orientation;
    Matrix4 cfMatrix;
    unsigned char unmodeled_080[64];
    Vector4 origin;
    unsigned char unmodeled_0d0[48];
    Vector4 scale;
    unsigned char unmodeled_110[48];
    SvModelPosition position;
    unsigned char unmodeled_150[4];
    float scaleMultiplier;
    unsigned char unmodeled_158[10];
    unsigned char blendMode;
    unsigned char unmodeled_163[13];
    void *model;
    void *texture;
    unsigned char unmodeled_178[16];
    short renderType;
    unsigned char unmodeled_18a[2];
    unsigned int renderFlags;
    unsigned char unmodeled_190[0x280 - 0x190];
} SvModelDrawItem;


extern Matrix _defMatLc;

typedef struct SvParticleEffect {
    unsigned char unmodeled_00[4];
    const short *scriptWords;
    unsigned char unmodeled_08[40];
    Vector4 position;
    unsigned char unmodeled_40[464 - 64];
    const short *renderDomainA;
    unsigned char unmodeled_1D4[12];
    const short *renderDomainB;
    unsigned char unmodeled_1E4[0x600 - 484];
    short particleIndices[256];
    unsigned char unmodeled_800[0x812 - 0x800];
    unsigned short flags;
} SvParticleEffect;


typedef struct SvParticleDrawContext {
    Matrix4 *place;
    Vector4 *scale;
    const Quadword *textureCoordinates;
    const Quadword *gifRegisters;
    float scaleMultiplier;
    int drawVariant;
} SvParticleDrawContext;


extern Matrix4 _invView;

extern Matrix4 _svUnitMatrix;

extern const Quadword stqTbl_004CBEA0[4];

extern const Quadword _svRegData;



static int _draw3D = 0;

static int _nEffect2D = 0;

static int eft_5 = -1;

static int charID_3;

static int eftCate_4;

static int _nowImage;

/*
 * One 0x24-byte entry of the working image list svImageListCreate clears
 * and svGetImageItem indexes; only its address is ever taken in this
 * allocation, so its fields stay unmodeled.
 */

typedef struct SvImageItem {
    unsigned char unmodeled_00[0x24];
} SvImageItem;


/*
 * The single working image list: a 0x1C-byte header (untouched by this
 * allocation) followed by 0x100 SvImageItem entries (0x1C + 0x100*0x24 =
 * 0x241C, svImageListCreate's own clear size).
 */

typedef struct SvImageList {
    unsigned char unmodeled_header[0x1C];
    SvImageItem items[0x100];
} SvImageList;


SvImageList _imageList = {0};

/*
 * One 0x24-byte entry of an image mapper's item table: only the leading
 * dword svImageListDestroy clears is modeled.
 */

typedef struct SvImageMapperItem {
    int used; /* 0x00, cleared when non-zero */
    unsigned char unmodeled_04[0x24 - 4];
} SvImageMapperItem;


/*
 * The image-mapper table has a 0x1C-byte header followed by 0x100 entries
 * of 0x24 bytes. These entry fields are evidenced by the lookup routines:
 * the active word is at +0x08, the type byte at +0x0E, and the resource
 * name pointer at +0x20.
 */

typedef struct SvTypeListEntry {
    const void *data;
    unsigned char unmodeled_04[4];
    int used;
    short resourceKind;
    union {
        unsigned short typeFlags; /* +0x0e */
        struct { unsigned char type; unsigned char unmodeled_0f; };
    };
    unsigned char unmodeled_10[2];
    unsigned short basePointer;
    unsigned short bitsPerPixel;
    unsigned short bufferWidthPixels;
    unsigned short width;
    unsigned short height;
    unsigned short textureWidthExponent;
    unsigned short textureHeightExponent;
    const char *name;
} SvTypeListEntry;


typedef struct SvTypeList {
    int total;
    int size;
    int capacity;
    int savedSize;
    short width;
    short height;
    short entryCount;
    unsigned char unmodeled_16[6];
    SvTypeListEntry entries[0x100];
} SvTypeList;


typedef struct SvReferenceImageSlot {
    unsigned int words[2];
} SvReferenceImageSlot;


/* _imageMapper's symbol-table size is 40 mapper records followed by these
 * forty two-word reference-image slots. */

typedef struct SvImageMapperStorage {
    SvTypeList typeLists[40];
    SvReferenceImageSlot referenceImages[40];
} SvImageMapperStorage;


/* Forty 0x241C-byte mapper records followed by forty reference-image slots. */

SvImageMapperStorage _imageMapper = {0};

Matrix _defMatLcMod = {{0.0f}};

Matrix _defMatLn = {{0.0f}};

int strcmp(const char *, const char *);

/*
 * Only `data`, `destAddress` and `format` are evidenced (svLoadClut's three
 * field reads); the bytes between `data` and `destAddress` are an explicit
 * unmodeled span rather than an invented field.
 */

typedef struct ClutUploadRequest {
    void *data;                     /* 0x00, CLUT pixel data source address */
    unsigned char unmodeled_04[0x0E];
    unsigned short destAddress;     /* 0x12, GS local CLUT destination */
    unsigned short format;          /* 0x14, 8 => 256-entry CLUT, else 16-entry */
} ClutUploadRequest;


/*
 * Parameter roles as the SDK body (main 0x0020b208) packs them: GS
 * BITBLTBUF DBP/DPSM/DBW from basePointer/pixelFormat/bufferWidth, TRXPOS
 * DSAX/DSAY from x/y, TRXREG RRW/RRH from width/height, then `qwc`
 * quadwords of `image` sent by reference.
 */

extern void sceVif1PkRefLoadImage(XglPacket *packet,
                                  unsigned short basePointer,
                                  unsigned char pixelFormat,
                                  unsigned short bufferWidth,
                                  const void *image, unsigned int qwc,
                                  unsigned int x, unsigned int y,
                                  unsigned int width, unsigned int height);

int svLoadImageList(SvTypeList *typeList);

static int svImageListAlloc(int typeListAddr);

/*
 * One svAddScript2 sub-record, at +0x1C of a 0x24-byte slot of the type
 * list's item array. Only the fields this function itself writes are
 * modeled, at their byte offset from this sub-record's own base: `codePtr`
 * (+0x00, the script's own +0xC0 address), `active` (+0x08, always 1),
 * `kind` (+0x0C, always 5, matching this function's own "script" name),
 * `id` (+0x0E, the script's own +0xA halfword), two cleared halfwords
 * (+0x18/+0x1A) and `dataPtr` (+0x20, the script's own +0x14 address).
 * +0x20 (slot +0x3C) lands past the slot's own 0x24-byte stride, into the
 * next slot's leading bytes -- evidenced by the original bytes, not
 * treated as a bug.
 */

typedef struct SvScriptSlot {
    void *codePtr;                  /* 0x00 */
    unsigned char unmodeled_04[4];
    int active;                      /* 0x08 */
    short kind;                       /* 0x0C */
    short id;                          /* 0x0E */
    unsigned char unmodeled_10[8];
    short clearedA;                     /* 0x18 */
    short clearedB;                      /* 0x1A */
    unsigned char unmodeled_1C[4];
    void *dataPtr;                        /* 0x20 */
} SvScriptSlot;


static void svAnalyzeChunk(SvTypeList *typeList, void *chunkFile);

static SvTypeList *svGetTypeList(int type);

void svImageListDestroy(SvTypeList *typeList);

/*
 * SGsSendPacket's own address precedes SGsPacket's definition (below, before
 * SGsInitGifPacket, outside this allocation): the same four fields, at the
 * same evidenced offsets that definition documents, restated here under a
 * local tag so this allocation adds text only next to its own function.
 */

typedef struct SGsPacketReset {
    int count;               /* 0x0 */
    int tagIndex;             /* 0x4 */
    int loopCount;             /* 0x8 */
    void *base;               /* 0xC, starts at the scratchpad (0x70000000) */
} SGsPacketReset;


extern void sceVif1PkCnt(XglPacket *packet, int count);

extern void sceVif1PkAddCode(XglPacket *packet, unsigned int code);

extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data,
                                    int count);

extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);

extern void sceVif1PkTerminate(XglPacket *packet);

#define SEF_CLIP_VIEW_VOLUME_OFFSET 0x4F0

/*
 * sefDrawSchedulerEffect (still INCLUDE_ASM here) calls sefClipViewVolume and
 * branches on the returned v0, and sefClipViewVolume itself ends in a tail
 * jump to sefClipViewVolumeA (no jal, so its result is whatever
 * sefClipViewVolumeA returns): both are declared non-void here.
 */

static int sefClipViewVolumeA(void *scheduler);

static void sefInitClipViewVolume(void *view);

static void sefDrawSchedulerEffect(int schedulerIndex);

void SGsSetZTestEnv(int enable);

extern char *srsAnalyzeEftNo(int effectId, int *charId,
                            int *effectCategory);

/*
 * One 0xab0-byte record of the scheduler table sefGetScheduler returns
 * (src/main/sef.h, "The original scheduler table..."). Only the three
 * fields this function reads are named; sefIsDeadSchduler (sef.c) reads the
 * same in-use word raw through its own TU's SchedulerState, which this TU
 * cannot include (TU-local elsewhere), so this is a separate, narrower view
 * of the same bytes.
 */

typedef struct SchedulerRecord {
    unsigned char unmodeled_000[0x6b0];
    int inUse;
    unsigned char unmodeled_6b4[8];
    int transformFlagA;
    unsigned char unmodeled_6c0[4];
    int transformFlagB;
    unsigned char unmodeled_6c8[0xa78 - 0x6c8];
    short effectId;
    unsigned char unmodeled_a7a[0xa88 - 0xa7a];
    float scaleMultiplier;
    int flags;
    unsigned char unmodeled_a90[0xa9a - 0xa90];
    signed char active;
    unsigned char unmodeled_a9b[0x15];
} SchedulerRecord;


/* sef.c (main/tu211) returns unsigned char *; this prototype
 * preserves that return type across the call. */

extern unsigned char *sefGetScheduler(void);

#define SCHEDULER_COUNT 0x80

/* Draws the alters whose group (+0x0E) equals layer. */

void sdvDrawAlters(int layer);

extern int _scMslCate;

void MEfObjExec2nd(void);

static void svDrawSchedulerParticle(void);

/*
 * The SGs* family (SGsInitGifPacket, SGsOpenGifPacket, SGsCloseGifPacket,
 * SGsAddReg, SGsAddGifXYZ2, SGsSendPacket and others in this TU) shares one
 * accumulator: a growing array of 16-byte GIF records at `base`, `count`
 * records long. SGsOpenGifPacket/SGsCloseGifPacket/SGsAddReg (still
 * INCLUDE_ASM here, read for this evidence per docs/naming.md) show that
 * `tagIndex` is the record index SGsOpenGifPacket saves so
 * SGsCloseGifPacket can patch the GIFtag it opened, and `loopCount` is
 * OR-ed, unshifted, into that tag's low (NLOOP) bits by SGsCloseGifPacket;
 * both functions here leave it at 1.
 */

/*
 * One 128-bit GIF packet record. The SGs* writers fill it as a whole
 * quadword (sq), as two doublewords (sd at 0x0/0x8: a register value and
 * its A+D address) or as four words (sw, SGsAddGifXYZ2).
 */



typedef struct SvImageParameter {
    unsigned long long texture;
    float width;
    float height;
} SvImageParameter;


extern const char D_004DBA50[];

extern const char D_004DBA58[];

/*
 * GS XYZF2 vertex: X/Y are (value*16 + screen offset), Z and F are the
 * raw depth and fog values -- the PS2 SDK's own XYZ2/XYZF2 naming.
 */

#include "sv_get_image_item.h"

extern int siz_0[40];

extern int top_1[40];

extern char *srsGetEsdData(int effectId);

extern int sefSearchMapperIndex(int effectId);

unsigned int strlen(const char *string);

int strncmp(const char *left, const char *right, unsigned int length);

extern unsigned long long alphaTbl_004CBE78[4];

typedef struct SvQuadImageParameters {
    unsigned char unmodeled_00[8];
    unsigned long long texture;
    float halfWidth;
    float halfHeight;
} SvQuadImageParameters;

typedef struct SvQuadParticle {
    Matrix4 localTransform;
    unsigned char unmodeled_40[248 - 64];
    float angle;
    unsigned char unmodeled_FC[320 - 252];
    SGsGifRecord packedColor;
    unsigned char unmodeled_150[18];
    unsigned char blendMode;
    unsigned char unmodeled_163[368 - 355];
    SvQuadImageParameters imageParameters;
    unsigned char unmodeled_188[576 - 392];
    Vector4 planeOffset;
    unsigned char unmodeled_250[640 - 592];
} SvQuadParticle;

static inline const SvQuadImageParameters *svPrepareParticleImage(
    SGsGifRecord *header, SvQuadParticle *particle, Quadword gifRegisters,
    unsigned int textureRegister, unsigned int alphaRegister)
{
    const SvQuadImageParameters *image = &particle->imageParameters;
    unsigned long long textureValue;

    header->quad = gifRegisters;
    textureValue = image->texture;
    header[1].dword[1] = textureRegister;
    header[1].dword[0] = textureValue;
    header[2].dword[0] = alphaTbl_004CBE78[particle->blendMode];
    header[2].dword[1] = alphaRegister;
    header[3].quad = particle->packedColor.quad;
    return image;
}

typedef struct SvSpriteParticle {
    unsigned char unmodeled_00[0xC0];
    float centerX;
    float centerY;
    unsigned char unmodeled_C8[0xF8 - 0xC8];
    float angle;
    unsigned char unmodeled_FC[4];
    float scaleX;
    float scaleY;
    unsigned char unmodeled_108[0x140 - 0x108];
    SGsGifRecord packedColor;
    unsigned char unmodeled_150[18];
    unsigned char blendMode;
    unsigned char unmodeled_163[0x178 - 0x163];
    unsigned long long texture;
    float halfWidth;
    float halfHeight;
    unsigned char unmodeled_188[0x240 - 0x188];
    float offsetX;
    float offsetY;
    unsigned char unmodeled_248[0x280 - 0x248];
} SvSpriteParticle;

typedef struct SvSpriteVertex {
    float x;
    float y;
} SvSpriteVertex;

static inline float svSpriteExtentPixels(float extent, float scale)
{
    float scaledExtent = extent * scale;
    return scaledExtent * 100.0f;
}

static inline void svAppendSpriteVertex(SGsPacket *packet,
                                         const Quadword *textureCoordinates,
                                         const SvSpriteVertex *vertex,
                                         float centerX, float centerY)
{
    int screenX;
    int screenY;

    packet->base[packet->count].quad = *textureCoordinates;
    packet->count++;
    screenX = (int)(centerX + vertex->x) * 16 + 0x7000;
    screenY = (int)(centerY + vertex->y) * 16 + 0x7200;
    packet->base[packet->count].word[0] = screenX;
    packet->base[packet->count].word[1] = screenY;
    packet->base[packet->count].word[2] = 0xFFFFFA;
    packet->base[packet->count].word[3] = 0;
    packet->count++;
}

extern StudioLight _defLight;

extern StudioLight *xglStudioGetLight2(void);

extern void xglLightCalcMatrix(StudioLight *light);

typedef struct StudioLightMatrixView {
    Vector4 ambient_color;
    StudioLightEntry lights[3];
    Quadword matrices[8];
} StudioLightMatrixView;

typedef struct SvParticleListCfContext {
    Matrix4 *place;
    Vector4 *scaledScale;
    const Quadword *textureCoordinates;
    const Quadword *gifRegisters;
    float scaleMultiplier;
    int drawVariant;
    unsigned char unmodeled_18[8];
    int schedulerVariant;
} SvParticleListCfContext;

typedef struct SvSchedulerEffectGroup {
    signed char effectIds[0x20];
    unsigned char unmodeled_20[0x0C];
    short effectCount;
    unsigned char unmodeled_2e[2];
} SvSchedulerEffectGroup;

typedef struct SvSchedulerEffectRecord {
    Quadword transform[4];
    Quadword localTransform[4];
    Quadword packetInfo;
    unsigned char unmodeled_90[0x10];
    float localScale;
    unsigned char unmodeled_a4[0x0C];
    SvSchedulerEffectGroup groups[0x20];
    int inUse;
    unsigned char unmodeled_6b4[8];
    void *transformFlagA;
    unsigned char unmodeled_6c0[4];
    void *transformFlagB;
    unsigned char unmodeled_6c8[0x3C0];
    float scaleMultiplier;
    unsigned int flags;
    unsigned char unmodeled_a90[10];
    signed char active;
    unsigned char unmodeled_a9b[0x15];
} SvSchedulerEffectRecord;

typedef struct SvBattleEffectData {
    unsigned char unmodeled_00[0x30];
    Quadword packetInfo;
    unsigned char unmodeled_40[0x1DC];
    float scaleMultiplier;
    unsigned char unmodeled_220[4];
    unsigned int flags;
    unsigned char unmodeled_228[4];
    unsigned int partialFlags;
} SvBattleEffectData;

extern Vector4 *MMathApplyMatrix(Vector4 *out, const Matrix4 *matrix, const Vector4 *vector);

extern Matrix4 *MMathMulMatrix(Matrix4 *out, const Matrix4 *left, const Matrix4 *right);

extern unsigned char *sefGetBattleData(void);

extern unsigned char *sefGetLineAdr(int lineIndex);

extern float sefGetMatrixScale(float *matrix);

extern StudioCamera *xglStudioGetActiveCamera(void);

typedef struct SvImageChunk {
    unsigned int byteSize;
    unsigned int storageSize;
    short kind;
    unsigned short typeFlags;
    short width;
    unsigned short height;
    unsigned short bitsPerPixel;
    unsigned char unmodeled_12[2];
    char name[0x2c];
    char data[1];
} SvImageChunk;

typedef struct SvClutChunk {
    unsigned char unmodeled_00[10];
    unsigned short typeFlags;
    short width;
    unsigned short height;
    unsigned short bitsPerPixel;
    unsigned char unmodeled_12[2];
    char name[44];
    unsigned char data[1];
} SvClutChunk;

void *svGetImageItem(int index) {
    return &_imageList.items[index];
}

void svImageListCreate(void) {
    memset(&_imageList, 0, sizeof(SvImageList));
}

void svImageListDestroy(SvTypeList *typeList)
{
  SvTypeList *header = typeList;
  SvImageMapperItem *item = (SvImageMapperItem *) (((unsigned char *) typeList) + 0x24);
  int count = 0xFF;

  do
  {
    if (item->used != 0)
    {
      item->used = 0;
    }
    count -= 1;
    item += 1;
  }
  while (count >= 0);

  header->savedSize = header->size;
  header->total = 0;
  header->width = 0;
  header->height = 0;
  header->entryCount = 0;
}

static int svImageListAlloc(int typeListAddr)
{
    SvTypeList *typeList;
    int index;

    typeList = (SvTypeList *)typeListAddr;
    for (index = 0; index < 0x100; index++) {
        if (typeList->entries[index].used == 0) {
            return index;
        }
    }
    return -1;
}

static SvTypeList * svGetTypeList(int type) {
    return &_imageMapper.typeLists[type];
}

static void *svGetImageListItemSub(int type, unsigned int flags)
{
    SvTypeList *typeList;
    SvTypeList *storage;
    SvTypeListEntry *entry;
    int index;

    typeList = svGetTypeList(type);
    storage = typeList;
    entry = storage->entries;
    index = 0;
    while (index < 0x100) {
        if (entry->used != 0 && entry->type == (unsigned char)flags) {
            return entry;
        }
        entry++;
        index++;
    }
    return 0;
}

void *svGetImageListItem(int type, unsigned int flags)
{
    int typeIndex;

    if ((flags & 0x100) != 0) {
        typeIndex = 0;
    } else if ((flags & 0x200) != 0) {
        typeIndex = type;
    } else if ((flags & 0x400) != 0) {
        typeIndex = type;
    } else if ((flags & 0x800) != 0) {
        typeIndex = 15;
    } else if ((flags & 0x1000) != 0) {
        typeIndex = type;
    } else {
        typeIndex = 14;
    }
    return svGetImageListItemSub(typeIndex, flags);
}

static SvTypeListEntry *svGetResFromName(const char *name)
{
    SvTypeList *typeList;
    SvTypeList *storage;
    SvTypeListEntry *entry;
    int index;

    typeList = svGetTypeList(0);
    storage = typeList;
    entry = storage->entries;
    index = 0;
    while (index < 0x100) {
        if (entry->used != 0 && strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry++;
        index++;
    }
    return 0;
}

SvImageParameter *svGetPrmFromName(const char *baseName,
                                   SvImageParameter *parameter)
{
    char resourceName[128];
    SvTypeListEntry *bitmap;
    SvTypeListEntry *clut;

    sprintf(resourceName, D_004DBA50, baseName);
    bitmap = svGetResFromName(resourceName);
    sprintf(resourceName, D_004DBA58, baseName);
    clut = svGetResFromName(resourceName);

    if (bitmap != 0) {
        if (clut != 0) {
            unsigned long long baseBits;
            unsigned long long dimensionBits;
            unsigned long long formatBits;
            unsigned long long clutBits;
            int pixelFormat;
            unsigned short clutBasePointer;
            unsigned short bitsPerPixel;
            float scaledWidth;

            clutBasePointer = clut->basePointer;
            baseBits = (unsigned long long)bitmap->basePointer |
                       0x2000000000000000ULL;
            dimensionBits =
                ((unsigned long long)bitmap->textureWidthExponent << 26) |
                0x400000000ULL;
            pixelFormat = 20;
            bitsPerPixel = bitmap->bitsPerPixel;
            if (bitsPerPixel == 8) {
                pixelFormat = 19;
            }
            formatBits =
                ((unsigned long long)pixelFormat << 20) |
                ((unsigned long long)(bitmap->bufferWidthPixels >> 6) << 14);
            clutBits =
                ((unsigned long long)bitmap->textureHeightExponent << 30) |
                ((unsigned long long)clutBasePointer << 37);
            scaledWidth = bitmap->width * 0.01f;
            parameter->height = bitmap->height * 0.01f;
            parameter->width = scaledWidth;
            parameter->texture = (baseBits | formatBits) |
                                 (dimensionBits | clutBits);
            return parameter;
        }
    }

    memset(parameter, 0, 16);
    return 0;
}

void svLoadTexture(SvTypeListEntry *image)
{
    XglPacket *packet;
    unsigned short basePointer;
    unsigned short height;
    unsigned short width;
    unsigned short bitsPerPixel;

    basePointer = image->basePointer;
    height = image->height;
    width = image->width;
    packet = xglPacketGetCurrent();
    bitsPerPixel = image->bitsPerPixel;

    if (bitsPerPixel == 8) {
        unsigned short bufferWidth;
        int qwc;

        bufferWidth = image->bufferWidthPixels;
        qwc = bufferWidth * height;
        if (qwc < 0) {
            qwc += 15;
        }
        sceVif1PkRefLoadImage(packet, basePointer, 19,
                              bufferWidth >> 6, image->data,
                              (unsigned int)(qwc >> 4), 0, 0, width, height);
    } else {
        unsigned short bufferWidth;
        int qwc;

        bufferWidth = image->bufferWidthPixels;
        qwc = bufferWidth * height;
        if (qwc < 0) {
            qwc += 15;
        }
        sceVif1PkRefLoadImage(packet, basePointer, 20,
                              bufferWidth >> 6, image->data,
                              (unsigned int)(qwc >> 4), 0, 0, width, height);
    }
}

void svLoadClut(ClutUploadRequest *clut)
{
    XglPacket *packet = xglPacketGetCurrent();
    unsigned short dest = clut->destAddress;

    if (clut->format == 8) {
        sceVif1PkRefLoadImage(packet, dest, 0, 1, clut->data, 0x40, 0, 0,
                              0x10, 0x10);
    } else {
        sceVif1PkRefLoadImage(packet, dest, 0, 1, clut->data, 4, 0, 0, 8, 2);
    }
}

int svLoadImageList(SvTypeList *typeList)
{
    int textureCount = 0;
    int index = 0;

    for (; index < 256; index++) {
        int used = typeList->entries[index].used;
        int kind;
        if (used == 0) {
            continue;
        }
        kind = typeList->entries[index].resourceKind;
        if (kind == 4) {
            textureCount++;
            svLoadTexture(&typeList->entries[index]);
            continue;
        }
        if (kind == 3) {
            svLoadClut((ClutUploadRequest *)&typeList->entries[index]);
        }
    }
    return textureCount;
}

s16 svLoadMapperList(void)
{
    s16 result;
    int type;
    SvTypeList *typeLists = _imageMapper.typeLists;

    for (type = 0; type < 40; type++) {
        /* Preserve the original loop's separate entry-count pointer. */
        const s16 *entryCount = &typeLists[type].entryCount;
        result = *entryCount;
        if (result > 0) {
            result = svLoadImageList(&_imageMapper.typeLists[type]);
        }
    }
    return result;
}

static int svGetSizeBit(unsigned int value)
{
    unsigned int mask;
    int bitIndex;

    mask = 2;
    bitIndex = 1;
    while (bitIndex < 32) {
        if ((value & mask) != 0) {
            return bitIndex;
        }
        mask <<= 1;
        bitIndex++;
    }
    return 0;
}

static int svAddImage(SvTypeList *typeList, void *chunk)
{
    int index;
    int storageSize;
    unsigned short typeFlags;
    SvTypeListEntry *entry;
    SvImageChunk *image;

    index = svImageListAlloc((int)typeList);
    if (index < 0) {
        return -1;
    }

    entry = &typeList->entries[index];
    entry->data = (unsigned char *)chunk + 0x40;
    entry->used = 1;
    entry->resourceKind = 4;
    image = chunk;
    typeFlags = image->typeFlags;
    entry->typeFlags = typeFlags;
    entry->width = image->width;
    entry->height = image->height;
    entry->bitsPerPixel = image->bitsPerPixel;
    entry->textureWidthExponent = svGetSizeBit(entry->width);
    entry->textureHeightExponent = svGetSizeBit(entry->height);
    entry->name = image->name;

    storageSize = image->storageSize;
    if ((short)image->bitsPerPixel == 4) {
        entry->bufferWidthPixels = image->width >> 1;
        if ((int)entry->height * 2 < (int)entry->width) {
            storageSize *= (int)entry->width / ((int)entry->height * 2);
        }
    } else if ((unsigned int)entry->width < (unsigned int)entry->height) {
        entry->bufferWidthPixels = image->width * (int)entry->height / (int)entry->width;
    } else {
        entry->bufferWidthPixels = (unsigned short)image->width;
    }

    entry->basePointer = (unsigned int)typeList->savedSize >> 6;
    typeList->savedSize += (unsigned int)storageSize >> 2;
    return index;
}

static int svAddClut(SvTypeList *typeList, void *const chunkData)
{
    const SvClutChunk *const chunk = chunkData;
    SvTypeListEntry *entry;
    int index;

    index = svImageListAlloc((int)typeList);
    if (index < 0) {
        return -1;
    }
    entry = &typeList->entries[index];
    entry->data = chunk->data;
    entry->used = 1;
    entry->resourceKind = 3;
    entry->typeFlags = chunk->typeFlags;
    entry->width = chunk->width;
    entry->height = chunk->height;
    entry->bitsPerPixel = chunk->bitsPerPixel;
    entry->textureWidthExponent = svGetSizeBit(entry->width);
    entry->textureHeightExponent = svGetSizeBit(entry->height);
    {
        const char *name = chunk->name;
        const int savedSize = typeList->savedSize;
        const short bufferWidthPixels = chunk->width * 4;
        const unsigned short basePointer = (unsigned int)savedSize >> 6;

        entry->bufferWidthPixels = bufferWidthPixels;
        entry->name = name;
        typeList->savedSize = entry->bitsPerPixel == 8 ? savedSize + 256 : savedSize + 128;
        entry->basePointer = basePointer;
    }
    return index;
}

typedef struct SvResourceChunk {
    unsigned int byteSize;
    unsigned char unmodeled_04[4];
    short kind;
    unsigned short typeFlags;
    char payload[0x34];
} SvResourceChunk;
static int svAddModel(SvTypeList *typeList, SvResourceChunk *chunk)
{
    SvTypeListEntry *entry;
    unsigned short typeFlags;
    int index;

    index = svImageListAlloc((int)typeList);
    if (index < 0) {
        return -1;
    }
    entry = &typeList->entries[index];
    entry->resourceKind = 6;
    entry->name = &chunk->payload[8];
    typeFlags = chunk->typeFlags | 0x2000;
    entry->data = &chunk->payload[sizeof(chunk->payload)];
    entry->typeFlags = typeFlags;
    entry->used = 1;
    entry->height = 0;
    entry->width = 0;
    return index;
}

static int svAddScript(SvTypeList *typeList, SvResourceChunk *chunk)
{
    SvTypeListEntry *entry;
    unsigned short typeFlags;
    int index;

    index = svImageListAlloc((int)typeList);
    if (index < 0) {
        return -1;
    }
    entry = &typeList->entries[index];
    entry->resourceKind = 5;
    entry->name = &chunk->payload[8];
    typeFlags = chunk->typeFlags;
    entry->data = &chunk->payload[sizeof(chunk->payload)];
    entry->typeFlags = typeFlags;
    entry->used = 1;
    entry->height = 0;
    entry->width = 0;
    return index;
}

static int svAddScript2(int typeListAddr, void *script)
{
  int index;
  void *codePtr;
  void *dataPtr;
  void *slot;
  SvScriptSlot *sub;

  index = svImageListAlloc(typeListAddr);
  codePtr = (unsigned char *) script + 0xC0;
  dataPtr = (unsigned char *) script + 0x14;
  slot = (void *) (typeListAddr + (index * 0x24));
  if (index < 0)
  {
    return -1;
  }
  sub = (SvScriptSlot *) ((unsigned char *) slot + 0x1C);
  sub->kind = 5;
  sub->dataPtr = dataPtr;
  sub->codePtr = codePtr;
  sub->id = (short) *((u16 *) (((unsigned char *) script) + 0xA));
  sub->active = 1;
  sub->clearedB = 0;
  sub->clearedA = 0;
  return index;
}



typedef struct SvResourceChunkFile {
    struct SvResourceChunk *firstChunk;
} SvResourceChunkFile;


static int svAddImage(SvTypeList *typeList, void *chunk);
static int svAddClut(SvTypeList *typeList, void *chunk);
static int svAddModel(SvTypeList *typeList, SvResourceChunk *chunk);
static int svAddScript(SvTypeList *typeList, SvResourceChunk *chunk);

static void svAnalyzeChunk(SvTypeList *typeList, void *chunkFile)
{
    SvResourceChunkFile *file = chunkFile;
    SvResourceChunk *chunk = file->firstChunk;

    while (chunk->kind != 0) {
        switch (chunk->kind) {
        case 1:
        case 2:
            chunk++;
            continue;
        case 4:
            svAddImage(typeList, chunk);
            chunk = (SvResourceChunk *)((unsigned char *)chunk + chunk->byteSize);
            break;
        case 3:
            svAddClut(typeList, chunk);
            chunk = (SvResourceChunk *)((unsigned char *)chunk + chunk->byteSize);
            break;
        case 7:
            svAddImage(typeList, chunk);
            chunk = (SvResourceChunk *)((unsigned char *)chunk + chunk->byteSize);
            break;
        case 6:
        case 9:
            svAddModel(typeList, chunk);
            chunk = (SvResourceChunk *)((unsigned char *)chunk + chunk->byteSize);
            break;
        case 5:
            svAddScript(typeList, chunk);
            chunk = (SvResourceChunk *)((unsigned char *)chunk + chunk->byteSize);
            break;
        case 10:
            svAddScript2((int)typeList, chunk);
            chunk = (SvResourceChunk *)((unsigned char *)chunk + chunk->byteSize);
            break;
        default:
            return;
        }
    }
}

void svInitImageMapper(void)
{
    int index;

    memset(&_imageMapper, 0, sizeof(_imageMapper));
    for (index = 0; index < 40; index++) {
        int initialTop = top_1[index];
        int initialCapacity;

        initialCapacity = siz_0[index];
        _imageMapper.typeLists[index].size = initialTop;
        _imageMapper.typeLists[index].capacity = initialCapacity;
        _imageMapper.typeLists[index].savedSize = initialTop;
        _imageMapper.typeLists[index].entryCount = 0;
        _imageMapper.typeLists[index].width = 0;
        _imageMapper.typeLists[index].height = 0;
    }
}

static void svInitRefImage(void)
{
    SvImageMapperStorage *storage;
    int index;

    storage = &_imageMapper;
    for (index = 0; index < 40; index++) {
        storage->referenceImages[index].words[0] = 0;
        storage->referenceImages[index].words[1] = 0;
    }
}

void svAddImageMapper(int type, int width, void *chunk, int height) {
    SvTypeList *typeList;
    SvTypeList *header;

    typeList = svGetTypeList(type);
    header = typeList;
    if (header->entryCount != 0) {
        svImageListDestroy(typeList);
    }
    header->height = (short) height;
    header->width = (short) width;
    header->entryCount = (short) ((u16) header->entryCount + 1);
    svAnalyzeChunk(typeList, chunk);
}

void svDeleteImageMapper(int type)
{
    SvTypeList *typeList = svGetTypeList(type);

    if (typeList->entryCount != 0) {
        svImageListDestroy(typeList);
    }
}

void svDeleteImageMapperID(int id)
{
    int index;

    for (index = 2; index < 14; index++) {
        if (_imageMapper.typeLists[index].width == id) {
            svImageListDestroy(&_imageMapper.typeLists[index]);
            return;
        }
    }
}

void svDeleteImageMapperData(int data)
{
    int index;

    for (index = 2; index < 14; index++) {
        if (_imageMapper.typeLists[index].height == data) {
            svImageListDestroy(&_imageMapper.typeLists[index]);
            return;
        }
    }
}

int svFileLoadScript(int cfId, int effectNo)
{
    int charNo;
    int effectType;
    SrsMemRes *resource;
    int result;
    int loadedSize;

    srsAnalyzeEftNo(effectNo, &charNo, &effectType);
    if (effectType == SRS_RES_EFFECT) {
        return srsFileLoadCf(cfId, effectNo);
    }
    result = -1;
    if (effectType == SRS_RES_BATTLE_IMAGE) {
        if (_srsMemRes.battleImage != 0) {
            int previousEffect = _srsMemRes.no[SRS_RES_BATTLE_IMAGE];
            if (effectNo == previousEffect) {
                return SRS_RES_BATTLE_IMAGE;
            }
            if (!sefIsFinishEffect2(previousEffect)) {
                sefGetLoadQue();
                sefSetLoadQue(effectNo);
                return -1;
            }
            svDeleteImageMapper(SRS_RES_BATTLE_IMAGE);
        }
        resource = &_srsMemRes;
        sefSetLoadQue(0);
        if (resource->battleImage == 0) {
            resource->battleImage = smAlloc(0x40000);
            if (resource->battleImage == 0) {
                return -1;
            }
        }
        resource->no[SRS_RES_BATTLE_IMAGE] = effectNo;
        resource->pending[SRS_RES_BATTLE_IMAGE] = 1;
        loadedSize = srsLoadEffectData(resource->battleImage, effectNo);
        if (loadedSize < 0) {
            resource->pending[SRS_RES_BATTLE_IMAGE] = 0;
            resource->no[SRS_RES_BATTLE_IMAGE] = 0;
            return -1;
        }
        return SRS_RES_BATTLE_IMAGE;
    }
    return result;
}


SvTypeListEntry *svGetScript(int effectId)
{
    const char *resourceName = srsGetEsdData(effectId);
    int mapperIndex;
    SvTypeList *typeList;
    int index;

    if (resourceName == 0) {
        return 0;
    }
    mapperIndex = sefSearchMapperIndex(effectId);
    typeList = &_imageMapper.typeLists[mapperIndex];
    if (mapperIndex == 14) {
        if (typeList->entries[0].used != 0 &&
            typeList->entries[0].resourceKind != 0) {
            return typeList->entries;
        }
    }
    for (index = 0; index < 256; index++) {
        if (typeList->entries[index].used != 0 &&
            typeList->entries[index].resourceKind == 5 &&
            typeList->entries[index].name != (const char *)-1) {
            if (strncmp(resourceName, typeList->entries[index].name, strlen(resourceName)) == 0) {
                return &typeList->entries[index];
            }
        }
    }
    return 0;
}

void SGsSendPacket(SGsPacketReset *packet)
{
    XglPacket *vifPacket;

    if (packet->count != 0) {
        vifPacket = xglPacketGetCurrent();
        sceVif1PkCnt(vifPacket, 0);
        sceVif1PkAddCode(vifPacket, 0x11000000);
        sceVif1PkOpenDirectHLCode(vifPacket, 0);
        sceVif1PkAddDirectDataN(vifPacket, packet->base, packet->count);
        sceVif1PkCloseDirectHLCode(vifPacket);
        sceVif1PkTerminate(vifPacket);
        packet->count = 0;
        packet->tagIndex = 0;
        packet->loopCount = 1;
        packet->base = (void *)0x70000000;
    }
}

extern void nmlModelSetLight(void *localModelMatrix, void *lightMatrix);

extern void nmlModelSetToumei(int enabled);

extern void nmlModelSetAlpha(unsigned long long alpha);

extern void nmlModelSetTexture(void *texture);

extern void nmlModelSetPlace(void *place);

extern void nmlModelEntryEffect(void *model);

extern const unsigned long long alphaTbl_2[3];

static inline void svResetTextureFlushPacket(SGsPacket *packet)
{
    packet->tagIndex = 0;
    packet->loopCount = 1;
    packet->base = (SGsGifRecord *)0x70000000;
    packet->count = 0;
}

static inline void svResetGsPacket(SGsPacket *packet)
{
    packet->loopCount = 1;
    packet->base = (SGsGifRecord *)0x70000000;
    packet->tagIndex = 0;
    packet->count = 0;
}

static inline void svAppendGsPacketHeader(SGsPacket *packet,
                                         unsigned long long tag,
                                         const unsigned long long registers)
{
    packet->base[packet->count].dword[0] = tag;
    packet->base[packet->count].dword[1] = registers;
    packet->count++;
}

static inline void svAppendGsAddressData(SGsPacket *packet,
                                         unsigned long long address,
                                         unsigned long long value)
{
    packet->base[packet->count].dword[0] = value;
    packet->base[packet->count].dword[1] = address;
    packet->count++;
}

static inline void svBeginGsRegisterPacket(SGsPacket *packet,
                                           SGsGifRecord *scratchpad,
                                           unsigned short loopCount)
{
    unsigned long long gifRegisters = 14;
    unsigned long long gifTag = 0x1000000000008000ULL | loopCount;

    packet->count = 1;
    packet->base = scratchpad;
    scratchpad[0].dword[0] = gifTag;
    scratchpad[0].dword[1] = gifRegisters;
}

static inline void svAppendFirstGsRegister(SGsPacket *packet,
                                           unsigned int address,
                                           unsigned long long value)
{
    packet->base[packet->count].dword[0] = value;
    packet->loopCount = 1;
    packet->tagIndex = 0;
    packet->base[packet->count].dword[1] = address;
    packet->count++;
}

static inline void svAppendGsRegister(SGsPacket *packet,
                                      unsigned long long value,
                                      unsigned int address)
{
    packet->base[packet->count].dword[0] = value;
    packet->base[packet->count].dword[1] = address;
    packet->count++;
}

static inline unsigned long long svPackModelAlpha(unsigned char blendMode,
                                                 int alphaValue)
{
    unsigned long long alphaHigh = (unsigned long long)alphaValue << 32;
    return alphaTbl_2[blendMode] | alphaHigh;
}

void SGsTexFlush(void)
{
    SGsPacket *packet = &_packet;
    unsigned long long gifRegisters;
    unsigned long long gifTag;

    svResetTextureFlushPacket(packet);
    gifRegisters = 14;
    gifTag = 0x1000000000008001ULL;
    svAppendGsPacketHeader(packet, gifTag, gifRegisters);
    svAppendGsAddressData(packet, 63, 1);
    SGsSendPacket((SGsPacketReset *)packet);
}

void SGsInitEnv(void)
{
    SGsGifRecord *scratchpad = (SGsGifRecord *)0x70000000;
    SGsPacket *packet = &_packet;
    StudioLight *light;
    unsigned long long gifRegisters;
    unsigned long long gifTag;
    int row;
    int column;
    float *rowStart;

    gifRegisters = 14;
    gifTag = 0x1000000000008001ULL;
    packet->tagIndex = 0;
    packet->loopCount = 1;
    packet->count = 1;
    packet->base = scratchpad;
    scratchpad[0].dword[0] = gifTag;
    scratchpad[0].dword[1] = gifRegisters;
    svAppendGsAddressData(packet, 63, 1);
    svAppendGsRegister(packet, 0x1000000000008004ULL, gifRegisters);
    svAppendGsAddressData(packet, 71, 0x5000D);
    svAppendGsRegister(packet, 96, 20);
    svAppendGsRegister(packet, 5, 8);
    svAppendGsRegister(packet, 0x131000000ULL, 78);
    SGsSendPacket((SGsPacketReset *)packet);

    light = xglStudioGetLight2();
    *(StudioLightMatrixView *)&_defLight = *(StudioLightMatrixView *)light;

    xglLightCalcMatrix(light);
    __asm__ __volatile__(
        "lq $8, 0(%1)\n\t"
        "lq $9, 16(%1)\n\t"
        "lq $10, 32(%1)\n\t"
        "lq $11, 48(%1)\n\t"
        "sq $8, 0(%0)\n\t"
        "sq $9, 16(%0)\n\t"
        "sq $10, 32(%0)\n\t"
        "sq $11, 48(%0)"
        : : "r"(&_defMatLcMod), "r"(&light->unmodeled_70[0x40])
        : "$8", "$9", "$10", "$11", "memory");
    __asm__ __volatile__(
        "lq $8, 0(%1)\n\t"
        "lq $9, 16(%1)\n\t"
        "lq $10, 32(%1)\n\t"
        "lq $11, 48(%1)\n\t"
        "sq $8, 0(%0)\n\t"
        "sq $9, 16(%0)\n\t"
        "sq $10, 32(%0)\n\t"
        "sq $11, 48(%0)"
        : : "r"(&_defMatLc), "r"(&light->unmodeled_70[0x40])
        : "$8", "$9", "$10", "$11", "memory");
    __asm__ __volatile__(
        "lq $8, 0(%1)\n\t"
        "lq $9, 16(%1)\n\t"
        "lq $10, 32(%1)\n\t"
        "lq $11, 48(%1)\n\t"
        "sq $8, 0(%0)\n\t"
        "sq $9, 16(%0)\n\t"
        "sq $10, 32(%0)\n\t"
        "sq $11, 48(%0)"
        : : "r"(&_defMatLn), "r"(&light->unmodeled_70[0])
        : "$8", "$9", "$10", "$11", "memory");

    rowStart = _defMatLc.elements;
    for (row = 0; row < 3; row++) {
        for (column = 2; column >= 0; column--) {
            rowStart[column] = 0.0f;
        }
        rowStart += 4;
    }
}

void SGsSetZTestEnv(int enable)
{
    SGsPacket *packet = &_packet;
    unsigned long long gifRegisters;
    unsigned long long gifTag;

    svResetGsPacket(packet);
    gifRegisters = 14;
    gifTag = 0x1000000000008001ULL;
    svAppendGsPacketHeader(packet, gifTag, gifRegisters);
    if (enable != 0) {
        svAppendGsAddressData(packet, 71, 0x5000d);
    } else {
        svAppendGsAddressData(packet, 71, 0x2000d);
    }
    SGsSendPacket((SGsPacketReset *)&_packet);
}

void SGsRestoreEnv(void)
{
    SGsGifRecord *scratchpad = (SGsGifRecord *)0x70000000;
    SGsPacket *packet = &_packet;

    svBeginGsRegisterPacket(packet, scratchpad, 2);
    svAppendFirstGsRegister(packet, 78, 0x31000000);
    svAppendGsRegister(packet, 0, 20);
    SGsSendPacket((SGsPacketReset *)packet);
    nmlModelSetLight(&_defMatLcMod, &_defMatLn);
}

static void sefInitClipViewVolume(void *view) {
    __asm__ __volatile__("lqc2 vf12, 0(%0)\n\tlqc2 vf13, 16(%0)\n\tlqc2 vf14, 32(%0)\n\tlqc2 vf15, 48(%0)\n\tlqc2 vf16, 64(%0)\n\tlqc2 vf17, 80(%0)\n\tlqc2 vf18, 96(%0)\n\tlqc2 vf19, 112(%0)\n\tnop" :  : "r"(view) : "memory");
}

int sefClipViewVolumeA(void *scheduler)
{
    int outside;

    __asm__ __volatile__(
        "lqc2 vf20, 0(%1)\n\t"
        "vaddw.x vf2, vf0, vf20w\n\t"
        "qmfc2 $4, vf2\n\t"
        "mtc1 $4, $f1\n\t"
        "vmulax.xyzw ACC, vf16, vf20x\n\t"
        "vmadday.xyzw ACC, vf17, vf20y\n\t"
        "vmaddaz.xyzw ACC, vf18, vf20z\n\t"
        "vmaddw.xyzw vf9, vf19, vf0w\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "vmulax.xyzw ACC, vf12, vf20x\n\t"
        "vmadday.xyzw ACC, vf13, vf20y\n\t"
        "vmaddaz.xyzw ACC, vf14, vf20z\n\t"
        "vmaddw.xyzw vf9, vf15, vf0w\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "sub $4, $4, $4\n\t"
        "j 2f\n\t"
        "nop\n\t"
        "1:\n\t"
        "sub $4, $4, $4\n\t"
        "addi $4, $4, 1\n\t"
        "2:\n\t"
        "move %0, $4"
        : "=r"(outside)
        : "r"(scheduler)
        : "$4", "$f0", "$f1", "memory");
    return outside;
}

int sefClipViewVolume(void *scheduler, void *effect)
{
    sefInitClipViewVolume((unsigned char *)effect + SEF_CLIP_VIEW_VOLUME_OFFSET);
    return sefClipViewVolumeA(scheduler);
}

void sefDrawParticle2D(SvSpriteParticle *particle, SGsPacket *packet,
                      const Quadword textureCoordinates[4])
{
    float scaleX;
    float scaleY;
    float halfWidth;
    float halfHeight;
    float centerX;
    float centerY;
    float offsetX;
    float offsetY;
    float angle;
    float sine;
    float cosine;
    float cornerX;
    float cornerY;
    SvSpriteVertex vertex;
    unsigned long long gifRegisters;
    unsigned long long gifTag;

    gifRegisters = 0x0525252521EEULL;
    packet->loopCount = 1;
    packet->tagIndex = packet->count;
    packet->base[packet->count].dword[1] = gifRegisters;
    gifTag = 0xB02A400000008000ULL;
    packet->base[packet->count].dword[0] = gifTag;
    packet->count++;
    packet->base[packet->count].dword[0] = particle->texture;
    packet->base[packet->count].dword[1] = 6;
    packet->count++;
    packet->base[packet->count].dword[0] = alphaTbl_004CBE78[particle->blendMode];
    packet->base[packet->count].dword[1] = 66;
    packet->count++;
    packet->base[packet->count].quad = particle->packedColor.quad;
    packet->count++;

    scaleX = particle->scaleX;
    scaleY = particle->scaleY;
    halfWidth = particle->halfWidth * scaleX;
    halfHeight = svSpriteExtentPixels(particle->halfHeight, scaleY);
    centerX = particle->centerX * 10.0f;
    centerY = particle->centerY * 10.0f;
    offsetX = particle->offsetX;
    offsetY = particle->offsetY;
    centerX += 256.0f;
    centerY += 224.0f;
    offsetX *= scaleX;
    offsetY *= scaleY;
    halfWidth *= 100.0f;
    angle = particle->angle;
    __asm__ __volatile__(
        "mfc1 $8, %1\n\tqmtc2 $8, vf4\n\tvcallms 0x20\n\t"
        "qmfc2.i $8, vf1\n\tmtc1 $8, %0"
        : "=f"(sine) : "f"(angle) : "$8", "memory");
    __asm__ __volatile__(
        "mfc1 $8, %1\n\tqmtc2 $8, vf4\n\tvcallms 0xE8\n\t"
        "qmfc2.i $8, vf1\n\tmtc1 $8, %0"
        : "=f"(cosine) : "f"(angle) : "$8", "memory");

    cornerX = offsetX - halfWidth;
    cornerY = offsetY - halfHeight;
    vertex.x = cornerX * cosine - cornerY * sine;
    vertex.y = cornerX * sine + cornerY * cosine;
    svAppendSpriteVertex(packet, &textureCoordinates[0], &vertex, centerX, centerY);
    cornerX = offsetX + halfWidth;
    cornerY = offsetY - halfHeight;
    vertex.x = cornerX * cosine - cornerY * sine;
    vertex.y = cornerX * sine + cornerY * cosine;
    svAppendSpriteVertex(packet, &textureCoordinates[1], &vertex, centerX, centerY);
    cornerX = offsetX - halfWidth;
    cornerY = offsetY + halfHeight;
    vertex.x = cornerX * cosine - cornerY * sine;
    vertex.y = cornerX * sine + cornerY * cosine;
    svAppendSpriteVertex(packet, &textureCoordinates[2], &vertex, centerX, centerY);
    cornerX = offsetX + halfWidth;
    cornerY = offsetY + halfHeight;
    vertex.x = cornerX * cosine - cornerY * sine;
    vertex.y = cornerX * sine + cornerY * cosine;
    svAppendSpriteVertex(packet, &textureCoordinates[3], &vertex, centerX, centerY);
    packet->loopCount = 1;
    packet->base[packet->tagIndex].dword[0] |= packet->loopCount;
}

static void svRotMatrixScale(void *dst, const void *src)
{
    const char *base = (const char *)src;
    const char *contrib = base + 0xC0;
    const char *scale = base + 0x100;

    __asm__ __volatile__(
        "lqc2 vf8, 0(%2)\n\t"
        "lqc2 vf1, 0(%1)\n\t"
        "lqc2 vf2, 16(%1)\n\t"
        "lqc2 vf3, 32(%1)\n\t"
        "lqc2 vf4, 48(%1)\n\t"
        "lqc2 vf5, 0(%3)\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf1x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf1z\n\t"
        "vmaddw.xyzw vf9xyzw, vf8xyzw, vf1w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf2x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf2y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf2z\n\t"
        "vmaddw.xyzw vf10xyzw, vf8xyzw, vf2w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf3x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf3y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf3z\n\t"
        "vmaddw.xyzw vf11xyzw, vf8xyzw, vf3w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf4x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf4y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf4z\n\t"
        "vmaddw.xyzw vf12xyzw, vf8xyzw, vf4w\n\t"
        "vmulx.xyzw vf9xyzw, vf9xyzw, vf5x\n\t"
        "vmuly.xyzw vf10xyzw, vf10xyzw, vf5y\n\t"
        "vmulz.xyzw vf11xyzw, vf11xyzw, vf5z\n\t"
        "vmulw.xyzw vf12xyzw, vf12xyzw, vf5w\n\t"
        "sqc2 vf9, 0(%0)\n\t"
        "sqc2 vf10, 16(%0)\n\t"
        "sqc2 vf11, 32(%0)\n\t"
        "sqc2 vf12, 48(%0)\n\t"
        "nop"
        : : "r"(dst), "r"(base), "r"(contrib), "r"(scale) : "memory");
}

void svDrawModel(SvModelDrawItem *item, void *place)
{
    Matrix lightMatrix;
    const Matrix *sourceLight;
    unsigned long long alpha;

    if (item->blendMode == 0 && item->position.alpha == 128) {
        alpha = 0;
    } else {
        alpha = svPackModelAlpha(item->blendMode, item->position.alpha);
        nmlModelSetSortOffset(-5.0f);
    }
    nmlModelSetFogCancel();
    sourceLight = item->blendMode == 0 ? &_defMatLcMod : &_defMatLc;
    /* SDK-shaped four-quadword copy; both global source matrices and
     * the ABI-aligned stack matrix have 16-byte-aligned bases. */
    __asm__ __volatile__(
        "lq $8, 0(%2)\n\t"
        "lq $9, 16(%2)\n\t"
        "lq $10, 32(%2)\n\t"
        "lq $11, 48(%2)\n\t"
        "sq $8, 0(%1)\n\t"
        "sq $9, 16(%1)\n\t"
        "sq $10, 32(%1)\n\t"
        "sq $11, 48(%1)"
        : "=m"(lightMatrix)
        : "r"(&lightMatrix), "r"(sourceLight), "m"(*sourceLight)
        : "$8", "$9", "$10", "$11", "memory");
    /* Convert the model's integer position to the light matrix's final
     * row; W carries alpha and the xyz-only scale leaves it untouched. */
    {
        float *positionRow = &lightMatrix.elements[12];
        const SvModelPosition *position = &item->position;
        float positionScale;

        __asm__ __volatile__("" : : "r"(position), "r"(positionRow));
        positionScale = 1.0f / 128.0f;

        __asm__ __volatile__(
            "mfc1 $8, %2\n\t"
            "qmtc2 $8, vf1\n\t"
            "lqc2 vf2, 0(%1)\n\t"
            "vitof0.xyzw vf2xyzw, vf2xyzw\n\t"
            "vmulx.xyz vf2xyz, vf2xyz, vf1x\n\t"
            "sqc2 vf2, 0(%0)"
            : : "r"(positionRow), "r"(position), "f"(positionScale)
            : "$8", "memory");
    }
    nmlModelSetLight(&lightMatrix, &_defMatLn);
    nmlModelSetSpecularOff(1);
    if (item->renderFlags & 1) {
        nmlModelSetRenderStatus(256);
    }
    if (alpha != 0) {
        nmlModelSetToumei(1);
        nmlModelSetAlpha(alpha);
    }
    if (item->texture != 0) {
        nmlModelSetTexture(item->texture);
    }
    nmlModelSetClip(1);
    nmlModelSetPlace(place);
    nmlModelEntryEffect(item->model);
}

void sefDrawParticle2(SvQuadParticle *particle, const Vector4 *position,
                      SGsPacket *packet, SvParticleDrawContext *context)
{
    SGsGifRecord *header;
    SGsGifRecord *vertices;
    const SvQuadImageParameters *image;
    const Vector4 *scale;
    const Quadword *textureCoordinates;
    unsigned long long registerWord;
    int drawVariant;

    header = &packet->base[packet->count];
    vertices = &header[3];
    scale = context->scale;
    /* The render basis and viewport registers are live on entry. */
    __asm__ __volatile__(
        "lqc2 vf8, 0(%0)\n\t"
        "lqc2 vf31, 0(%1)\n\t"
        "lqc2 vf1, 0(%2)\n\t"
        "lqc2 vf2, 16(%2)\n\t"
        "lqc2 vf3, 32(%2)\n\t"
        "lqc2 vf4, 48(%2)\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf1x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf1z\n\t"
        "vmaddw.xyzw vf9xyzw, vf8xyzw, vf1w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf2x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf2y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf2z\n\t"
        "vmaddw.xyzw vf10xyzw, vf8xyzw, vf2w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf3x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf3y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf3z\n\t"
        "vmaddw.xyzw vf11xyzw, vf8xyzw, vf3w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf4x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf4y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf4z\n\t"
        "vmaddw.xyzw vf12xyzw, vf8xyzw, vf4w\n\t"
        "vmulx.xyzw vf9xyzw, vf9xyzw, vf31x\n\t"
        "vmuly.xyzw vf10xyzw, vf10xyzw, vf31y\n\t"
        "vmulz.xyzw vf11xyzw, vf11xyzw, vf31z\n\t"
        "vmulw.xyzw vf12xyzw, vf12xyzw, vf31w"
        : : "r"(position), "r"(scale), "r"(&particle->localTransform)
        : "memory");
    /* Project the four transformed and scaled matrix rows. */
    __asm__ __volatile__(
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf9x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf9y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf9z\n\t"
        "vmaddw.xyzw vf27xyzw, vf24xyzw, vf9w\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf10x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf10y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf10z\n\t"
        "vmaddw.xyzw vf28xyzw, vf24xyzw, vf10w\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf11x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf11y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf11z\n\t"
        "vmaddw.xyzw vf29xyzw, vf24xyzw, vf11w\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf12x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf12y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf12z\n\t"
        "vmaddw.xyzw vf30xyzw, vf24xyzw, vf12w"
        : : );

    {
        unsigned long long gifRegisters = 0x00512512512512eeULL;
        unsigned long long dataWord;

        header[0].dword[1] = gifRegisters;
        dataWord = 0xe02a400000008000ULL;
        header[0].dword[0] = dataWord;
        image = &particle->imageParameters;
        dataWord = image->texture;
        header[1].dword[0] = dataWord;
        header[1].dword[1] = 6;
        dataWord = alphaTbl_004CBE78[particle->blendMode];
        registerWord = 66;
        header[2].dword[0] = dataWord;
        header[2].dword[1] = registerWord;
    }
    drawVariant = context->drawVariant;
    if (drawVariant & 1) {
        float halfWidth = image->halfWidth;
        float halfHeight = image->halfHeight;
        const Vector4 *plane = &particle->planeOffset;

        __asm__ __volatile__(
            "lqc2 vf5, 0(%2)\n\t"
            "mfc1 $2, %0\n\tmfc1 $3, %1\n\t"
            "qmtc2 $2, vf6\n\tqmtc2 $3, vf7\n\t"
            "vmove.zw vf1zw, vf5zw\n\tvmove.zw vf2zw, vf5zw\n\t"
            "vmove.zw vf3zw, vf5zw\n\tvmove.zw vf4zw, vf5zw\n\t"
            "vsubx.x vf1x, vf5x, vf6x\n\tvaddx.x vf2x, vf5x, vf6x\n\t"
            "vsubx.x vf3x, vf5x, vf6x\n\tvaddx.x vf4x, vf5x, vf6x\n\t"
            "vaddx.y vf1y, vf5y, vf7x\n\tvaddx.y vf2y, vf5y, vf7x\n\t"
            "vsubx.y vf3y, vf5y, vf7x\n\tvsubx.y vf4y, vf5y, vf7x"
            : : "f"(halfWidth), "f"(halfHeight), "r"(plane)
            : "$2", "$3", "memory");
    } else if (drawVariant & 2) {
        float halfWidth = image->halfWidth;
        float halfHeight = image->halfHeight;
        const Vector4 *plane = &particle->planeOffset;

        __asm__ __volatile__(
            "lqc2 vf5, 0(%2)\n\t"
            "mfc1 $2, %0\n\tmfc1 $3, %1\n\t"
            "qmtc2 $2, vf6\n\tqmtc2 $3, vf7\n\t"
            "vmove.yw vf1yw, vf5yw\n\tvmove.yw vf2yw, vf5yw\n\t"
            "vmove.yw vf3yw, vf5yw\n\tvmove.yw vf4yw, vf5yw\n\t"
            "vsubx.x vf1x, vf5x, vf6x\n\tvaddx.x vf2x, vf5x, vf6x\n\t"
            "vsubx.x vf3x, vf5x, vf6x\n\tvaddx.x vf4x, vf5x, vf6x\n\t"
            "vaddx.z vf1z, vf5z, vf7x\n\tvaddx.z vf2z, vf5z, vf7x\n\t"
            "vsubx.z vf3z, vf5z, vf7x\n\tvsubx.z vf4z, vf5z, vf7x"
            : : "f"(halfWidth), "f"(halfHeight), "r"(plane)
            : "$2", "$3", "memory");
    } else if (drawVariant & 4) {
        float halfWidth = image->halfWidth;
        float halfHeight = image->halfHeight;
        const Vector4 *plane = &particle->planeOffset;

        __asm__ __volatile__(
            "lqc2 vf5, 0(%2)\n\t"
            "mfc1 $2, %0\n\tmfc1 $3, %1\n\t"
            "qmtc2 $2, vf6\n\tqmtc2 $3, vf7\n\t"
            "vmove.xw vf1xw, vf5xw\n\tvmove.xw vf2xw, vf5xw\n\t"
            "vmove.xw vf3xw, vf5xw\n\tvmove.xw vf4xw, vf5xw\n\t"
            "vsubx.y vf1y, vf5y, vf6x\n\tvaddx.y vf2y, vf5y, vf6x\n\t"
            "vsubx.y vf3y, vf5y, vf6x\n\tvaddx.y vf4y, vf5y, vf6x\n\t"
            "vaddx.z vf1z, vf5z, vf7x\n\tvaddx.z vf2z, vf5z, vf7x\n\t"
            "vsubx.z vf3z, vf5z, vf7x\n\tvsubx.z vf4z, vf5z, vf7x"
            : : "f"(halfWidth), "f"(halfHeight), "r"(plane)
            : "$2", "$3", "memory");
    } else {
        return;
    }

    textureCoordinates = context->textureCoordinates;
    vertices[0].quad = textureCoordinates[0];
    vertices[3].quad = textureCoordinates[1];
    vertices[6].quad = textureCoordinates[2];
    vertices[9].quad = textureCoordinates[3];
    vertices[1].quad = particle->packedColor.quad;
    vertices[4].quad = particle->packedColor.quad;
    vertices[7].quad = particle->packedColor.quad;
    vertices[10].quad = particle->packedColor.quad;

    /* Project each corner, divide ST by W, then write the packed XYZ2 lanes. */
    __asm__ __volatile__(
        "ctc2 $0, $vi18\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf1x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf1z\n\t"
        "vmaddw.xyzw vf9xyzw, vf30xyzw, vf0w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf2x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf2y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf2z\n\t"
        "vclipw.xyz vf9xyz, vf9w\n\tvdiv Q, vf0w, vf9w\n\t"
        "vmaddw.xyzw vf10xyzw, vf30xyzw, vf0w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf3x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf3y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf3z\n\t"
        "vmaddw.xyzw vf11xyzw, vf30xyzw, vf0w\n\t"
        "vclipw.xyz vf10xyz, vf10w\n\tlqc2 vf31, 0(%0)\n\t"
        "vwaitq\n\tvmulq.xyz vf9xyz, vf9xyz, Q\n\t"
        "vmulq.xyz vf31xyz, vf31xyz, Q\n\tvdiv Q, vf0w, vf10w\n\t"
        "sqc2 vf31, 0(%0)\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf4x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf4y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf4z\n\t"
        "vmaddw.xyzw vf12xyzw, vf30xyzw, vf0w\n\t"
        "vclipw.xyz vf11xyz, vf11w\n\tlqc2 vf31, 48(%0)\n\t"
        "vwaitq\n\tvmulq.xyz vf10xyz, vf10xyz, Q\n\t"
        "vmulq.xyz vf31xyz, vf31xyz, Q\n\tvdiv Q, vf0w, vf11w\n\t"
        "sqc2 vf31, 48(%0)\n\tvclipw.xyz vf12xyz, vf12w\n\t"
        "lqc2 vf31, 96(%0)\n\tvwaitq\n\t"
        "vmulq.xyz vf11xyz, vf11xyz, Q\n\t"
        "vmulq.xyz vf31xyz, vf31xyz, Q\n\tvdiv Q, vf0w, vf12w\n\t"
        "sqc2 vf31, 96(%0)\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf1xyzw, vf9xyzw, vf25xyzw\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf2xyzw, vf10xyzw, vf25xyzw\n\t"
        "lqc2 vf31, 144(%0)\n\tvwaitq\n\t"
        "vmulq.xyz vf12xyz, vf12xyz, Q\n\t"
        "vmulq.xyz vf31xyz, vf31xyz, Q\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf3xyzw, vf11xyzw, vf25xyzw\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf4xyzw, vf12xyzw, vf25xyzw\n\t"
        "sqc2 vf31, 144(%0)\n\t"
        "vftoi4.xy vf9xy, vf1xy\n\tvftoi4.xy vf10xy, vf2xy\n\t"
        "vftoi4.xy vf11xy, vf3xy\n\tvftoi4.xy vf12xy, vf4xy\n\t"
        "vftoi0.zw vf9zw, vf1zw\n\tvftoi0.zw vf10zw, vf2zw\n\t"
        "vftoi0.zw vf11zw, vf3zw\n\tvftoi0.zw vf12zw, vf4zw\n\t"
        "vsub.w vf9w, vf9w, vf9w\n\tvsub.w vf10w, vf10w, vf10w\n\t"
        "vsub.w vf11w, vf11w, vf11w\n\tvsub.w vf12w, vf12w, vf12w\n\t"
        "sqc2 vf9, 32(%0)\n\tsqc2 vf10, 80(%0)\n\t"
        "sqc2 vf11, 128(%0)\n\tsqc2 vf12, 176(%0)"
        : : "r"(vertices) : "memory");
    __asm__ __volatile__("cfc2.ni %0, $vi18" : "=r"(registerWord));
    if (registerWord == 0) {
        packet->tagIndex = packet->count;
        packet->count += 15;
        header[0].dword[0] |= 1;
    }
}

void sefDrawParticle(SvQuadParticle *particle, const Vector4 *position,
                     SGsPacket *packet, SvParticleDrawContext *context)
{
    SGsGifRecord *header;
    SGsGifRecord *vertices;
    SGsGifRecord *xyzVertices;
    const SvQuadImageParameters *image;
    const Quadword *textureCoordinates;
    const Vector4 *scale;
    float angle;
    float halfWidth;
    float halfHeight;
    int clipFlags;

    scale = context->scale;
    header = &packet->base[packet->count];
    vertices = &header[4];
    angle = particle->angle;
    __asm__ __volatile__(
        "lqc2 vf8, 0(%0)\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf16x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf16y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf16z\n\t"
        "vmaddw.xyzw vf9xyzw, vf8xyzw, vf16w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf17x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf17y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf17z\n\t"
        "vmaddw.xyzw vf10xyzw, vf8xyzw, vf17w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf18x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf18y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf18z\n\t"
        "vmaddw.xyzw vf11xyzw, vf8xyzw, vf18w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf19x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf19y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf19z\n\t"
        "vmaddw.xyzw vf12xyzw, vf8xyzw, vf19w\n\t"
        "mfc1 $8, %2\n\tqmtc2 $8, vf4\n\tvcallms 0xe8\n\t"
        "vaddx.x vf7x, vf0x, vf1x\n\tqmtc2 $8, vf4\n\t"
        "vcallms 0x20\n\tvaddx.x vf8x, vf0x, vf1x\n\t"
        "lqc2 vf1, 0(%1)\n\t"
        "vmulax.xyzw ACCxyzw, vf9xyzw, vf7x\n\t"
        "vmaddx.xyzw vf5xyzw, vf10xyzw, vf8x\n\t"
        "vmulax.xyzw ACCxyzw, vf10xyzw, vf7x\n\t"
        "vmsubx.xyzw vf6xyzw, vf9xyzw, vf8x\n\t"
        "vmulz.xyzw vf11xyzw, vf11xyzw, vf1z\n\t"
        "vmulw.xyzw vf12xyzw, vf12xyzw, vf1w\n\t"
        "vmulx.xyzw vf9xyzw, vf5xyzw, vf1x\n\t"
        "vmuly.xyzw vf10xyzw, vf6xyzw, vf1y\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf9x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf9y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf9z\n\t"
        "vmaddw.xyzw vf27xyzw, vf24xyzw, vf9w\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf10x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf10y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf10z\n\t"
        "vmaddw.xyzw vf28xyzw, vf24xyzw, vf10w\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf11x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf11y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf11z\n\t"
        "vmaddw.xyzw vf29xyzw, vf24xyzw, vf11w\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf12x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf12y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf12z\n\t"
        "vmaddw.xyzw vf30xyzw, vf24xyzw, vf12w"
        : : "r"(position), "r"(scale), "f"(angle)
        : "$8", "memory");

    __asm__ __volatile__("" : : "r"(vertices));
    image = svPrepareParticleImage(header, particle, *context->gifRegisters,
                                   6, 66);
    halfWidth = image->halfWidth;
    halfHeight = image->halfHeight;
    __asm__ __volatile__(
        "lqc2 vf5, 0(%2)\n\tmfc1 $2, %0\n\tmfc1 $3, %1\n\t"
        "qmtc2 $2, vf6\n\tqmtc2 $3, vf7\n\t"
        "vmove.zw vf1zw, vf5zw\n\tvmove.zw vf2zw, vf5zw\n\t"
        "vmove.zw vf3zw, vf5zw\n\tvmove.zw vf4zw, vf5zw\n\t"
        "vsubx.x vf1x, vf5x, vf6x\n\tvaddx.x vf2x, vf5x, vf6x\n\t"
        "vsubx.x vf3x, vf5x, vf6x\n\tvaddx.x vf4x, vf5x, vf6x\n\t"
        "vaddx.y vf1y, vf5y, vf7x\n\tvaddx.y vf2y, vf5y, vf7x\n\t"
        "vsubx.y vf3y, vf5y, vf7x\n\tvsubx.y vf4y, vf5y, vf7x"
        : : "f"(halfWidth), "f"(halfHeight), "r"(&particle->planeOffset)
        : "$2", "$3", "memory");

    textureCoordinates = context->textureCoordinates;
    xyzVertices = &vertices[1];
    vertices[0].quad = textureCoordinates[0];
    vertices[2].quad = textureCoordinates[1];
    vertices[4].quad = textureCoordinates[2];
    vertices[6].quad = textureCoordinates[3];
    /* All XYZ2 stores finish before the clip flags are returned to C. */
    __asm__ __volatile__(
        "ctc2 $0, $vi18\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf1x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf1z\n\t"
        "vmaddw.xyzw vf9xyzw, vf30xyzw, vf0w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf2x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf2y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf2z\n\t"
        "vmaddw.xyzw vf10xyzw, vf30xyzw, vf0w\n\t"
        "vclipw.xyz vf9xyz, vf9w\n\tvdiv Q, vf0w, vf9w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf3x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf3y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf3z\n\t"
        "vmaddw.xyzw vf11xyzw, vf30xyzw, vf0w\n\t"
        "vclipw.xyz vf10xyz, vf10w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf4x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf4y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf4z\n\t"
        "vmaddw.xyzw vf12xyzw, vf30xyzw, vf0w\n\t"
        "vclipw.xyz vf11xyz, vf11w\n\t"
        "vmulq.xyz vf9xyz, vf9xyz, Q\n\tvmulq.xyz vf10xyz, vf10xyz, Q\n\t"
        "vmulq.xyz vf11xyz, vf11xyz, Q\n\tvclipw.xyz vf12xyz, vf12w\n\t"
        "vmulq.xyz vf12xyz, vf12xyz, Q\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf1xyzw, vf9xyzw, vf25xyzw\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf2xyzw, vf10xyzw, vf25xyzw\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf3xyzw, vf11xyzw, vf25xyzw\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf4xyzw, vf12xyzw, vf25xyzw\n\t"
        "vftoi4.xy vf9xy, vf1xy\n\tvftoi4.xy vf10xy, vf2xy\n\t"
        "vftoi4.xy vf11xy, vf3xy\n\tvftoi4.xy vf12xy, vf4xy\n\t"
        "vftoi0.zw vf9zw, vf1zw\n\tvftoi0.zw vf10zw, vf2zw\n\t"
        "vftoi0.zw vf11zw, vf3zw\n\tvftoi0.zw vf12zw, vf4zw\n\t"
        "vsub.w vf9w, vf0w, vf0w\n\tvsub.w vf10w, vf0w, vf0w\n\t"
        "vsub.w vf11w, vf0w, vf0w\n\tvsub.w vf12w, vf0w, vf0w\n\t"
        "sqc2 vf9, 0(%1)\n\tsqc2 vf10, 32(%1)\n\t"
        "sqc2 vf11, 64(%1)\n\tsqc2 vf12, 96(%1)\n\tcfc2.ni %0, $vi18"
        : "=r"(clipFlags) : "r"(xyzVertices) : "memory");
    if (clipFlags == 0) {
        int tagIndex = packet->count;

        packet->tagIndex = tagIndex;
        packet->count = tagIndex + 12;
        header->dword[0] |= 1;
    }
}

void sefDrawParticleList(SvParticleEffect *effect)
{
    Matrix4 modelPlace;
    SvParticleDrawContext context;
    SGsPacket *packet;
    int is2D;
    int drawMode;
    int drawVariant;
    SvModelDrawItem *particles;
    short *particleIndex;
    short quadRenderType;
    int available;

    is2D = 0;
    drawMode = _draw3D;
    if (*effect->renderDomainA == 3 || *effect->renderDomainB == 3) {
        is2D = 1;
    }
    drawVariant = 0;
    if (effect->scriptWords != 0) {
        drawVariant = effect->scriptWords[-1];
    }
    packet = &_packet;
    particles = (SvModelDrawItem *)sevGetPtAllocator2(0);
    if (drawMode != 1 || is2D != 0) {
        if (drawMode != 1 && is2D == 1) {
            _nEffect2D++;
            _imageMapper.referenceImages[_nowImage].words[1]++;
        } else {
            if (drawMode == 0) {
                _imageMapper.referenceImages[_nowImage].words[0]++;
            }
            context.textureCoordinates = stqTbl_004CBEA0;
            context.gifRegisters = &_svRegData;
            context.drawVariant = drawVariant;
            __asm__ __volatile__(
                "lqc2 vf16, 0(%0)\n\tlqc2 vf17, 16(%0)\n\t"
                "lqc2 vf18, 32(%0)\n\tlqc2 vf19, 48(%0)"
                : : "r"(&_invView) : "memory");
            __asm__ __volatile__(
                "lqc2 vf13, 0(%0)\n\tlqc2 vf14, 16(%0)\n\tlqc2 vf15, 32(%0)"
                : : "r"(&_svUnitMatrix) : "memory");
            packet->loopCount = 1;
            packet->base = (SGsGifRecord *)0x70000000;
            packet->count = 0;
            available = 64;
            quadRenderType = 4;
            packet->tagIndex = 0;
            particleIndex = effect->particleIndices;
            {
                int remaining = 255;

                while (remaining >= 0) {
                    int index = *particleIndex;

                    if (index >= 0) {
                        SvModelDrawItem *item = &particles[index];

                        if (item->position.alpha != 0) {
                            switch (drawMode) {
                            case 2:
                                if (item->renderType == 6) {
                                    svRotMatrixScale(&modelPlace, item);
                                    svDrawModel(item, &modelPlace);
                                }
                                break;
                            case 1:
                                if (item->renderType == quadRenderType) {
                                    available--;
                                    sefDrawParticle2D(item, packet, context.textureCoordinates);
                                    if (available == 0) {
                                        available = 64;
                                        SGsSendPacket((SGsPacketReset *)packet);
                                    }
                                }
                                break;
                            case 0:
                                if (item->renderType == quadRenderType) {
                                    context.scale = &item->scale;
                                    if (context.drawVariant == 0) {
                                        sefDrawParticle(item, &item->origin, packet, &context);
                                    } else {
                                        sefDrawParticle2(item, &item->origin, packet, &context);
                                    }
                                    available--;
                                    if (available == 0) {
                                        available = 64;
                                        SGsSendPacket((SGsPacketReset *)packet);
                                    }
                                }
                                break;
                            }
                        }
                    }
                    remaining--;
                    particleIndex++;
                }
            }
            SGsSendPacket((SGsPacketReset *)packet);
        }
    }
}

void sefDrawParticleListCf(SvParticleEffect *effect,
                           SvParticleListCfContext *context,
                           SvSchedulerEffectRecord *scheduler)
{
    extern Matrix4 *MMathTranslateMatrix(Matrix4 *, const Matrix4 *, const Vector4 *);
    extern Matrix4 *MMathScaleMatrix(Matrix4 *, const Matrix4 *, const Vector4 *);
    extern void sefDrawParticle2(SvQuadParticle *, const Vector4 *, SGsPacket *,
                                 SvParticleDrawContext *);
    Vector4 position;
    Vector4 scaledScale;
    Matrix4 modelPlace;
    Matrix4 particlePlace;
    Matrix4 *modelPlaceOutput;
    Vector4 *scaleOutput;
    Matrix4 *particlePlaceOutput;
    int drawMode;
    SGsPacket *drawPacket;
    SGsGifRecord *scratchpad;
    SvModelDrawItem *particles;
    int composePlace;
    short drawVariant;
    int available;
    short *particleIndex;
    int remaining;

    drawMode = _draw3D;
    drawPacket = &_packet;
    particles = (SvModelDrawItem *)sevGetPtAllocator2(0);
    composePlace = (effect->flags & 0x400) != 0 ||
                   (scheduler->flags & 0x1000) != 0;
    drawVariant = 0;
    if (effect->scriptWords != 0) {
        drawVariant = effect->scriptWords[-1];
    }
    /* The draw helpers consume the persistent camera and unit-matrix VF banks. */
    __asm__ __volatile__(
        "lqc2 vf16, 0(%0)\n\tlqc2 vf17, 16(%0)\n\t"
        "lqc2 vf18, 32(%0)\n\tlqc2 vf19, 48(%0)"
        : : "r"(&_invView) : "memory");
    __asm__ __volatile__(
        "lqc2 vf13, 0(%0)\n\tlqc2 vf14, 16(%0)\n\tlqc2 vf15, 32(%0)"
        : : "r"(&_svUnitMatrix) : "memory");
    _packet.loopCount = 1;
    scaleOutput = &scaledScale;
    particleIndex = effect->particleIndices;
    _packet.count = 0;
    available = 64;
    _packet.tagIndex = 0;
    particlePlaceOutput = &particlePlace;
    scratchpad = (SGsGifRecord *)0x70000000;
    _packet.base = scratchpad;
    modelPlaceOutput = &modelPlace;
    position.w = 1.0f;
    context->drawVariant = drawVariant;
    scaledScale.w = 1.0f;
    context->scaledScale = scaleOutput;
    for (remaining = 255; remaining >= 0; remaining--, particleIndex++) {
        int index = *particleIndex;

        if (index >= 0) {
            SvModelDrawItem *item = &particles[index];

            if (composePlace != 0) {
                MMathMulMatrix(particlePlaceOutput, &item->cfMatrix,
                               (const Matrix4 *)scheduler->localTransform);
                context->place = particlePlaceOutput;
                context->scaleMultiplier = item->scaleMultiplier * scheduler->localScale;
            }
            switch (drawMode) {
            case 2:
                if (item->renderType == 6) {
                    MMathTranslateMatrix(modelPlaceOutput, context->place, &item->origin);
                    MMathMulMatrix(modelPlaceOutput, modelPlaceOutput, &item->orientation);
                    MMathScaleMatrix(modelPlaceOutput, modelPlaceOutput, &item->scale);
                    svDrawModel(item, modelPlaceOutput);
                }
                break;
            default:
                if (item->position.alpha != 0 && item->renderType == 4) {
                    MMathApplyMatrix(&position, context->place, &item->origin);
                    __asm__ __volatile__(
                        "lqc2 vf1, 0(%0)\n\t"
                        "mfc1 $8, %1\n\t"
                        "qmtc2.ni $8, vf2\n\t"
                        "vmulx.xyz vf1, vf1, vf2x\n\t"
                        "sqc2 vf1, 0(%2)"
                        : : "r"(&item->scale), "f"(context->scaleMultiplier),
                            "r"(scaleOutput) : "$8", "memory");
                    if (context->drawVariant == 0) {
                        sefDrawParticle((SvQuadParticle *)item, &position, drawPacket,
                                        (SvParticleDrawContext *)context);
                    } else {
                        sefDrawParticle2((SvQuadParticle *)item, &position, drawPacket,
                                         (SvParticleDrawContext *)context);
                    }
                    available--;
                }
                break;
            }
            if (available == 0) {
                available = 64;
                SGsSendPacket((SGsPacketReset *)&_packet);
            }
        }
    }
    SGsSendPacket((SGsPacketReset *)&_packet);
}

void sefDrawSchedulerEffect(int schedulerIndex)
{
    SvSchedulerEffectRecord *scheduler =
        (SvSchedulerEffectRecord *)(sefGetScheduler() + schedulerIndex * 0xAB0);
    SvBattleEffectData *battleData = (SvBattleEffectData *)sefGetBattleData();
    StudioCamera *camera;
    int drawMode;
    int groupIndex;
    int effectIndex;

    if (scheduler->inUse == 0 || scheduler->active == 0) {
        return;
    }

    camera = xglStudioGetActiveCamera();
    __asm__ __volatile__(
        "lqc2 vf21, 0(%0)\n\t"
        "lqc2 vf22, 16(%0)\n\t"
        "lqc2 vf23, 32(%0)\n\t"
        "lqc2 vf24, 48(%0)"
        : : "r"(&camera->viewMatrix) : "memory");
    __asm__ __volatile__(
        "lqc2 vf25, 0(%0)\n\t"
        "lqc2 vf26, 0(%1)"
        : : "r"(&camera->screenScale), "r"(&camera->screenOffset) : "memory");

    battleData->flags = scheduler->flags;
    battleData->partialFlags = scheduler->flags & 2;
    if ((scheduler->flags & 2) == 0 || (drawMode = _draw3D) == 1) {
        __asm__ __volatile__(
            "lq $8, 0(%1)\n\t"
            "sq $8, 0(%0)"
            : : "r"(&battleData->packetInfo), "r"(&scheduler->packetInfo)
            : "$8", "memory");
        for (groupIndex = 0; groupIndex < 32; groupIndex++) {
            SvSchedulerEffectGroup *group = &scheduler->groups[groupIndex];

            if (group->effectCount == 0) {
                continue;
            }
            for (effectIndex = 0; effectIndex < 32; effectIndex++) {
                int lineIndex = group->effectIds[effectIndex];

                if (lineIndex >= 0) {
                    sefDrawParticleList((SvParticleEffect *)sefGetLineAdr(lineIndex));
                }
            }
        }
    } else {
        Quadword localTransform[4];
        Vector4 position;
        SvParticleListCfContext context;
        float matrixScale;
        const Quadword *gifRegisters;

        context.textureCoordinates = stqTbl_004CBEA0;
        gifRegisters = &_svRegData;
        context.schedulerVariant = (unsigned short)scheduler->flags & 0x10;
        context.gifRegisters = gifRegisters;
        context.place = (Matrix4 *)localTransform;
        if (scheduler->transformFlagA != 0 || scheduler->transformFlagB != 0) {
            MMathMulMatrix((Matrix4 *)localTransform,
                           (const Matrix4 *)scheduler->transform,
                           (const Matrix4 *)scheduler->localTransform);
            matrixScale = sefGetMatrixScale((float *)localTransform);
        } else {
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "lq $9, 16(%1)\n\t"
                "lq $10, 32(%1)\n\t"
                "lq $11, 48(%1)\n\t"
                "sq $8, 0(%0)\n\t"
                "sq $9, 16(%0)\n\t"
                "sq $10, 32(%0)\n\t"
                "sq $11, 48(%0)"
                : : "r"(localTransform), "r"(scheduler->localTransform)
                : "$8", "$9", "$10", "$11", "memory");
            matrixScale = scheduler->localScale;
        }
        battleData->scaleMultiplier = scheduler->scaleMultiplier * matrixScale;

        for (groupIndex = 0; groupIndex < 32; groupIndex++) {
            SvSchedulerEffectGroup *group = &scheduler->groups[groupIndex];

            if (group->effectCount == 0) {
                continue;
            }
            for (effectIndex = 0; effectIndex < 32; effectIndex++) {
                int lineIndex = group->effectIds[effectIndex];

                if (lineIndex >= 0) {
                    SvParticleEffect *effect =
                        (SvParticleEffect *)sefGetLineAdr(lineIndex);
                    Vector4 *effectPosition = &effect->position;

                    context.place = (Matrix4 *)localTransform;
                    context.scaleMultiplier = matrixScale;
                    if (*effect->renderDomainA == 3 || *effect->renderDomainB == 3) {
                        char *base = (char *)&_imageMapper.typeLists[0].size;
                        int offset = _nowImage * 8 + 369760;
                        SvReferenceImageSlot *slot = (SvReferenceImageSlot *)(base + offset);

                        _nEffect2D++;
                        slot->words[0]++;
                    } else if ((effect->flags & 0x400) != 0) {
                        sefDrawParticleListCf(effect, &context, scheduler);
                    } else {
                        MMathApplyMatrix(&position, (Matrix4 *)localTransform, effectPosition);
                        position.w = battleData->scaleMultiplier;
                        if (context.schedulerVariant != 0 &&
                            drawMode == 0 &&
                            sefClipViewVolume(&position, camera) != 0) {
                            continue;
                        }
                        sefDrawParticleListCf(effect, &context, scheduler);
                    }
                }
            }
        }
    }
}

static void svDrawSchedulerBlk(int eftCate, int eftNo)
{
    short effectId;
    int schedulerIndex;
    SchedulerRecord *record;
    SchedulerRecord *scheduler;

    scheduler = (SchedulerRecord *)sefGetScheduler();
    schedulerIndex = 0;
    record = scheduler;
    do {
        if (record->inUse != 0) {
            effectId = record->effectId;
            if (eft_5 != effectId) {
                eft_5 = effectId;
                srsAnalyzeEftNo(effectId, &charID_3, &eftCate_4);
            }
            if (eftCate_4 == eftCate &&
                (eftNo == 0 || (eftNo > 0 && eftNo == eft_5))) {
                SGsSetZTestEnv(((record->flags >> 2) ^ 1) & 1);
                sefDrawSchedulerEffect(schedulerIndex);
            }
        }
        schedulerIndex++;
        record++;
    } while (schedulerIndex < SCHEDULER_COUNT);
}

static void svDrawAlters(int layer)
{
    if (_draw3D == 0) {
        sdvDrawAlters(layer);
    }
}

static void svDrawMissile(int category)
{
    if (_draw3D == 0) {
        if (_scMslCate == category) {
            MEfObjExec2nd();
        }
    }
}

static void svDrawSchedulerParticle(void)
{
    int drawMode = _draw3D;
    int resourceType;
    int loadedTextureCount;
    int effectCount;

    SGsInitEnv();
    if (_imageMapper.typeLists[SRS_RES_IMAGE].entryCount > 0) {
        if (svLoadImageList(&_imageMapper.typeLists[SRS_RES_IMAGE]) > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(SRS_RES_IMAGE, 0);
    }
    svDrawMissile(SRS_RES_IMAGE);

    if (drawMode != 1 ||
        (int)_imageMapper.referenceImages[SRS_RES_BATTLE_IMAGE].words[1] > 0 ||
        (int)_imageMapper.referenceImages[SRS_RES_WEAPON].words[1] > 0) {
        loadedTextureCount = 0;
        for (resourceType = SRS_RES_WEAPON;
             resourceType <= SRS_RES_WEAPON + 8;
             resourceType++) {
            if (_imageMapper.typeLists[resourceType].entryCount > 0) {
                loadedTextureCount +=
                    svLoadImageList(&_imageMapper.typeLists[resourceType]);
            }
        }
        if (loadedTextureCount > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(SRS_RES_WEAPON, 0);
        svDrawAlters(SRS_RES_WEAPON);
    }
    svDrawMissile(SRS_RES_WEAPON);

    if (drawMode != 1 ||
        (int)_imageMapper.referenceImages[SRS_RES_BATTLE_IMAGE].words[1] > 0 ||
        (int)_imageMapper.referenceImages[SRS_RES_COMBO].words[1] > 0) {
        loadedTextureCount = 0;
        for (resourceType = SRS_RES_COMBO;
             resourceType <= SRS_RES_COMBO + 2;
             resourceType++) {
            if (_imageMapper.typeLists[resourceType].entryCount > 0) {
                loadedTextureCount +=
                    svLoadImageList(&_imageMapper.typeLists[resourceType]);
            }
        }
        if (loadedTextureCount > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(SRS_RES_COMBO, 0);
        svDrawAlters(SRS_RES_COMBO);
    }
    svDrawMissile(SRS_RES_COMBO);

    if ((drawMode != 1 ||
         (int)_imageMapper.referenceImages[SRS_RES_BATTLE_IMAGE].words[1] > 0) &&
        _imageMapper.typeLists[SRS_RES_BATTLE_IMAGE].entryCount > 0) {
        if (svLoadImageList(
                &_imageMapper.typeLists[SRS_RES_BATTLE_IMAGE]) > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(SRS_RES_BATTLE_IMAGE, 0);
    }
    svDrawAlters(SRS_RES_BATTLE_IMAGE);
    svDrawMissile(SRS_RES_BATTLE_IMAGE);

    if (_imageMapper.typeLists[SRS_RES_CF_IMAGE].entryCount > 0) {
        if (svLoadImageList(&_imageMapper.typeLists[SRS_RES_CF_IMAGE]) > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(SRS_RES_CF_IMAGE, 0);
    }

    for (resourceType = SRS_RES_EFFECT;
         resourceType < SRS_RES_COUNT;
         resourceType++) {
        if (_imageMapper.typeLists[resourceType].entryCount > 0 &&
            (drawMode != 1 ||
             (int)_imageMapper.referenceImages[resourceType].words[1] > 0)) {
            if (svLoadImageList(
                    &_imageMapper.typeLists[resourceType]) > 0) {
                SGsTexFlush();
            }
            effectCount = _srsMemRes.no[resourceType];
            if (effectCount > 0) {
                svDrawSchedulerBlk(SRS_RES_EFFECT, effectCount);
            }
        }
    }
    SGsRestoreEnv();
}

void svDrawScheduler2D(void)
{
    if (_nEffect2D != 0) {
        _nowImage = 0;
        _draw3D = 1;
        svDrawSchedulerParticle();
    }
}

void svDrawScheduler(void)
{
    _draw3D = 0;
    _nEffect2D = 0;
    _nowImage = 0;
    svDrawSchedulerParticle();
}

void SGsRestoreEnv(void);
extern void MEfObjExec1st(void);

void svDrawScheduler3D(void)
{
    int type;

    _draw3D = 2;
    svInitRefImage();
    SGsInitEnv();
    if (_imageMapper.typeLists[SRS_RES_IMAGE].entryCount > 0) {
        _nowImage = SRS_RES_IMAGE;
        svDrawSchedulerBlk(SRS_RES_IMAGE, 0);
    }
    _nowImage = SRS_RES_WEAPON;
    svDrawSchedulerBlk(SRS_RES_WEAPON, 0);
    _nowImage = SRS_RES_COMBO;
    svDrawSchedulerBlk(SRS_RES_COMBO, 0);
    _nowImage = SRS_RES_BATTLE_IMAGE;
    svDrawSchedulerBlk(SRS_RES_BATTLE_IMAGE, 0);
    _nowImage = SRS_RES_CF_IMAGE;
    svDrawSchedulerBlk(SRS_RES_CF_IMAGE, 0);
    for (type = SRS_RES_EFFECT; type < SRS_RES_COUNT; type++) {
        if (_imageMapper.typeLists[type].entryCount > 0) {
            short effectNumber = _srsMemRes.no[type];
            if (effectNumber > 0) {
                _nowImage = type;
                svDrawSchedulerBlk(SRS_RES_EFFECT, effectNumber);
            }
        }
    }
    MEfObjExec1st();
    SGsRestoreEnv();
}

void SGsInitGifPacket(SGsPacket *packet)
{
    packet->loopCount = 1;
    packet->base = (SGsGifRecord *)0x70000000;
    packet->tagIndex = 0;
    packet->count = 0;
}

void SGsOpenGifPacket(SGsPacket *packet, int prim, unsigned long long registers,
                      unsigned int nreg)
{
    unsigned long long tag;

    packet->loopCount = 1;
    packet->tagIndex = packet->count;
    packet->base[packet->count].dword[1] = registers;
    tag = ((unsigned long long)prim << 47) |
          ((unsigned long long)nreg << 60) | 0x0000400000008000ULL;
    packet->base[packet->count].dword[0] = tag;
    packet->count++;
}

void SGsCloseGifPacket(SGsPacket *packet) {
    packet->base[packet->tagIndex].dword[0] |= packet->loopCount;
}

void SGsAddReg(SGsPacket *packet, unsigned long long address,
               unsigned long long value)
{
    packet->base[packet->count].dword[0] = value;
    packet->base[packet->count].dword[1] = address;
    packet->count++;
}

void SGsAddGifRGBA(SGsPacket *packet, Quadword rgba)
{
    packet->base[packet->count].quad = rgba;
    packet->count++;
}

void SGsAddGifXYZ2(SGsPacket *packet, int x, int y, int z, int fog)
{
    packet->base[packet->count].word[0] = x * 0x10 + 0x7000;
    packet->base[packet->count].word[1] = y * 0x10 + 0x7200;
    packet->base[packet->count].word[2] = z;
    packet->base[packet->count].word[3] = fog;
    packet->count++;
}

void SGsAddGifData(SGsPacket *packet, Quadword data)
{
    packet->base[packet->count].quad = data;
    packet->count++;
}

void SGsAddGifSTQ(SGsPacket *packet, float texture_s, float texture_t,
                  float texture_q)
{
    packet->base[packet->count].floatValue[0] = texture_s;
    packet->base[packet->count].floatValue[1] = texture_t;
    packet->base[packet->count].floatValue[2] = texture_q;
    packet->base[packet->count].word[3] = 0;
    packet->count++;
}

void SGsAddGifUV(SGsPacket *packet, int u_coordinate, int v_coordinate)
{
    packet->base[packet->count].halfword[0] =
        (unsigned short)((unsigned int)u_coordinate << 4);
    packet->base[packet->count].halfword[1] =
        (unsigned short)((unsigned int)v_coordinate << 4);
    packet->count++;
}

const char D_004DBA50[8] = {'%', 's', '.', 'b', 'm', 'p', '\0', '\0'};

const char D_004DBA58[8] = {'%', 's', '.', 'c', 'l', 't', '\0', '\0'};
