#ifndef INCLUDE_MAIN_JTHREAD_H
#define INCLUDE_MAIN_JTHREAD_H

#include "main/jni.h"

/* The actual JNI thread owns object+0x10, method+0x18 and flags+0x24.
 * This owner uses that same record through its historical handle name. */
typedef JThread JThreadHandle;

void JTHREAD_defaultUnit(JThreadHandle *thread);

#endif /* INCLUDE_MAIN_JTHREAD_H */
