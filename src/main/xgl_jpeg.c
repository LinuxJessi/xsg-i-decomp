#include "common.h"

#include "shared.h"

static u8 *StartOfScan(u8 *segment, void *requestAddress);

/* The codec work area is quadword aligned for FFIDCT's VU0 transfers.
 * Quantization tables occupy +0xD0 and +0x1D0; bit input, IDCT storage and
 * restart counters are modeled below. Other regions remain explicit partial
 * spans. */

typedef struct JpegYuvPixel {
    float luminance;
    float firstChroma;
    float secondChroma;
    u8 unmodeled_0c[4];
} JpegYuvPixel;

typedef struct JpegImageDimensions {
    short width;
    short height;
} JpegImageDimensions;

typedef struct JpegDecodeControl {
    u8 *source;
    u8 *destination;
    u8 unmodeled_08[4];
    short frameBufferBase;
    short frameBufferWidth;
    u16 originX;
    u16 originY;
    JpegImageDimensions imageLimits;
    u8 luminancePercent;
    u8 chrominancePercent;
    u8 scaleMode;
    u8 pixelFormat;
    u8 redPercent;
    u8 greenPercent;
    u8 bluePercent;
    u8 unmodeled_1f;
} JpegDecodeControl;

typedef struct JpegEncodeRequest {
    u8 unmodeled_00[4];
    u8 *outputBuffer;
    int outputSize;
    short sourceX;
    short sourceY;
    short destinationX;
    short destinationY;
    short width;
    short height;
    u8 luminanceQuality;
    u8 chrominanceQuality;
    u8 samplingMode;
    u8 unmodeled_1b;
} JpegEncodeRequest;

extern int sceGsSetDefStoreImage(void *image, short sourceX, short sourceY, short psm,
                                 short destinationX, short destinationY, unsigned int width,
                                 unsigned int height);

extern int sceGsExecStoreImage(void *image, void *buffer);

extern u8 headerALL[0x26F];

extern u8 LQTableOrg_1[64];

extern u8 CQTableOrg_2[64];

extern float ffdct_0[8][8];

extern u8 imm0_3[0x80];

extern u8 imm1_4[0x60];

extern void sceGsSetDefLoadImage(void *image, short fbp, short fbw, short psm, short x, short y, short w, short h);

extern void sceGsExecLoadImage(void *image, void *data);

extern void *memcpy(void *dest, const void *src, unsigned int size);

extern const float D_004D8798;

extern const float D_004D879C;

extern const float D_004D87A0;

extern const float D_004D87A4;

extern const float D_004D87A8;

extern const float D_004D87AC;

extern const float D_004D87B0;

extern const float D_004D87B4;

extern const float D_004D87B8;

extern const float D_004D87BC;

extern const float D_004D87C0;

extern const float D_004D87C4;

typedef struct JpegWork {
    u8 unmodeled_0000[0xD0];
    float luminanceQuantization[64];    /* +0xD0 */
    float chrominanceQuantization[64];  /* +0x1D0 */
    u8 unmodeled_02d0[0x350 - 0x2D0];
    u8 *buffer;
    int outputPosition;
    signed char bitsRemaining;
    u8 bufferedByte;
    u8 unmodeled_035a[0x360 - 0x35a];
    int samplingMode;
    short imageWidth;
    short imageHeight;
    short maximumX; /* +0x368 */
    short maximumY; /* +0x36a */
    int rowBytes;
    s16 luminanceDcCodes[64]; /* +0x370 */
    s16 chrominanceDcCodes[64]; /* +0x3f0 */
    s16 luminanceAcCodes[1024]; /* +0x470 */
    s16 chrominanceAcCodes[1024]; /* +0xc70 */
    float idct_block[8][8];
    float idct_stage[8];
    int dcPredictors[3]; /* +0x1590 */
    unsigned char unmodeled_159c[0x15a0 - 0x159c];
    JpegYuvPixel decodedYuvPixels[16 * 16]; /* +0x15a0 */
    float decodedMcuPlanes[6 * 64]; /* +0x25a0 */
    u32 restart_interval;
    u32 restart_interval_plus_one;
    int colorScaleMode; /* +0x2ba8 */
    float luminanceScale; /* +0x2bac */
    float firstChromaScale; /* +0x2bb0 */
    float secondChromaScale; /* +0x2bb4 */
    float redScale; /* +0x2bb8 */
    float greenScale; /* +0x2bbc */
    float blueScale; /* +0x2bc0 */
} JpegWork;

static JpegWork *sw = (JpegWork *)0x70000000;

extern int F2I(float value);

extern u8 ZigZag[64];

typedef struct JpegHuffCode {
    u16 code;
    s16 length;
} JpegHuffCode;

static void PutBits(u16 code, int length);

/* A DRI marker carries a big-endian restart interval in bytes 2 and 3.
 * The parser stores that value and its successor at +0x2BA0/+0x2BA4,
 * then returns the next segment byte. */

extern float I2F(int value);

/* Each quantization table has 64 floats. DefineQuantizeTable selects
 * luminance for table ID 0 and chrominance otherwise; HuffmanDecode uses
 * the same bases (main:0x224c24/0x224c30) and indexes 64 coefficients. */

static int GetBit(void);

/* Input rows contain sixteen consecutive Y/U/V float triples. */
static void ConvertYUV2MCU(float *planes, const float *pixels, int samplingMode)
{
    int row;
    int column;

    switch (samplingMode) {
    case 0:
        for (row = 0; row < 8; row++) {
            for (column = 0; column < 8; column++) {
                planes[row * 8 + column] = pixels[row * 48 + column * 3] - 128.0f;
                planes[4 * 64 + row * 8 + column] = pixels[row * 48 + column * 3 + 1];
                planes[5 * 64 + row * 8 + column] = pixels[row * 48 + column * 3 + 2];
            }
        }
        return;
    case 1:
        for (row = 0; row < 8; row++) {
            for (column = 0; column < 8; column++) {
                planes[row * 8 + column] = pixels[row * 48 + column * 3] - 128.0f;
                planes[64 + row * 8 + column] = pixels[row * 48 + column * 3 + 24] - 128.0f;
                planes[4 * 64 + row * 8 + column] =
                    (pixels[row * 48 + column * 6 + 1] +
                     pixels[row * 48 + column * 6 + 4]) * 0.5f;
                planes[5 * 64 + row * 8 + column] =
                    (pixels[row * 48 + column * 6 + 2] +
                     pixels[row * 48 + column * 6 + 5]) * 0.5f;
            }
        }
        return;
    case 2:
    default:
        for (row = 0; row < 8; row++) {
            for (column = 0; column < 8; column++) {
                planes[row * 8 + column] = pixels[row * 48 + column * 3] - 128.0f;
                planes[64 + row * 8 + column] = pixels[row * 48 + column * 3 + 24] - 128.0f;
                planes[2 * 64 + row * 8 + column] = pixels[row * 48 + column * 3 + 384] - 128.0f;
                planes[3 * 64 + row * 8 + column] = pixels[row * 48 + column * 3 + 408] - 128.0f;
                planes[4 * 64 + row * 8 + column] =
                    (pixels[row * 96 + column * 6 + 1] +
                     pixels[row * 96 + column * 6 + 4] +
                     pixels[row * 96 + column * 6 + 49] +
                     pixels[row * 96 + column * 6 + 52]) * 0.25f;
                planes[5 * 64 + row * 8 + column] =
                    (pixels[row * 96 + column * 6 + 2] +
                     pixels[row * 96 + column * 6 + 5] +
                     pixels[row * 96 + column * 6 + 50] +
                     pixels[row * 96 + column * 6 + 53]) * 0.25f;
            }
        }
        break;
    }
}

