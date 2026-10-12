#include "common.h"

#include "jc_face.h"

INCLUDE_ASM("asm/main/nonmatchings/jc_face", JC_face);

INCLUDE_ASM("asm/main/nonmatchings/jc_face", JC_setParent);

INCLUDE_ASM("asm/main/nonmatchings/jc_face", JC_setParent2);

static inline void copyJointMatrix(JntMatrix *destination,
                                   const JntMatrix *source)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        "lq $2, 16(%1)\n"
        "sq $2, 16(%0)\n"
        "lq $2, 32(%1)\n"
        "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n"
        "sq $2, 48(%0)\n"
        : : "r"(destination), "r"(source)
        : "$2", "memory");
}

static inline void copyChainSlot(JntWork *work, const JcChain *chain,
                                 const JntMatrix *records, unsigned int slot)
{
    copyJointMatrix(&work->matrix_buffer[chain->index + slot],
                    &records[chain->static_index + slot]);
}

void *JC_copyChain(JntWork *work, void *element, JcChain *chain)
{
    const JntMatrix *records = *chain->source;
    void *cursor;
    unsigned int flags_after_element;
    int bound;
    unsigned int slot;

    if (element == 0) {
        copyJointMatrix(&work->matrix_buffer[chain->index],
                        &records[chain->static_index]);
        work->flags |= 8;
        return 0;
    }
    work->flags |= 0x18;
    JNT_getElement(element, chain->index);
    flags_after_element = work->flags;
    cursor = element;

    if (flags_after_element & 0x20) {
        bound = chain->index + 1;
        while (work->current_index < bound)
            cursor = JNT_getStaticVal2(work, cursor);
        bound = work->hair_id_source->channel_limit;
        slot = 0;
        copyJointMatrix(&work->matrix_buffer[0], &records[0]);
        while (work->current_index < bound) {
            copyChainSlot(work, chain, records, slot);
            cursor = JNT_nextElement(cursor);
            slot++;
            work->current_index++;
        }
    } else {
        bound = chain->index + 1;
        while (work->current_index < bound)
            cursor = JNT_getStaticVal2(work, cursor);
        copyJointMatrix(&work->matrix_buffer[0], &records[0]);
        bound = work->hair_id_source->channel_limit;
        slot = 0;
        while (work->current_index < bound) {
            copyChainSlot(work, chain, records, slot);
            cursor = JNT_nextElement(cursor);
            slot++;
            work->current_index++;
        }
    }
    work->flags &= ~0x40;
    return cursor;
}
