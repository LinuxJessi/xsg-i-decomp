#ifndef INCLUDE_MAIN_SCRIPT_H
#define INCLUDE_MAIN_SCRIPT_H

/*
 * Queue a funcObserver task that invokes the no-argument script method
 * `method_name` ("()V") on the current Stage object, then run it once.
 * Every caller ignores $v0, so the entry point returns nothing.
 */
extern void CallMethod(const char *method_name);

int SCRIPT_getCfTime(void);

void SCRIPT_execTalkto(void *actor);

int SCRIPT_load2(const char *path);

#endif /* INCLUDE_MAIN_SCRIPT_H */