static void FFDCT(float *block)
{
    JpegWork *work = sw;
    float oddWeight1 = D_004D8798;
    float oddWeight3 = D_004D879C;
    float oddWeight5 = D_004D87A0;
    float oddWeight7 = D_004D87A4;
    float evenWeight2 = D_004D87A8;
    float evenWeight6 = D_004D87AC;
    float negativeOddWeight7 = D_004D87B0;
    float negativeOddWeight5 = D_004D87B4;
    float negativeOddWeight3 = D_004D87B8;
    float negativeOddWeight1 = D_004D87BC;
    float negativeEvenWeight6 = D_004D87C0;
    float negativeEvenWeight2 = D_004D87C4;
    float (*bottomRow)[8];
    float *lastCoefficient;
    const float *samples;
    int coordinate;
    int frequency;

    for (coordinate = 0; coordinate < 8; coordinate++) {
        samples = block + coordinate * 8;
        lastCoefficient = &work->idct_block[coordinate][7];
        lastCoefficient[-7] = samples[0] + samples[1] + samples[2] + samples[3] +
                        samples[4] + samples[5] + samples[6] + samples[7];
        lastCoefficient[-6] = samples[0] * oddWeight1 + samples[1] * oddWeight3 +
                        samples[2] * oddWeight5 + samples[3] * oddWeight7 +
                        samples[4] * negativeOddWeight7 + samples[5] * negativeOddWeight5 +
                        samples[6] * negativeOddWeight3 + samples[7] * negativeOddWeight1;
        lastCoefficient[-5] = samples[0] * evenWeight2 + samples[1] * evenWeight6 +
                        samples[2] * negativeEvenWeight6 + samples[3] * negativeEvenWeight2 +
                        samples[4] * negativeEvenWeight2 + samples[5] * negativeEvenWeight6 +
                        samples[6] * evenWeight6 + samples[7] * evenWeight2;
        lastCoefficient[-4] = samples[0] * oddWeight3 + samples[1] * negativeOddWeight7 +
                        samples[2] * negativeOddWeight1 + samples[3] * negativeOddWeight5 +
                        samples[4] * oddWeight5 + samples[5] * oddWeight1 +
                        samples[6] * oddWeight7 + samples[7] * negativeOddWeight3;
        lastCoefficient[-3] = samples[0] - samples[1] - samples[2] + samples[3] +
                        samples[4] - samples[5] - samples[6] + samples[7];
        lastCoefficient[-2] = samples[0] * oddWeight5 + samples[1] * negativeOddWeight1 +
                        samples[2] * oddWeight7 + samples[3] * oddWeight3 +
                        samples[4] * negativeOddWeight3 + samples[5] * negativeOddWeight7 +
                        samples[6] * oddWeight1 + samples[7] * negativeOddWeight5;
        lastCoefficient[-1] = samples[0] * evenWeight6 + samples[1] * negativeEvenWeight2 +
                        samples[2] * evenWeight2 + samples[3] * negativeEvenWeight6 +
                        samples[4] * negativeEvenWeight6 + samples[5] * evenWeight2 +
                        samples[6] * negativeEvenWeight2 + samples[7] * evenWeight6;
        lastCoefficient[0] = samples[0] * oddWeight7 + samples[1] * negativeOddWeight5 +
                        samples[2] * oddWeight3 + samples[3] * negativeOddWeight1 +
                        samples[4] * oddWeight1 + samples[5] * negativeOddWeight3 +
                        samples[6] * oddWeight5 + samples[7] * negativeOddWeight7;
    }

    bottomRow = &work->idct_block[7];
    for (coordinate = 0; coordinate < 8; coordinate++) {
        block[coordinate] = (bottomRow[-7][coordinate] + bottomRow[-6][coordinate] +
                         bottomRow[-5][coordinate] + bottomRow[-4][coordinate] +
                         bottomRow[-3][coordinate] + bottomRow[-2][coordinate] +
                         bottomRow[-1][coordinate] + bottomRow[0][coordinate]) * 0.125f;
        for (frequency = 1; frequency < 8; frequency++) {
            block[frequency * 8 + coordinate] =
                (bottomRow[-7][coordinate] * ffdct_0[frequency][0] +
                 bottomRow[-6][coordinate] * ffdct_0[frequency][1] +
                 bottomRow[-5][coordinate] * ffdct_0[frequency][2] +
                 bottomRow[-4][coordinate] * ffdct_0[frequency][3] +
                 bottomRow[-3][coordinate] * ffdct_0[frequency][4] +
                 bottomRow[-2][coordinate] * ffdct_0[frequency][5] +
                 bottomRow[-1][coordinate] * ffdct_0[frequency][6] +
                 bottomRow[0][coordinate] * ffdct_0[frequency][7]) * 0.125f;
        }
    }
}

static void Quantize(int *quantized, const float *coefficients, const float *reciprocals)
{
    float coefficient;
    float reciprocal;
    float scaled;
    int index;

    for (index = 63; index >= 0; index--) {
        coefficient = *coefficients;
        coefficients++;
        reciprocal = *reciprocals;
        reciprocals++;
        scaled = coefficient * reciprocal;
        if (coefficient < 0.0f) {
            scaled -= 0.5f;
        } else {
            scaled += 0.5f;
        }
        *quantized = F2I(scaled);
        quantized++;
    }
}

