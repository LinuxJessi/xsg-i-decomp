#include "common.h"

#include "shared.h"

#include "main/xgl_packet.h"

#include "main/srs.h"

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
    SGsGifRecord *base;     /* 0xC, starts at the scratchpad (0x70000000) */
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
    unsigned char type;
    unsigned char unmodeled_0F[3];
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
    unsigned char unmodeled_08[4];
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

s16 svLoadImageList(SvTypeList *typeList);

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

static void svAnalyzeChunk(SvTypeList *typeList, void *chunk);

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

extern void SGsSetZTestEnv(int enable);

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
    int inUse;                            /* +0x6b0 */
    unsigned char unmodeled_6b4[0xa78 - 0x6b4];
    short effectId;                       /* +0xa78 */
    unsigned char unmodeled_a7a[0xa8c - 0xa7a];
    int flags;                            /* +0xa8c */
    unsigned char unmodeled_a90[0xab0 - 0xa90];
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

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", svLoadImageList);

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

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", svAddImage);

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", svAddClut);

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", svAddModel);

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", svAddScript);

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

typedef struct SvResourceChunk {
    unsigned int byteSize;
    unsigned char unmodeled_04[4];
    short kind;
    unsigned char unmodeled_0A[54];
} SvResourceChunk;

typedef struct SvResourceChunkFile {
    SvResourceChunk *firstChunk;
} SvResourceChunkFile;

static int svAddImage(SvTypeList *typeList, void *chunk);
static int svAddClut(SvTypeList *typeList, void *chunk);
static int svAddModel(SvTypeList *typeList, void *chunk);
static int svAddScript(SvTypeList *typeList, void *chunk);

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

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", svInitImageMapper);

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


INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", svGetScript);

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

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", SGsInitEnv);

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

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", sefClipViewVolumeA);

int sefClipViewVolume(void *scheduler, void *effect)
{
    sefInitClipViewVolume((unsigned char *)effect + SEF_CLIP_VIEW_VOLUME_OFFSET);
    return sefClipViewVolumeA(scheduler);
}

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", sefDrawParticle2D);

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

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", sefDrawParticle2);

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", sefDrawParticle);

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

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", sefDrawParticleListCf);

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", sefDrawSchedulerEffect);

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

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", svDrawSchedulerParticle);

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

void SGsInitEnv(void);
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