static void RebuildQuantizeTable(u8 *quantizers, float *reciprocals,
                                 int chrominance, int quality)
{
    const u8 *original;
    float *reciprocalDest;
    float scale;
    int index;
    int value;

    if (chrominance == 0) {
        original = LQTableOrg_1;
    } else {
        original = CQTableOrg_2;
    }
    scale = (float) quality;
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    if (scale > 100.0f) {
        scale = 100.0f;
    }
    if (scale < 50.0f) {
        scale = 5000.0f / scale;
    } else {
        scale = 200.0f - 2.0f * scale;
    }
    reciprocalDest = reciprocals;
    for (index = 0; index < 64; index++) {
        value = F2I(((float) original[index] * scale + 50.0f) * 0.01f);
        if (value <= 0) {
            value = 1;
        }
        if (value >= 256) {
            value = 255;
        }
        quantizers[index] = value;
        *reciprocalDest++ = 1.0f / I2F(value);
    }
}

static void PutBits(u16 code, int length)
{
    u8 *output = sw->buffer;
    int bits = code;
    int bufferedBits;
    JpegWork *work;

    if (length != 0) {
        bits &= (1 << length) - 1;
        while (length + sw->bitsRemaining >= 8) {
            sw->bufferedByte |= bits >> (length + sw->bitsRemaining - 8);
            output[sw->outputPosition] = sw->bufferedByte;
            if (sw->bufferedByte == 0xFF) {
                sw->outputPosition++;
                output[sw->outputPosition] = 0;
            }
            work = sw;
            bufferedBits = work->bitsRemaining;
            work->bitsRemaining = 0;
            length = length + bufferedBits - 8;
            work->outputPosition++;
            sw->bufferedByte = 0;
        }
        if (length > 0) {
            sw->bufferedByte |= bits << (8 - sw->bitsRemaining - length);
            sw->bitsRemaining += length;
        }
    }
}

static void EncodeDc(int value, JpegHuffCode *table)
{
    int magnitude;
    int negative;
    int remaining;
    int length;
    u16 bits;

    magnitude = value;
    if (magnitude == 0) {
        PutBits(table[0].code, table[0].length);
        return;
    }
    negative = 0;
    if (magnitude < 0) {
        negative = 1;
        magnitude = -magnitude;
    }
    remaining = magnitude;
    length = 0;
    while (remaining != 0) {
        remaining >>= 1;
        length++;
    }
    PutBits(table[length].code, table[length].length);
    if (negative) {
        bits = (u16) (~magnitude & 0xFFFF);
    } else {
        bits = (u16) (magnitude & 0xFFFF);
    }
    PutBits(bits, length);
}

static void EncodeAc(int value, int runIndex, JpegHuffCode *table)
{
    int magnitude;
    int negative;
    int remaining;
    int length;
    u16 bits;

    magnitude = value;
    if (magnitude == 0) {
        PutBits(table[runIndex].code, table[runIndex].length);
        return;
    }

    negative = 0;
    if (magnitude < 0) {
        negative = 1;
        magnitude = -magnitude;
    }

    remaining = magnitude;
    length = 0;
    while (remaining != 0) {
        remaining >>= 1;
        length++;
    }

    PutBits(table[length + runIndex].code, table[length + runIndex].length);

    if (negative) {
        bits = (u16) (~magnitude & 0xFFFF);
    } else {
        bits = (u16) (magnitude & 0xFFFF);
    }
    PutBits(bits, length);
}

static void HuffmanEncode(int *coefficients, JpegHuffCode *dcCodes,
                         JpegHuffCode *acCodes, int *previousDc)
{
    short difference;
    short value;
    short zeroRun;
    int index;

    zeroRun = 0;
    index = 1;
    difference = (short) (coefficients[0] - *previousDc);
    *previousDc += difference;
    EncodeDc(difference, dcCodes);

    for (;;) {
        value = (short) coefficients[ZigZag[index]];
        index++;
        if (value == 0) {
            if (index >= 64) {
                EncodeAc(0, 0, acCodes);
                break;
            }
            zeroRun++;
        } else {
            while (zeroRun >= 16) {
                EncodeAc(0, 0xF0, acCodes);
                zeroRun -= 16;
            }
            EncodeAc(value, zeroRun * 16, acCodes);
            if (index >= 64) {
                break;
            }
            zeroRun = 0;
        }
    }
}

static void ExtractHuffmanTableSub(JpegHuffCode *table, const u8 *symbols, int tableSize)
{
    const u8 *counts;
    int code;
    s16 length;
    int lengthsRemaining;
    int symbolsRemaining;
    JpegHuffCode *entry;

    memset(table, 0, tableSize * 4);
    counts = symbols;
    code = 0;
    length = 1;
    symbols += 16;
    for (lengthsRemaining = 15; lengthsRemaining >= 0; lengthsRemaining--) {
        symbolsRemaining = *counts++;
        while (symbolsRemaining-- > 0) {
            entry = &table[*symbols++];
            entry->code = code;
            code = (code + 1) & 0xFFFF;
            entry->length = length;
        }
        code = (code * 2) & 0xFFFE;
        length++;
    }
}

int xglJpegEncode(JpegEncodeRequest *request)
{
    u8 *header;
    u8 *outputBuffer;
    int y;
    int x;
    int row;
    int column;
    int i;
    int mcuWidth;
    int mcuHeight;
    int quotient;

    sw->rowBytes = request->width * 3;
    RebuildQuantizeTable(sw->unmodeled_02d0, sw->luminanceQuantization, 0, request->luminanceQuality);
    RebuildQuantizeTable(&sw->unmodeled_02d0[0x40], sw->chrominanceQuantization, 1, request->chrominanceQuality);
    ExtractHuffmanTableSub((JpegHuffCode *) sw->luminanceDcCodes, headerALL + 0xB6, 16);
    ExtractHuffmanTableSub((JpegHuffCode *) sw->chrominanceDcCodes, headerALL + 0x18E, 16);
    ExtractHuffmanTableSub((JpegHuffCode *) sw->luminanceAcCodes, headerALL + 0xD7, 256);
    ExtractHuffmanTableSub((JpegHuffCode *) sw->chrominanceAcCodes, headerALL + 0x1AF, 256);

    sw->buffer = request->outputBuffer;
    sw->bitsRemaining = 0;
    sw->bufferedByte = 0;
    __builtin_memcpy(sw->buffer, headerALL, 623);

    sw->outputPosition = 623;
    header = sw->buffer + 14;
    quotient = 0xAF00 / request->height;
    header[1] = quotient;
    header[0] = quotient >> 8;
    quotient = 0xFA00 / request->width;
    header[3] = quotient;
    header[2] = quotient >> 8;

    header = sw->buffer + 25;
    for (i = 0; i < 64; i++) {
        *header = sw->unmodeled_02d0[ZigZag[i]];
        header++;
    }
    header = sw->buffer + 94;
    for (i = 0; i < 64; i++) {
        *header = sw->unmodeled_02d0[0x40 + ZigZag[i]];
        header++;
    }

    switch (request->samplingMode) {
    case 0:
        i = 0x11;
        break;
    case 1:
        i = 0x21;
        break;
    case 2:
    default:
        i = 0x22;
        break;
    }
    header = sw->buffer + 163;
    header[0] = request->height >> 8;
    header[1] = request->height;
    header[2] = request->width >> 8;
    header[3] = request->width;
    header[6] = i;

    sw->dcPredictors[2] = 0;
    sw->dcPredictors[1] = 0;
    sw->dcPredictors[0] = 0;

    switch (request->samplingMode) {
    case 0:
        mcuWidth = 8;
        mcuHeight = mcuWidth;
        break;
    case 1:
        mcuWidth = 16;
        mcuHeight = 8;
        break;
    case 2:
    default:
        mcuWidth = 16;
        mcuHeight = mcuWidth;
        break;
    }

    for (y = 0; y < request->height; y += mcuHeight) {
        for (x = 0; x < request->width; x += mcuWidth) {
            sceGsSetDefStoreImage(&sw->unmodeled_0000[0x60], request->sourceX, request->sourceY, 1,
                                  request->destinationX + x, request->destinationY + y,
                                  ((unsigned) mcuWidth << 16) >> 16, ((unsigned) mcuHeight << 16) >> 16);
            sceGsExecStoreImage(&sw->unmodeled_0000[0x60], (void *) 0x70003000);

            for (row = 0; row < mcuHeight; row++) {
                u8 *rgb = (u8 *) 0x70003000 + row * mcuWidth * 3;
                float *pixel = (float *) sw->decodedYuvPixels + row * 48;
                for (column = 0; column < mcuWidth; column++) {
                    float red = I2F(rgb[0]);
                    float green = I2F(rgb[1]);
                    float blue = I2F(rgb[2]);

                    pixel[0] = red * 0.299f + green * 0.587f + blue * 0.114f;
                    pixel[1] = red * -0.16874f + green * -0.33126f + blue * 0.5f;
                    pixel[2] = red * 0.5f + green * -0.41869f + blue * -0.08131f;
                    rgb += 3;
                    pixel += 3;
                }
            }

            ConvertYUV2MCU(sw->decodedMcuPlanes, (float *) sw->decodedYuvPixels, request->samplingMode);

            switch (request->samplingMode) {
        case 0:
            FFDCT(sw->decodedMcuPlanes + 0);
            FFDCT(sw->decodedMcuPlanes + 256);
            FFDCT(sw->decodedMcuPlanes + 320);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[0], sw->decodedMcuPlanes + 0, sw->luminanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[4], sw->decodedMcuPlanes + 256, sw->chrominanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[5], sw->decodedMcuPlanes + 320, sw->chrominanceQuantization);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[0], (JpegHuffCode *) sw->luminanceDcCodes,
                          (JpegHuffCode *) sw->luminanceAcCodes, &sw->dcPredictors[0]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[4], (JpegHuffCode *) sw->chrominanceDcCodes,
                          (JpegHuffCode *) sw->chrominanceAcCodes, &sw->dcPredictors[1]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[5], (JpegHuffCode *) sw->chrominanceDcCodes,
                          (JpegHuffCode *) sw->chrominanceAcCodes, &sw->dcPredictors[2]);
            break;
        case 1:
            FFDCT(sw->decodedMcuPlanes + 0);
            FFDCT(sw->decodedMcuPlanes + 64);
            FFDCT(sw->decodedMcuPlanes + 256);
            FFDCT(sw->decodedMcuPlanes + 320);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[0], sw->decodedMcuPlanes + 0, sw->luminanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[1], sw->decodedMcuPlanes + 64, sw->luminanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[4], sw->decodedMcuPlanes + 256, sw->chrominanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[5], sw->decodedMcuPlanes + 320, sw->chrominanceQuantization);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[0], (JpegHuffCode *) sw->luminanceDcCodes,
                          (JpegHuffCode *) sw->luminanceAcCodes, &sw->dcPredictors[0]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[1], (JpegHuffCode *) sw->luminanceDcCodes,
                          (JpegHuffCode *) sw->luminanceAcCodes, &sw->dcPredictors[0]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[4], (JpegHuffCode *) sw->chrominanceDcCodes,
                          (JpegHuffCode *) sw->chrominanceAcCodes, &sw->dcPredictors[1]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[5], (JpegHuffCode *) sw->chrominanceDcCodes,
                          (JpegHuffCode *) sw->chrominanceAcCodes, &sw->dcPredictors[2]);
            break;
        case 2:
        default:
            FFDCT(sw->decodedMcuPlanes + 0);
            FFDCT(sw->decodedMcuPlanes + 64);
            FFDCT(sw->decodedMcuPlanes + 128);
            FFDCT(sw->decodedMcuPlanes + 192);
            FFDCT(sw->decodedMcuPlanes + 256);
            FFDCT(sw->decodedMcuPlanes + 320);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[0], sw->decodedMcuPlanes + 0, sw->luminanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[1], sw->decodedMcuPlanes + 64, sw->luminanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[2], sw->decodedMcuPlanes + 128, sw->luminanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[3], sw->decodedMcuPlanes + 192, sw->luminanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[4], sw->decodedMcuPlanes + 256, sw->chrominanceQuantization);
            Quantize(((int (*)[64]) sw->decodedYuvPixels)[5], sw->decodedMcuPlanes + 320, sw->chrominanceQuantization);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[0], (JpegHuffCode *) sw->luminanceDcCodes,
                          (JpegHuffCode *) sw->luminanceAcCodes, &sw->dcPredictors[0]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[1], (JpegHuffCode *) sw->luminanceDcCodes,
                          (JpegHuffCode *) sw->luminanceAcCodes, &sw->dcPredictors[0]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[2], (JpegHuffCode *) sw->luminanceDcCodes,
                          (JpegHuffCode *) sw->luminanceAcCodes, &sw->dcPredictors[0]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[3], (JpegHuffCode *) sw->luminanceDcCodes,
                          (JpegHuffCode *) sw->luminanceAcCodes, &sw->dcPredictors[0]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[4], (JpegHuffCode *) sw->chrominanceDcCodes,
                          (JpegHuffCode *) sw->chrominanceAcCodes, &sw->dcPredictors[1]);
            HuffmanEncode(((int (*)[64]) sw->decodedYuvPixels)[5], (JpegHuffCode *) sw->chrominanceDcCodes,
                          (JpegHuffCode *) sw->chrominanceAcCodes, &sw->dcPredictors[2]);
            break;
            }
        }
    }

    if (sw->bitsRemaining != 0) {
        sw->buffer[sw->outputPosition] = sw->bufferedByte;
        sw->outputPosition++;
    }
    sw->buffer[sw->outputPosition] = 0xFF;
    sw->buffer[sw->outputPosition + 1] = 0xD9;
    sw->outputPosition += 2;
    request->outputSize = sw->outputPosition;
    return 0;
}

static void *DefineRestartInterval(void *marker)
{
    u8 *segment = marker;
    JpegWork *work = sw;
    u32 interval;

    interval = (segment[2] << 8) + segment[3];
    work->restart_interval = interval;
    work->restart_interval_plus_one = interval + 1;
    return segment + 4;
}

static u8 *DefineQuantizeTable(u8 *marker)
{
    u8 *segment;
    u16 count;
    float *dest;
    u8 value;

    segment = marker;
    count = (segment[0] << 8) + segment[1] - 3;
    if (segment[2] == 0) {
        dest = sw->luminanceQuantization;
    } else {
        dest = sw->chrominanceQuantization;
    }
    segment += 3;

    while (count != 0) {
        value = segment[0];
        segment += 1;
        *dest = I2F(value);
        dest += 1;
        count--;
    }
    return segment;
}

static u8 *StartOfFrame(u8 *segment)
{
    JpegWork *work = sw;
    int width;

    work->imageHeight = segment[4] + (segment[3] << 8);
    width = segment[6] + (segment[5] << 8);
    work->imageWidth = width;
    work->rowBytes = (short) width * 3;
    switch (segment[9]) {
    case 0x11:
        work->samplingMode = 0;
        break;
    case 0x21:
        work->samplingMode = 1;
        break;
    case 0x22:
    default:
        work->samplingMode = 2;
        break;
    }
    return segment + ((segment[0] << 8) + segment[1]);
}

static u8 *DefineHuffmanTable(u8 *segment)
{
    JpegWork *work;
    u8 *cursor = segment;
    u8 *end = cursor + ((cursor[0] << 8) + cursor[1]);
    u8 *counts;
    s16 *entry;
    int code;
    int length;
    int count;

    cursor += 2;
    if (cursor < end) {
        work = sw;
        do {
            switch (*cursor) {
            case 0:
                entry = work->luminanceDcCodes;
                break;
            case 1:
                entry = work->chrominanceDcCodes;
                break;
            case 0x10:
                entry = work->luminanceAcCodes;
                break;
            default:
                entry = work->chrominanceAcCodes;
                break;
            }
            cursor++;
            code = 0;
            counts = cursor;
            cursor += 16;
            for (length = 0; length < 16; length++) {
                count = *counts++;
                while (count-- > 0) {
                    entry[0] = code;
                    code = (code + 1) & 0xFFFF;
                    entry[1] = *cursor++;
                    entry += 2;
                }
                /* The decoder recognizes 0xFE as a code-length boundary. */
                entry[1] = 0xFE;
                entry += 2;
                code = (code * 2) & 0xFFFE;
            }
            entry[1] = 0xFF;
        } while (cursor < end);
    }
    return cursor;
}

static int GetBit(void)
{
    u8 *buffer;
    JpegWork *work;
    u8 value;

    if (sw->bitsRemaining == 0) {
        work = sw;
        buffer = work->buffer;
        value = *buffer;
        if (value == 0xFF) {
            work->buffer = buffer + 1;
        }
        work->bufferedByte = value;
        work->buffer++;
        sw->bitsRemaining = 7;
    } else {
        sw->bitsRemaining--;
    }
    return (sw->bufferedByte >> sw->bitsRemaining) & 1;
}

static s16 decode_sub(s16 *codes)
{
    u32 code = 0;
    s16 terminator = 0xFE;

    for (;;) {
        code = code * 2 + GetBit();
        if (codes[1] != terminator) {
            do {
                if ((u16) codes[0] == code) {
                    return codes[1];
                }
                codes += 2;
            } while (codes[1] != terminator);
        }
        codes += 2;
    }
}

static void HuffmanDecode(float *coefficients, int component)
{
    JpegWork *work;
    s16 *dcCodes;
    s16 *acCodes;
    float *quantization;
    int *predictor;
    int value;
    int bits;
    int dcBias;
    int acBias;
    int position;

    switch (component) {
    case 0:
    default:
        work = sw;
        dcCodes = work->luminanceDcCodes;
        acCodes = work->luminanceAcCodes;
        quantization = work->luminanceQuantization;
        break;
    case 1:
    case 2:
        work = sw;
        dcCodes = work->chrominanceDcCodes;
        acCodes = work->chrominanceAcCodes;
        quantization = work->chrominanceQuantization;
        break;
    }

    value = 0;
    predictor = &work->dcPredictors[component];
    bits = decode_sub(dcCodes);
    if (bits != 0) {
        if (GetBit() == 0) {
            dcBias = (1 << bits) - 1;
        } else {
            dcBias = 0;
            value = 1;
        }
        bits--;
        while (bits > 0) {
            bits--;
            value = value * 2 + GetBit();
        }
        value -= dcBias;
    }
    position = 1;
    value += *predictor;
    *predictor = value;
    coefficients[0] = I2F(value) * quantization[0];

    do {
        value = 0;
        bits = decode_sub(acCodes);
        if (bits != 0) {
            while (bits >= 16) {
                coefficients[ZigZag[position++]] = 0.0f;
                bits -= 16;
            }
            if (bits != 0) {
                if (GetBit() == 0) {
                    value = 0;
                    acBias = (1 << bits) - 1;
                } else {
                    acBias = 0;
                    value = 1;
                }
                bits--;
                while (bits > 0) {
                    bits--;
                    value = value * 2 + GetBit();
                }
                value -= acBias;
            }
            coefficients[ZigZag[position]] = I2F(value) * quantization[position];
        } else {
            while (position < 64)
                coefficients[ZigZag[position++]] = 0.0f;
        }
        position++;
    } while (position < 64);
}

/*
 * Two-dimensional inverse DCT of one 8x8 block of floating-point coefficients,
 * in place.
 *
 * StartOfScan leaves the one-dimensional transform in the VU0 register file and
 * it stays there for every block of the scan: vf1-vf8 for the first pass and
 * vf11-vf18, the same rows scaled through I, for the second.  A pass therefore
 * reduces to one matrix-by-vector product per line, which is the only part of
 * this routine that cannot be written in C.  The even-indexed samples of the
 * line go to vf9 and the odd-indexed ones to vf10, and the accumulator chain
 * multiplies them by the transform; what VU0 returns is the butterfly's four
 * sums and four differences, which the scalar code below combines into the
 * eight output samples.
 *
 * The first pass reads the columns of the caller's block and writes the rows of
 * the decoder's intermediate block; the second pass reads the columns of that
 * intermediate block and writes the rows of the caller's block back.
 */
static void FFIDCT(float block[8][8])
{
    JpegWork *work;
    float *column;
    float *stage;
    float *line;
    float *out;
    int i;

    stage = sw->idct_stage;

    for (i = 0; i < 8; i++) {
        stage[0] = block[0][i];
        stage[1] = block[2][i];
        stage[2] = block[4][i];
        stage[3] = block[6][i];
        stage[4] = block[1][i];
        stage[5] = block[3][i];
        stage[6] = block[5][i];
        stage[7] = block[7][i];
        __asm__ __volatile__(
            "lqc2 vf9,0(%0)\n"
            "lqc2 vf10,16(%0)\n"
            "vmulax.xyzw ACCxyzw,vf1xyzw,vf9x\n"
            "vmadday.xyzw ACCxyzw,vf2xyzw,vf9y\n"
            "vmaddaz.xyzw ACCxyzw,vf3xyzw,vf9z\n"
            "vmaddw.xyzw vf19xyzw,vf4xyzw,vf9w\n"
            "vmulax.xyzw ACCxyzw,vf5xyzw,vf10x\n"
            "vmadday.xyzw ACCxyzw,vf6xyzw,vf10y\n"
            "vmaddaz.xyzw ACCxyzw,vf7xyzw,vf10z\n"
            "vmaddw.xyzw vf20xyzw,vf8xyzw,vf10w\n"
            "sqc2 vf19,0(%0)\n"
            "sqc2 vf20,16(%0)\n"
            :
            : "r"(stage)
            : "memory");
        work = sw;
        line = work->idct_block[i];
        line[0] = stage[0] + stage[2] + stage[4];
        line[1] = stage[1] + stage[3] + stage[5];
        line[2] = stage[1] - stage[3] + stage[6];
        line[3] = stage[0] - stage[2] + stage[7];
        line[4] = stage[0] - stage[2] - stage[7];
        line[5] = stage[1] - stage[3] - stage[6];
        line[6] = stage[1] + stage[3] - stage[5];
        line[7] = stage[0] + stage[2] - stage[4];
    }

    /*
     * The work-area pointer the second pass reads is the one the first pass
     * left behind, so the first column starts without reloading it; every
     * later column re-reads sw, because the VU0 block above clobbers memory.
     * The original's loop is entered past that reload for exactly that reason
     * (see the style note in the attempt report: the structured spelling of
     * this loop compiles to a strength-reduced destination walk instead of the
     * original's recomputed sw + 5232 + i * 4 and block + i * 32).
     */
    i = 0;
    goto transform_column;
reload_work:
    work = sw;
    for (;;) {
transform_column:
        column = &work->idct_block[0][i];
        stage[0] = column[0 * 8];
        stage[1] = column[2 * 8];
        stage[2] = column[4 * 8];
        stage[3] = column[6 * 8];
        stage[4] = column[1 * 8];
        stage[5] = column[3 * 8];
        stage[6] = column[5 * 8];
        stage[7] = column[7 * 8];
        __asm__ __volatile__(
            "lqc2 vf9,0(%0)\n"
            "lqc2 vf10,16(%0)\n"
            "vmulax.xyzw ACCxyzw,vf11xyzw,vf9x\n"
            "vmadday.xyzw ACCxyzw,vf12xyzw,vf9y\n"
            "vmaddaz.xyzw ACCxyzw,vf13xyzw,vf9z\n"
            "vmaddw.xyzw vf19xyzw,vf14xyzw,vf9w\n"
            "vmulax.xyzw ACCxyzw,vf15xyzw,vf10x\n"
            "vmadday.xyzw ACCxyzw,vf16xyzw,vf10y\n"
            "vmaddaz.xyzw ACCxyzw,vf17xyzw,vf10z\n"
            "vmaddw.xyzw vf20xyzw,vf18xyzw,vf10w\n"
            "sqc2 vf19,0(%0)\n"
            "sqc2 vf20,16(%0)\n"
            :
            : "r"(stage)
            : "memory");
        out = block[i];
        out[0] = stage[0] + stage[2] + stage[4];
        out[1] = stage[1] + stage[3] + stage[5];
        out[2] = stage[1] - stage[3] + stage[6];
        out[3] = stage[0] - stage[2] + stage[7];
        out[4] = stage[0] - stage[2] - stage[7];
        out[5] = stage[1] - stage[3] - stage[6];
        out[6] = stage[1] + stage[3] - stage[5];
        out[7] = stage[0] + stage[2] - stage[4];
        if (++i >= 8) {
            break;
        }
        goto reload_work;
    }
}

static void ConvertMCU2YUV(void)
{
    JpegWork *work = sw;
    float *planes = work->decodedMcuPlanes;
    JpegYuvPixel *pixels = work->decodedYuvPixels;
    int row;
    int column;

    switch (work->samplingMode) {
    case 0:
        for (row = 0; row < 8; row++) {
            for (column = 0; column < 8; column++) {
                pixels[row * 16 + column].luminance = planes[row * 8 + column];
                pixels[row * 16 + column].firstChroma = planes[4 * 64 + row * 8 + column];
                pixels[row * 16 + column].secondChroma = planes[5 * 64 + row * 8 + column];
            }
        }
        return;
    case 1:
        for (row = 0; row < 8; row++) {
            for (column = 0; column < 8; column++) {
                pixels[row * 16 + column].luminance = planes[row * 8 + column];
                pixels[row * 16 + column + 8].luminance = planes[64 + row * 8 + column];
                pixels[row * 16 + column * 2].firstChroma =
                    pixels[row * 16 + column * 2 + 1].firstChroma =
                    planes[4 * 64 + row * 8 + column];
                pixels[row * 16 + column * 2].secondChroma =
                    pixels[row * 16 + column * 2 + 1].secondChroma =
                    planes[5 * 64 + row * 8 + column];
            }
        }
        return;
    case 2:
    default:
        for (row = 0; row < 8; row++) {
            for (column = 0; column < 8; column++) {
                pixels[row * 16 + column].luminance = planes[row * 8 + column];
                pixels[row * 16 + column + 8].luminance = planes[64 + row * 8 + column];
                pixels[row * 16 + column + 128].luminance = planes[2 * 64 + row * 8 + column];
                pixels[row * 16 + column + 136].luminance = planes[3 * 64 + row * 8 + column];
                pixels[row * 32 + column * 2].firstChroma =
                    pixels[row * 32 + column * 2 + 1].firstChroma =
                    pixels[row * 32 + column * 2 + 16].firstChroma =
                    pixels[row * 32 + column * 2 + 17].firstChroma =
                    planes[4 * 64 + row * 8 + column];
                pixels[row * 32 + column * 2].secondChroma =
                    pixels[row * 32 + column * 2 + 1].secondChroma =
                    pixels[row * 32 + column * 2 + 16].secondChroma =
                    pixels[row * 32 + column * 2 + 17].secondChroma =
                    planes[5 * 64 + row * 8 + column];
            }
        }
        break;
    }
}

static void ConvertYUV2RGB(int width, int height)
{
    int row = 0;
    u32 *rgb = (u32 *)0x70003000;

    if (height > 0) {
        do {
            JpegWork *work = sw;
            JpegYuvPixel *pixel = work->decodedYuvPixels + row * 16;

            if (width > 0) {
                int remaining = width;

                do {
                    /* StartOfScan installs VF21-VF26; FFIDCT leaves them live. */
                    __asm__ __volatile__(
                        "lqc2 vf10, 0(%0)\n\t"
                        "vmulax.xyzw ACCxyzw,vf21xyzw,vf10x\n\t"
                        "vmadday.xyzw ACCxyzw,vf22xyzw,vf10y\n\t"
                        "vmaddaz.xyzw ACCxyzw,vf23xyzw,vf10z\n\t"
                        "vmaddw.xyzw vf9xyzw,vf24xyzw,vf0w\n\t"
                        "qmfc2 $8,vf25\n\t"
                        "vftoi0.xyzw vf10xyzw,vf9xyzw\n\t"
                        "qmfc2 $9,vf26\n\t"
                        "qmfc2 $2,vf10\n\t"
                        "pmaxw $3,$2,$8\n\t"
                        "pminw $2,$3,$9\n\t"
                        "ppach $3,$0,$2\n\t"
                        "ppacb $2,$0,$3\n\t"
                        "sw $2,0(%1)"
                        :
                        : "r"(pixel), "r"(rgb)
                        : "$2", "$3", "$8", "$9", "memory");
                    remaining--;
                    pixel++;
                    rgb++;
                } while (remaining != 0);
            }
            row++;
        } while (row < height);
    }
}

static u8 *StartOfScan(u8 *segment, void *requestAddress)
{
    JpegDecodeControl *request = requestAddress;
    int mcuWidth;
    int mcuHeight;
    int mcuX;
    int mcuY;
    int texelRow;
    u8 *destCursor;
    u8 *stagedRgba;

    /* VU0 setup: the I register is loaded with 0.125f through $2 (hence the clobber),
     * vf1-vf8 are scaled into vf11-vf18 and vf21-vf26 are loaded. */
    __asm__ __volatile__(
        "lui $2,0x3e00\n\t"
        "ctc2 $2,$vi21\n\t"
        "lqc2 vf1,0(%0)\n\t"
        "lqc2 vf2,16(%0)\n\t"
        "lqc2 vf3,32(%0)\n\t"
        "lqc2 vf4,48(%0)\n\t"
        "lqc2 vf5,64(%0)\n\t"
        "lqc2 vf6,80(%0)\n\t"
        "lqc2 vf7,96(%0)\n\t"
        "lqc2 vf8,112(%0)\n\t"
        "vmuli.xyzw vf11xyzw,vf1xyzw,I\n\t"
        "vmuli.xyzw vf12xyzw,vf2xyzw,I\n\t"
        "vmuli.xyzw vf13xyzw,vf3xyzw,I\n\t"
        "vmuli.xyzw vf14xyzw,vf4xyzw,I\n\t"
        "vmuli.xyzw vf15xyzw,vf5xyzw,I\n\t"
        "vmuli.xyzw vf16xyzw,vf6xyzw,I\n\t"
        "vmuli.xyzw vf17xyzw,vf7xyzw,I\n\t"
        "vmuli.xyzw vf18xyzw,vf8xyzw,I\n\t"
        :
        : "r"(imm0_3)
        : "$2", "memory");
    __asm__ __volatile__(
        "lqc2 vf21,0(%0)\n\t"
        "lqc2 vf22,16(%0)\n\t"
        "lqc2 vf23,32(%0)\n\t"
        "lqc2 vf24,48(%0)\n\t"
        "lqc2 vf25,64(%0)\n\t"
        "lqc2 vf26,80(%0)\n\t"
        :
        : "r"(imm1_4)
        : "memory");

    /* Skip the scan header (length, component count, selectors, spectral range). */
    segment += 12;
    sw->buffer = segment; /* cursor into the entropy-coded data */
    sw->bitsRemaining = 0; /* bit reader starts byte-aligned */
    sw->bufferedByte = 0; /* nothing buffered from the stream yet */
    sw->dcPredictors[2] = 0;
    sw->dcPredictors[1] = 0;
    sw->dcPredictors[0] = 0;

    /* MCU size: 8x8 (mode 0), 16x8 (mode 1) or 16x16 (mode 2). */
    switch (sw->samplingMode) {
    case 0:
        mcuWidth = 8;
        mcuHeight = 8;
        break;
    case 1:
        mcuWidth = 16;
        mcuHeight = 8;
        break;
    case 2:
    default:
        mcuWidth = 16;
        mcuHeight = 16;
        break;
    }

    /* Decode the image one MCU at a time, left to right, top to bottom. */
    for (mcuY = 0; mcuY < sw->imageHeight; mcuY += mcuHeight) {
        for (mcuX = 0; mcuX < sw->imageWidth; mcuX += mcuWidth) {
            u32 restartPeriod = sw->restart_interval;
            /* A restart interval resets the bit reader and the DC predictors. */
            if (restartPeriod != 0 && --sw->restart_interval_plus_one == 0) {
                sw->restart_interval_plus_one = restartPeriod; /* restart the interval countdown */
                sw->buffer += 2; /* step over the RSTn marker */
                sw->bitsRemaining = 0; /* no bits left in the current byte */
                sw->dcPredictors[2] = 0;
                sw->dcPredictors[1] = 0;
                sw->dcPredictors[0] = 0;
            }

            /* Block order: component 0 (1, 2 or 4 blocks), then components 1 and 2. */
            HuffmanDecode(sw->decodedMcuPlanes, 0);
            if (sw->samplingMode > 0) {
                HuffmanDecode(&sw->decodedMcuPlanes[64], 0);
            }
            if (sw->samplingMode == 2) {
                HuffmanDecode(&sw->decodedMcuPlanes[128], 0);
                HuffmanDecode(&sw->decodedMcuPlanes[192], 0);
            }
            HuffmanDecode(&sw->decodedMcuPlanes[256], 1);
            HuffmanDecode(&sw->decodedMcuPlanes[320], 2);

            /* MCUs outside the requested crop window are decoded but not converted or stored. */
            if (mcuX <= sw->maximumX && mcuY <= sw->maximumY) {
                /* Inverse DCT of each decoded 8x8 block, in decode order. */
                FFIDCT((float (*)[8])sw->decodedMcuPlanes);
                if (sw->samplingMode > 0) {
                    FFIDCT((float (*)[8])&sw->decodedMcuPlanes[64]);
                }
                if (sw->samplingMode == 2) {
                    FFIDCT((float (*)[8])&sw->decodedMcuPlanes[128]);
                    FFIDCT((float (*)[8])&sw->decodedMcuPlanes[192]);
                }
                FFIDCT((float (*)[8])&sw->decodedMcuPlanes[256]);
                FFIDCT((float (*)[8])&sw->decodedMcuPlanes[320]);
                /* Gather the IDCT output into per-pixel YUV, then convert to RGBA at 0x70003000. */
                ConvertMCU2YUV();
                ConvertYUV2RGB(mcuWidth, mcuHeight);

                /* No destination buffer: upload the MCU to the GS (frame buffer base and width come from the request). */
                if (request->destination == 0) {
                    /* The load-image packet lives in the work area; the MCU is read from 0x70003000. */
                    sceGsSetDefLoadImage(sw, request->frameBufferBase, request->frameBufferWidth, 0,
                                         (short)(request->originX + mcuX), /* GS x */
                                         (short)(request->originY + mcuY), /* GS y */
                                         (short)mcuWidth, (short)mcuHeight); /* upload size: one MCU */
                    sceGsExecLoadImage(sw, (void *)0x70003000);
                } else if (request->pixelFormat & 1) {
                /* 16-bit RGB555 destination: pack each RGBA texel. */
                    for (texelRow = 0; texelRow < mcuHeight; texelRow++) {
                        int texelsLeft;

                        stagedRgba = (u8 *)0x70003000 + texelRow * (mcuWidth * 4);
                        destCursor = request->destination + ((mcuY + texelRow) * sw->imageWidth + mcuX) * 2;
                        texelsLeft = sw->imageWidth < mcuX + mcuWidth ? sw->imageWidth - mcuX : mcuWidth;
                        for (; texelsLeft > 0; texelsLeft--) {
                            int green555 = (stagedRgba[1] >> 3) << 5;
                            int blue555 = (stagedRgba[2] >> 3) << 10;

                            *(u16 *)destCursor = (u16)((stagedRgba[0] >> 3) + (blue555 + green555));
                            destCursor += 2; /* advance one RGB555 pixel (2 bytes) */
                            stagedRgba += 4; /* advance one RGBA texel (4 bytes) */
                        }
                    }
                } else {
                    /* 32-bit destination: copy each MCU row with memcpy. */
                    destCursor = request->destination + (mcuY * sw->imageWidth + mcuX) * 4;
                    stagedRgba = (u8 *)0x70003000;

                    for (texelRow = 0; texelRow < mcuHeight; texelRow++) {
                        if (sw->imageWidth < mcuX + mcuWidth) {
                            /* Clip the last MCU mcuX to the image width. */
                            memcpy(destCursor, stagedRgba, (sw->imageWidth - mcuX) * 4);
                        } else {
                            memcpy(destCursor, stagedRgba, mcuWidth * 4);
                        }
                        destCursor += sw->imageWidth * 4; /* advance one destination row (imageWidth texels) */
                        stagedRgba += mcuWidth * 4; /* advance one staged row (mcuWidth texels) */
                    }
                }
            }
        }
    }

    /* Scan for the next marker: a 0xFF that is not a stuffed 0x00 byte. The
     * returned pointer addresses that 0xFF. */
    for (;;) {
        while (*segment++ != 0xFF) {
        }
        if (*segment != 0) {
            return segment - 1; /* return the address of the marker byte */
        }
    }
}

int xglJpegDecode(void *requestAddress)
{
    JpegDecodeControl *request = requestAddress;
    JpegWork *work;
    u32 extentBits;
    u8 *cursor;
    u8 marker;
    float yuvPercentScale;
    float rgbPercentScale;
    float chrominanceScale;

    /* The complete dimension pair is zero for an unbounded image. */
    __builtin_memcpy(&extentBits, &request->imageLimits, sizeof(extentBits));
    if (extentBits == 0) {
        work = sw;
        work->maximumY = work->maximumX = 0x7FFF;
    } else {
        work = sw;
        work->maximumX = request->imageLimits.width - 1;
        work->maximumY = request->imageLimits.height - 1;
    }
    work->restart_interval = 0;
    work->colorScaleMode = request->scaleMode;
    work->blueScale = work->greenScale = work->redScale =
        work->secondChromaScale = work->firstChromaScale =
        work->luminanceScale = 1.0f;

    switch (request->scaleMode) {
    case 1:
        yuvPercentScale = 0.01f;
        sw->luminanceScale = I2F(request->luminancePercent) * yuvPercentScale;
        chrominanceScale = I2F(request->chrominancePercent) * yuvPercentScale;
        sw->firstChromaScale = sw->secondChromaScale = chrominanceScale;
        break;
    case 2:
        rgbPercentScale = 0.01f;
        sw->redScale = I2F(request->redPercent) * rgbPercentScale;
        sw->greenScale = I2F(request->greenPercent) * rgbPercentScale;
        sw->blueScale = I2F(request->bluePercent) * rgbPercentScale;
        break;
    default:
        break;
    }
    cursor = request->source;
    for (;;) {
        while (*cursor++ != 0xFF) {
        }
        marker = *cursor;
        if (marker == 0xD0 || marker == 0xD9)
            break;
        cursor++;
        switch (marker) {
        case 0xD8:
            break;
        case 0xDD:
            cursor = DefineRestartInterval(cursor);
            break;
        case 0xDB:
            cursor = DefineQuantizeTable(cursor);
            break;
        case 0xC0:
            cursor = StartOfFrame(cursor);
            break;
        case 0xC4:
            cursor = DefineHuffmanTable(cursor);
            break;
        case 0xDA:
            cursor = StartOfScan(cursor, requestAddress);
            break;
        default:
            cursor += (cursor[0] << 8) + cursor[1];
            break;
        }
    }
    request->imageLimits.width = sw->imageWidth;
    request->imageLimits.height = sw->imageHeight;
    return 0;
}
