#include "common.h"

#include "shared.h"
#include "main/jni.h"

#include "main/xgl_cd.h"
#include "main/xgl_thread.h"
#include "main/xgl_2.h"

/* GNU EE native TI storage type: one 16-byte store clears the debug menu storage. */
typedef unsigned int Quadword __attribute__((mode(TI)));

#include "script.h"

extern XglTaskPrefix *xglTaskEntryNext(XglTaskScheduler *scheduler,
                                       int (*callback)(XglTaskPrefix *task),
                                       XglTaskPrefix *entry);

extern int xglTaskRemove(XglTaskPrefix *task);

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

extern SceneString *loadConstString(const char *bytes, int length);

extern SceneMethod *findMethod(SceneClass *scene_class, SceneString *name,
                               void *signature_or_type);

extern void JNI_callMethod(SceneVm *vm, SceneMethod *method,
                           SceneObject *arguments, int *output);

extern int JNI_isInstanceOf(SceneObject object, SceneClass *target_class);

extern void JNI_initThread(SceneVm *vm);

extern void JNI_catchException(void);

extern int getScriptFlag(SceneObject object);

extern void XTK_setWindowOwner(int owner);

extern void talkCancel(ScriptObserverTask *task);

extern void actTalkAfter(SceneObject object);

extern void initVM(void);

extern void nmlModelSetFadeOut(int frames, int option);

extern void nmlModelSetFadeIn(int frames, int option);

extern void FX_ScreenMask(int unused, int color, int frames, int mode);

extern void nmlModelSetBackBufferToBattle(int owner_index);

extern void nmlModelSetFadeInCancel(int frames);

extern void nmlModelSetFadeOutCancel(int frames);

extern void nmlModelSetEffectWrite(int enabled);

extern void nmlModelSendSignalEventFinish(void);

extern void nmlModelSetGlobalPointLightReset(void);

extern void MAP_initUnit(void);

extern void ACT_initScene(void);

extern void ACT_initMTNResource(void);

extern void TWSYS_init(void);

extern void MSG_init(void);

extern void xglRenderClearColor(u32 color);

extern void SCENE_cleanup(SceneVm *vm, SceneObject scene_object);

extern void xglStudioGetCamera(StudioCamera **camera_out, int camera_index);

extern void xglCameraClipRangeDefault(StudioCamera *camera);

SceneThread *defaultVM;

extern SceneThread *JNI_createThread(int kind, int stack_words,
                                     int frame_words);

extern void JNI_pushFrame(void);

extern void JNI_loadNativeClass(void);

int UseVMFlag;

SceneThread *stageVM;

SceneThread *evtVM[2];

extern void createTalkTask(void *actor, const char *method_name);

extern void sceneObserver2(ScriptSceneTask *task);

extern const char sig0_0[];

extern const char sig1_1[];

extern void Before_Talk(Actor *actor);

extern void TWIN_initCF(void *window);

extern SceneWindow *TWIN_create2(int component_id);

void talktoObserver(ScriptObserverTask *task);

const char D_004C1D50[] = "setColor";

const char D_004C1D60[] = "setDirection2";

#define call_method_signature_void sig_2

#define call_method_signature_int sig_3

#define call_method_signature_int_int sig_4

/* EUC-JP text below means "event in progress". */

/* JP: イベント実行中
 * EN: Event running */
const char func_observer_debug_text[16] =
    "\xa5\xa4\xa5\xd9\xa5\xf3\xa5\xc8\xbc\xc2\xb9\xd4\xc3\xe6";

extern void funcObserver(ScriptObserverTask *task);

int getEmptyVM(ScriptObserverTask *observer);

extern ScriptPadRecord PadData[2];

static unsigned int s_nScriptFadeOutTime = 30;

static unsigned int s_nScriptFadeRequest = 0;

static unsigned char s_nScriptChangeTime = 0;

static unsigned int s_nScriptSequenceReset = 0;

static unsigned int s_nScriptFrameLockEntry = 0;

static unsigned int s_nScriptEventSkipEntry = 0;

static unsigned int s_nScriptCfTime = 0;

static unsigned int s_nScriptEventFin = 0;

static unsigned int s_nScriptEventActive = 0;

static unsigned int s_nScriptTalkLock = 0;

extern const char D_004DA460[];
const char D_004DA460[8] = "light";

static const char sig_2[4] = "()V";

static const char sig_3[5] = "(I)V";

static const char sig_4[6] = "(II)V";

static int windowOwner = 0;

typedef void (*JSNativeMethod)(void);

extern void JS_init(int state, int class_capacity, int method_capacity);

extern int JS_loadClass(const char *class_name);

extern void JS_classSetup(int class_id, JSNativeMethod get_peer);

extern void JS_classAddMethod(int class_id, const char *name,
                              JSNativeMethod method);

extern void JS_classLight_getPeer(void);

extern void JS_classLight_setColor(void);

extern void JS_classLight_setDirection2(void);

/* Only the halfword at GameLoopState+0xC is evidenced by
 * SCRIPT_sendMovieSkipSignal; the rest of the game-state record remains
 * scaffold-owned. */

typedef struct GameLoopMovieStatePrefix {
    u8 unmodeled_00[0xC];
    unsigned short movie_state;
} GameLoopMovieStatePrefix;

static int currentScriptDB;

static int resourceID;

/* TU-local observer and CallMethod declarations. */

/* Proposal: the script table indexed by currentScriptDB. Only the two fields
 * this allocation touches are modeled; scriptDB's element stride (0x1C bytes,
 * evidenced by *7*4 in every CallMethod family function) is fixed by that
 * indexing arithmetic, not invented. */

typedef struct ScriptDbEntry {
    void *class_image;   /* +0x0: class database image read from the script file */
    void *motion_data;   /* +0x4: extra-motion pack read next to the script file (.fpk) */
    void *pdb;            /* +0x8: event data used for resource lookup */
    int active;          /* +0xC: nonzero when this script slot is bound (zero test only) */
    SceneThread *thread; /* +0x10: the script's VM thread; its own object is at +0x10 */
    int class_slot;      /* +0x14: class database slot the image is loaded into */
    int skip_count;      /* +0x18: class-name skip count JNI_searchClasses reported */
} ScriptDbEntry;

static ScriptDbEntry scriptDB[2];

typedef struct ScriptPdbFile {
    u8 unmodeled_00[8];
    int data;
} ScriptPdbFile;

extern ScriptPdbFile *PDB_findFile(void *pdb, const char *path);

/*
 * TU-local partial view of the engine's actor record (the 0xa70-strided
 * `actor` array at main 0x0043c1e0; other units keep their own scoped views
 * of the same record, e.g. src/main/near_dir.h). SCRIPT_execTalkto and
 * SCRIPT_execTouchto are handed one entry and read only the three members
 * below; every other byte of the record is untouched by this allocation and
 * stays an unmodeled span.
 *
 *   +0x4c0 script_object      the native actor peer passed to the talk
 *                             method as its first actor argument; the
 *                             talk and touch entry points check its
 *                             xeno/Chr class before queuing a task.
 *   +0x9f4 talk_method_name   the JNI method name of the actor's talk
 *                             script method, null when it has none. Both
 *                             entry points gate on it before queuing a talk
 *                             task, and SCRIPT_execTalkto passes it straight
 *                             through as the method to call. createTalkTask
 *                             (0x00261620, still assembler) passes its own
 *                             second argument to loadConstString(bytes,
 *                             length) (0x00261698), so the field is a name
 *                             string, not an integer flag.
 *   +0x9f8 touch_method_name  the method SCRIPT_execTouchto passes instead,
 *                             once the same +0x9f4 gate lets it through.
 */

typedef struct ScriptActorMethods {
    u8 unmodeled_00[0x4c0];
    SceneObject script_object;      /* +0x4c0 */
    u8 unmodeled_4c4[0x9f4 - 0x4c4];
    const char *talk_method_name;   /* +0x9f4 */
    const char *touch_method_name;  /* +0x9f8 */
} ScriptActorMethods;

/* Reused TU-local declaration (src/core/main-0025a9c0/private.h and every
 * other GameLoopState-family accepted source in this unit). */

extern ScriptGameLoopState GameLoopState;

/* Proposal: per-function JNI method-signature literals. Each is a duplicate
 * function-local `static const char sig[]`, so the assembler disambiguates
 * with the original ELF's own dup-static suffixing: "sig.2"/"sig.3"/"sig.4". */

/* "(II)V"  @ 0x004da478, ELF name "sig.4" */

/* PadData (0x00490d90, 0xd0 bytes) read as one doubleword at +0x28, the
 * shared half_28/half_2a pair (original: lui 0x49; ld 3512). */

#define PAD_DATA_DOUBLEWORD_AT_28 (PadData[0].packed_input)

/* Drop the task from the scheduler and leave the task body. The usual
 * do/while (0) statement-macro idiom; its loop notes are part of the
 * original's code generation. */

extern int loadScriptCD(ScriptDbEntry *entry, const char *path);

extern int loadScriptCD2(ScriptDbEntry *entry, const char *path);

extern void SCRIPT_load_DBG(const char *path);

static int PauseSkipCheck(void);

extern char *strcpy(char *destination, const char *source);

extern unsigned int strlen(const char *string);

extern ScriptRenderState sRender;

/* Queue a debug menu task for `callback` and give it the owner and an empty
 * window, the way every debug entry point does. */

extern int rcFile;

extern int lightFileNum;

extern ScriptFileSelect *fileSelect;

extern ScriptActorRecord actor[64];

extern void TWSYS_setGRPStatus(int group, int status);

extern ScriptWindow *TMENU_create(int component_id);

extern ScriptWindow *TSLIDER_create(int component_id);

extern void TMENU_addItem(ScriptWindow *menu, const char *text);

extern void initCursor(void);

extern int sprintf(char *buffer, const char *format, ...);

extern void DB_reset(int x, int y);

extern void DB_println(const char *format, ...);

extern void DB_lightWrite(int descriptor);

extern void RSRC_init(int rc);

extern char *RSRC_loadFile2(int rc, const char *directory, const char *name);

extern int *RSRC_getItem(int rc, char *handle);

extern void JS_exec(char *begin, char *end);

extern int xglCdFileSelect(ScriptFileSelect *state);

extern int strncmp(const char *a, const char *b, unsigned int length);

extern int MapGetNo(void);

extern char *MapGetName(int map_no);

extern void LoadMap2(int map_no, int entrance);

extern ScriptDbEntry *SCRIPT_exec(void);

static char *getStrIndex(char *string, unsigned int length, int ch);

extern void TWIN_dispose(TComponent *window);

extern void attrObserver(ScriptObserverTask *task);

extern int sceOpen(const char *path, int flags, ...);

extern int sceClose(int descriptor);

extern char *strrchr(const char *string, int ch);

static void initJS(int state)
{
    int class_id;

    JS_init(state, 0x40, 8);
    class_id = JS_loadClass(D_004DA460);
    JS_classSetup(class_id, JS_classLight_getPeer);
    JS_classAddMethod(class_id, D_004C1D50, JS_classLight_setColor);
    JS_classAddMethod(class_id, D_004C1D60, JS_classLight_setDirection2);
}

void SCRIPT_test(SceneObject argument, SceneMethod *method)
{
    JNI_initThread(stageVM);
    if (method != 0) {
        SceneObject arguments[1];

        arguments[0] = argument;
        JNI_callMethod(stageVM, method, arguments, 0);
    }
}

void SCRIPT_talkIgnoreSet(void)
{
    s_nScriptTalkLock = 1;
}

void SCRIPT_talkIgnoreClr(void)
{
    s_nScriptTalkLock = 0;
}

int SCRIPT_talkIgnoreGet(void)
{
    return s_nScriptTalkLock;
}

void SCRIPT_sceneChangeTimeInit(void)
{
    s_nScriptChangeTime = 0;
}

int SCRIPT_sceneChangeTimeGet(void)
{
    return s_nScriptChangeTime;
}

void SCRIPT_sceneChangeTimeSet(int value)
{
    s_nScriptChangeTime = value;
}

void SCRIPT_sceneChangeTimeDec(void)
{
    if (s_nScriptChangeTime != 0)
        s_nScriptChangeTime -= 1;
    else
        s_nScriptChangeTime = 0;
}

void SCRIPT_sendMovieSkipSignal(void)
{
    GameLoopMovieStatePrefix *game_state = (GameLoopMovieStatePrefix *)&GameLoopState;

    if (game_state->movie_state == 3)
        s_nScriptEventFin = 1;
}

void SCRIPT_clrEventActiveFlag(void)
{
    s_nScriptEventActive = 0;
}

void SCRIPT_setEventActiveFlag(void)
{
    s_nScriptEventActive = 1;
}

int SCRIPT_getEventActiveFlag(void)
{
    return s_nScriptEventActive;
}

void SCRIPT_eventFinish(void)
{
    s_nScriptEventFin = 1;
}

void SCRIPT_methodClearSet(void)
{
    s_nScriptSequenceReset = 1;
}

int SCRIPT_methodClearGet(void)
{
    return s_nScriptSequenceReset;
}

void SCRIPT_fade(int time)
{
    s_nScriptFadeRequest = 1;
    if (time < 0)
        time = 0;
    s_nScriptFadeOutTime = time;
}

int SCRIPT_getFadeTime(void)
{
    return s_nScriptFadeOutTime;
}

int SCRIPT_fadeGet(void)
{
    if (s_nScriptFadeRequest == 0)
        return 0;
    return s_nScriptFadeOutTime;
}

int SCRIPT_getCfTime(void)
{
    return (int)s_nScriptCfTime;
}

void SCRIPT_incCfTime(void)
{
    s_nScriptCfTime += 1u;
}

void SCRIPT_frameLock2Battle(void)
{
    s_nScriptFrameLockEntry = 1;
}

void SCRIPT_reset(void)
{
    SceneThread **event_vm;
    int remaining_vms;

    event_vm = evtVM;
    remaining_vms = 1;
    initVM();
    defaultVM = JNI_createThread(0, 8, 0x80);
    stageVM = JNI_createThread(0, 8, 0x46);
    do {
        --remaining_vms;
        *event_vm = JNI_createThread(0, 8, 0x40);
        ++event_vm;
    } while (remaining_vms >= 0);
    UseVMFlag = 0;
    JNI_pushFrame();
    JNI_pushFrame();
    JNI_loadNativeClass();
}


extern void JNI_loadClassDB(int slot, void *image);


INCLUDE_ASM("asm/main/nonmatchings/script", SCRIPT_init);

void SCRIPT_execTalkto(void *actor)
{
    ScriptActorMethods *methods = (ScriptActorMethods *)actor;
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (methods->talk_method_name != 0 && entry->active != 0 &&
        s_nScriptTalkLock == 0 &&
        JNI_isInstanceOf(methods->script_object, classJava_xeno_Chr) != 0) {
        do {
            if (!JNI_isInstanceOf(entry->thread->object, classJava_xeno_Stage))
                break;
            createTalkTask(actor, methods->talk_method_name);
        } while (0);
    }
}

void SCRIPT_execTouchto(void *actor)
{
    ScriptActorMethods *methods = (ScriptActorMethods *)actor;
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (methods->talk_method_name != 0 && entry->active != 0 &&
        s_nScriptTalkLock == 0 &&
        JNI_isInstanceOf(methods->script_object, classJava_xeno_Chr) != 0) {
        do {
            if (!JNI_isInstanceOf(entry->thread->object, classJava_xeno_Stage))
                break;
            createTalkTask(actor, methods->touch_method_name);
        } while (0);
    }
}

void createTalkTask(void *actor, const char *method_name)
{
    SceneObject receiver;
    SceneClass *scene_class;
    SceneMethod *method;
    ScriptObserverTask *task;
    XglTaskScheduler *scheduler;

    if (GameLoopState.flags & 0x20)
        return;
    receiver = scriptDB[currentScriptDB].thread->object;
    scene_class = ((SceneObjectHeader *)receiver)->class_ref->scene_class;
    method = findMethod(scene_class, loadConstString(method_name, -1),
                        loadConstString(sig0_0, -1));
    if (method != 0) {
        scheduler = GameLoopState.task_scheduler;
        task = (ScriptObserverTask *)xglTaskEntryNext(
            scheduler, (int (*)(XglTaskPrefix *))talktoObserver,
            scheduler != 0 ? scheduler->active_tail : 0);
        if (task != 0) {
            task->state_flags = 0;
            task->owner = &GameLoopState;
            task->object = 0;
        }
        task->argument2 = 0;
        task->method_signature = sig0_0;
    } else {
        TComponent *window;

        method = findMethod(scene_class, loadConstString(method_name, -1),
                            loadConstString(sig1_1, -1));
        if (method == 0)
            return;
        scheduler = GameLoopState.task_scheduler;
        task = (ScriptObserverTask *)xglTaskEntryNext(
            scheduler, (int (*)(XglTaskPrefix *))talktoObserver,
            scheduler != 0 ? scheduler->active_tail : 0);
        if (task != 0) {
            task->state_flags = 0;
            task->owner = &GameLoopState;
            task->object = 0;
        }
        task->method_signature = sig1_1;
        window = (TComponent *)TWIN_create2(-1);
        window->class_ref = classJava_xeno_util_Window->instance_class_ref;
        TWIN_initCF(window);
        window->flags &= ~0x40;
        window->flags &= ~0x80;
        task->argument2 = (int)window;
    }
    task->receiver = receiver;
    task->method_name = method_name;
    task->method = method;
    task->object = actor;
    Before_Talk(actor);
    GameLoopState.flags |= 0x9000;
    XTK_setWindowOwner((int)actor);
    JNI_initThread(stageVM);
    talktoObserver(task);
}

void talktoObserver(ScriptObserverTask *task)
{
    unsigned int owner_flags = task->owner->flags;
    SceneObject receiver;
    SceneObject object;
    SceneMethod *method;
    int script_flags;
    SceneObject arguments[3];

    if (owner_flags & 0x800) {
        xglTaskRemove(&task->task);
        return;
    }
    if (owner_flags & 0x20) {
        xglTaskRemove(&task->task);
        return;
    }

    receiver = task->receiver;
    {
        SceneObjectHeader *header = (SceneObjectHeader *)receiver;
        SceneClass *scene_class = header->class_ref->scene_class;
        SceneString *name = loadConstString(task->method_name, -1);
        SceneString *signature = loadConstString(task->method_signature, -1);

        task->method = findMethod(scene_class, name, signature);
    }
    if (task->method == 0) {
        xglTaskRemove(&task->task);
        return;
    }

    object = task->object;
    script_flags = getScriptFlag(object);
    if (task->argument2 != 0 && (script_flags & 8)) {
        if (UseVMFlag != 0 || (PAD_DATA_DOUBLEWORD_AT_28 & 0x600000) == 0x400000) {
            XTK_setWindowOwner(0);
            talkCancel(task);
            return;
        }
    }

    /*
     * The method is read back from the task before the argument array is
     * filled: the original loads it (lw a1,60(s2)) ahead of the stageVM
     * argument, which only happens when the array stores separate the read
     * from the call (a read in the call itself, attempt-4d6f277fff4a form
     * 08, is scheduled after the stageVM load).
     */
    method = task->method;
    arguments[0] = receiver;
    arguments[1] = ((ScriptActorMethods *)object)->script_object;
    if (task->argument2 != 0)
        arguments[2] = (SceneObject)task->argument2;
    JNI_callMethod(stageVM, method, arguments, 0);
    if (stageVM->flags & SCENE_THREAD_CALL_PENDING) {
        XTK_setWindowOwner(0);
        actTalkAfter(object);
        GameLoopState.flags &= ~0x1000;
        GameLoopState.flags &= ~0x8000;
        GameLoopState.flags &= ~0x10000;
        xglTaskRemove(&task->task);
    }
}

void talkCancel(ScriptObserverTask *task)
{
    TComponent *window;
    ScriptGameLoopState *game_state;

    actTalkAfter(task->object);
    window = (TComponent *)task->argument2;
    game_state = &GameLoopState;
    window->flags |= 0x80;
    window->flags &= ~2;
    game_state->flags &= ~0x1000;
    game_state->flags &= ~0x8000;
    game_state->flags &= ~0x10000;
    if (window != 0)
        window->state = 2;
    TWIN_dispose(window);
    xglTaskRemove(&task->task);
}

void SCRIPT_execEntered(int attribute)
{
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (entry->active != 0) {
        SceneObject object = entry->thread->object;

        if (!(GameLoopState.flags & 0x400) &&
            JNI_isInstanceOf(object, classJava_xeno_Stage)) {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            ScriptObserverTask *task = (ScriptObserverTask *)xglTaskEntryNext(
                scheduler, (int (*)(XglTaskPrefix *))attrObserver,
                scheduler != 0 ? scheduler->active_tail : 0);

            if (task != 0) {
                task->owner = &GameLoopState;
                task->state_flags = 0;
                task->object = 0;
            }
            task->object = object;
            task->argument1 = attribute;
            GameLoopState.flags |= 0x100000;
        }
    }
}

void CallMethod(const char *method_name)
{
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (entry->active != 0) {
        SceneObject object = entry->thread->object;

        if (JNI_isInstanceOf(object, classJava_xeno_Stage) != 0) {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            ScriptObserverTask *task = (ScriptObserverTask *)xglTaskEntryNext(
                scheduler, (int (*)(XglTaskPrefix *))funcObserver,
                scheduler != 0 ? scheduler->active_tail : 0);
            unsigned char started;

            if (task != 0) {
                task->owner = &GameLoopState;
                task->state_flags = 0;
                task->object = 0;
            }
            task->object = object;
            task->method_name = method_name;
            task->method_signature = call_method_signature_void;
            task->invocation_state = 0;
            funcObserver(task);
            /* separate load keeps the funcObserver call out of tail position:
             * without it (form 11) GCC emits a sibling `j funcObserver` */
            started = task->invocation_state;
        }
    }
}

void CallMethod_I(const char *method_name, int argument1)
{
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (entry->active != 0) {
        SceneObject object = entry->thread->object;

        if (JNI_isInstanceOf(object, classJava_xeno_Stage) != 0) {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            ScriptObserverTask *task = (ScriptObserverTask *)xglTaskEntryNext(
                scheduler, (int (*)(XglTaskPrefix *))funcObserver,
                scheduler != 0 ? scheduler->active_tail : 0);
            unsigned char started;

            if (task != 0) {
                task->owner = &GameLoopState;
                task->state_flags = 0;
                task->object = 0;
            }
            task->object = object;
            task->method_name = method_name;
            task->method_signature = call_method_signature_int;
            task->argument1 = argument1;
            task->invocation_state = 0;
            funcObserver(task);
            /* separate load keeps the funcObserver call out of tail position:
             * without it (form 11) GCC emits a sibling `j funcObserver` */
            started = task->invocation_state;
        }
    }
}

void CallMethod_II(const char *method_name, int argument1, int argument2)
{
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (entry->active != 0) {
        SceneObject object = entry->thread->object;

        if (JNI_isInstanceOf(object, classJava_xeno_Stage) != 0) {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            ScriptObserverTask *task = (ScriptObserverTask *)xglTaskEntryNext(
                scheduler, (int (*)(XglTaskPrefix *))funcObserver,
                scheduler != 0 ? scheduler->active_tail : 0);
            unsigned char started;

            if (task != 0) {
                task->owner = &GameLoopState;
                task->state_flags = 0;
                task->object = 0;
            }
            task->object = object;
            task->method_name = method_name;
            task->method_signature = call_method_signature_int_int;
            task->argument1 = argument1;
            task->argument2 = argument2;
            task->invocation_state = 0;
            funcObserver(task);
            /* separate load keeps the funcObserver call out of tail position:
             * without it (form 11) GCC emits a sibling `j funcObserver` */
            started = task->invocation_state;
        }
    }
}

void funcObserver(ScriptObserverTask *task)
{
    unsigned int owner_flags;
    ScriptDbEntry *entry;
    signed char vm_slot;
    SceneObject object;

    xglFontDebugPrintf(8, 24, func_observer_debug_text);

    owner_flags = task->owner->flags;
    if (owner_flags & 0x800)
        do
        {
            xglTaskRemove(&(task)->task);
            return;
        }
        while (0);
    if ((task->state_flags & 1) == 0) {
        if (owner_flags & 0x400)
            do
            {
                xglTaskRemove(&(task)->task);
                return;
            }
            while (0);
        task->state_flags |= 1;
        if (getEmptyVM(task) == 0)
            do
            {
                xglTaskRemove(&(task)->task);
                return;
            }
            while (0);
    }

    entry = &scriptDB[currentScriptDB];
    vm_slot = task->vm_slot;
    if (entry->active == 0)
        return;

    /* The Stage object changed since CallMethod queued the task. */
    object = entry->thread->object;
    if (object != task->object)
        do
        {
            xglTaskRemove(&(task)->task);
            return;
        }
        while (0);

    {
        SceneObjectHeader *header = (SceneObjectHeader *)task->object;
        SceneClass *scene_class = header->class_ref->scene_class;
        SceneString *name = loadConstString(task->method_name, -1);
        SceneString *signature = loadConstString(task->method_signature, -1);
        SceneMethod *method = findMethod(scene_class, name, signature);
        SceneObject arguments[3];

        task->method = method;
        if (method == 0) {
            UseVMFlag &= ~(1 << vm_slot);
            xglTaskRemove(&task->task);
            return;
        }

        arguments[0] = object;
        arguments[1] = (SceneObject)task->argument1;
        arguments[2] = (SceneObject)task->argument2;
        JNI_callMethod(evtVM[vm_slot], method, arguments, 0);
        if (evtVM[vm_slot]->flags & SCENE_THREAD_CALL_PENDING) {
            JNI_catchException();
            UseVMFlag &= ~(1 << vm_slot);
            xglTaskRemove(&task->task);
            return;
        }
    }
    GameLoopState.flags |= 0x200000;
}

int getEmptyVM(ScriptObserverTask *observer)
{
    int slot;

    observer->vm_slot = -1;
    for (slot = 0; slot < 2; ++slot) {
        if (((UseVMFlag >> slot) & 1) == 0) {
            JNI_initThread(evtVM[slot]);
            observer->state_flags |= 1;
            UseVMFlag |= 1 << slot;
            observer->vm_slot = (signed char)slot;
            return 1;
        }
    }
    return observer->vm_slot != -1;
}

int SCRIPT_load(const char *path)
{
    return loadScriptCD(&scriptDB[(currentScriptDB + 1) & 1], path);
}

int SCRIPT_load2(const char *path)
{
    return loadScriptCD2(&scriptDB[(currentScriptDB + 1) & 1], path);
}




INCLUDE_ASM("asm/main/nonmatchings/script", SCRIPT_load_DBG);

INCLUDE_ASM("asm/main/nonmatchings/script", getStrIndex_00262160);

static char *replacePathExt(char *path, const char *ext)
{
    return strcpy(getStrIndex(path, strlen(path), '.'), ext);
}

void attrObserver(ScriptObserverTask *task)
{
    SceneObject object = task->object;
    SceneMethod *method;
    int vm_slot;
    ScriptMethodArgument arguments[2];

    if ((task->state_flags & 1) == 0) {
        if (task->owner->flags & 0x400) {
            xglTaskRemove(&task->task);
            return;
        }
        task->owner->flags |= 0x400;
        if (getEmptyVM(task) == 0) {
            xglTaskRemove(&task->task);
            return;
        }
        task->state_flags |= 1;
    }
    {
        SceneObjectHeader *header = (SceneObjectHeader *)object;
        SceneClass *scene_class = header->class_ref->scene_class;
        SceneString *name = loadConstString("entered", -1);
        SceneString *signature = loadConstString("(I)V", -1);

        method = findMethod(scene_class, name, signature);
    }
    if (method == 0) {
        task->owner->flags &= ~0x400;
        xglTaskRemove(&task->task);
        return;
    }
    arguments[0].reference = object;
    vm_slot = task->vm_slot;
    arguments[1].integer = task->argument1;
    JNI_callMethod(evtVM[vm_slot], method, (SceneObject *)arguments, 0);
    if (evtVM[vm_slot]->flags & SCENE_THREAD_CALL_PENDING) {
        JNI_catchException();
        task->owner->flags &= ~0x400;
        UseVMFlag &= ~(1 << vm_slot);
        xglTaskRemove(&task->task);
        GameLoopState.flags &= ~0x100000;
    }
}








INCLUDE_ASM("asm/main/nonmatchings/script", PauseSkipCheck);

#define SCRIPT_FADE_COLOR(red, green, blue)                                    \
    (0x7F000000 + ((int)((red) * 127.0f) << 16) +                              \
     ((int)((green) * 127.0f) << 8) + (int)((blue) * 127.0f))

void sceneObserver2(ScriptSceneTask *task)
{
    SceneThread *thread;
    StudioCamera *camera;
    int camera_index;

    if (!(task->state_flags & 1)) {
        task->state_flags |= 1;
        task->fade_started = 0;
        task->elapsed_frames = 0;
    }
    thread = (SceneThread *)task->thread;
    if (PauseSkipCheck() != 0 || s_nScriptEventFin != 0) {
        thread->flags = (thread->flags & ~0x10) | SCENE_THREAD_CALL_PENDING;
        s_nScriptEventFin = 0;
    }
    if (!(thread->flags & SCENE_THREAD_CALL_PENDING))
        return;
    if (task->fade_started == 0) {
        task->fade_started = 1;
        if (s_nScriptFrameLockEntry == 0) {
            if ((int)GameLoopState.fade_out_frames > 0) {
                nmlModelSetFadeOut((int)GameLoopState.fade_out_frames, 0);
                FX_ScreenMask(0,
                              SCRIPT_FADE_COLOR(GameLoopState.fade_out_red,
                                                GameLoopState.fade_out_green,
                                                GameLoopState.fade_out_blue),
                              (int)GameLoopState.fade_out_frames, 2);
            }
        } else {
            GameLoopState.frame_lock_wait = s_nScriptFadeOutTime + 4;
        }
        if (s_nScriptEventSkipEntry != 0 && s_nScriptFrameLockEntry == 0)
            nmlModelSetBackBufferToBattle(sRender.owner_index);
        s_nScriptChangeTime = (int)GameLoopState.fade_out_frames + 1;
    }
    task->elapsed_frames++;
    if ((int)s_nScriptFadeOutTime >= task->elapsed_frames)
        return;
    if (s_nScriptFrameLockEntry != 0) {
        nmlModelSetBackBufferToBattle(sRender.owner_index);
        nmlModelSetFadeInCancel(30);
        nmlModelSetFadeOutCancel(30);
    }
    if (s_nScriptEventSkipEntry != 0) {
        nmlModelSendSignalEventFinish();
        s_nScriptEventSkipEntry = 0;
    }
    s_nScriptFadeOutTime = 30;
    s_nScriptChangeTime = 25;
    camera_index = 0;
    s_nScriptEventActive = 0;
    s_nScriptFrameLockEntry = 0;
    s_nScriptFadeRequest = 0;
    s_nScriptTalkLock = 0;
    MAP_initUnit();
    ACT_initScene();
    ACT_initMTNResource();
    TWSYS_init();
    MSG_init();
    s_nScriptSequenceReset = 0;
    xglRenderClearColor(0x80000000);
    GameLoopState.kind = 2;
    s_nScriptCfTime = 0;
    nmlModelSetGlobalPointLightReset();
    SCENE_cleanup(stageVM, thread->object);
    JNI_catchException();
    task->owner->flags &= ~0x800;
    do {
        xglStudioGetCamera(&camera, camera_index);
        camera_index++;
        xglCameraClipRangeDefault(camera);
    } while (camera_index < 8);
    nmlModelSetGlobalPointLightReset();
    nmlModelSetEffectWrite(1);
    nmlModelSetFadeIn((int)GameLoopState.fade_frames, 0);
    FX_ScreenMask(0,
                  SCRIPT_FADE_COLOR(GameLoopState.fade_red,
                                    GameLoopState.fade_green,
                                    GameLoopState.fade_blue),
                  (int)GameLoopState.fade_frames, 3);
    xglTaskRemove(&task->task);
}




extern int MapGetNo(void);

INCLUDE_ASM("asm/main/nonmatchings/script", sceneObserver);







extern int vmDebugMask;

extern const char *vmSwitchStr[];

extern void TMENU_setItem(ScriptWindow *menu, int index, const char *text);

int DB_lightSave(ScriptFileSelect *state);

void DB_evtMonitor(ScriptMenuTask *task);

INCLUDE_ASM("asm/main/nonmatchings/script", DB_light);

static void DB_vmSwitch(ScriptMenuTask *task)
{
    ScriptWindow *menu = task->window;
    int item;
    int on;
    int i;
    int bit;
    int item_bit;

    if (!(task->state_flags & 1)) {
        menu = TMENU_create(-1);
        menu->options = 0;
        menu->mode |= 0x10;
        for (i = 0, bit = 1; i < 8 && vmSwitchStr[i * 2] != 0; i++, bit = 1 << (i & 0x1f)) {
            on = (vmDebugMask & bit) != 0;
            TMENU_setItem(menu, -1, vmSwitchStr[i * 2 + on]);
        }
        task->window = menu;
        task->state_flags |= 1;
    }
    if (menu->state != 0)
        return;
    item = menu->selected_item;
    if (menu->cancel_item < 0) {
        menu->state = 2;
        task->owner->flags &= ~1;
        task->owner->kind = task->owner->saved_kind;
        TWSYS_setGRPStatus(0, 0);
        xglTaskRemove(&task->task);
        return;
    }
    item_bit = 1 << (item & 0x1f);
    if (vmDebugMask & item_bit) {
        vmDebugMask &= ~item_bit;
        on = 0;
    } else {
        vmDebugMask |= item_bit;
        on = 1;
    }
    TMENU_setItem(menu, item, vmSwitchStr[item * 2 + on]);
    menu->state = 0xe;
    if (item == 4) {
        ScriptMenuTask *monitor;

        do {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            monitor = (ScriptMenuTask *)xglTaskEntryNext(
                scheduler, (int (*)(XglTaskPrefix *))DB_evtMonitor,
                scheduler != 0 ? scheduler->active_tail : 0);
            if (monitor != 0) {
                monitor->state_flags = 0;
                monitor->window = 0;
                monitor->owner = &GameLoopState;
            }
        } while (0);
    }
}

static int DB_stageSelect(ScriptMenuTask *task)
{
    ScriptWindow *slider;
    const char *map_name;
    int value;

    if (!(task->state_flags & 1)) {
        task->state_flags |= 1;
        slider = TSLIDER_create(-1);
        slider->max_value = 2000;
        slider->mode |= 0x10;
        slider->x = 256.0f;
        slider->y = 224.0f;
        slider->value = MapGetNo();
        task->window = slider;
        task->phase = 0;
        task->loaded_map = 0;
    }
    slider = task->window;
    switch (task->phase) {
    case 0:
        if (slider->value >= 0) {
            map_name = MapGetName(slider->value);
            if (map_name != 0)
                xglFontDebugPrintf(8, 32, map_name);
        }
        if (slider->state == 2) {
            if (slider->value >= 0) {
                task->phase = 1;
                task->map_no = slider->value;
                slider = TSLIDER_create(-1);
                task->window = slider;
                slider->x = 256.0f;
                slider->y = 224.0f;
                slider->mode |= 0x10;
                break;
            }
            task->owner->flags &= ~1;
            task->owner->kind = task->owner->saved_kind;
            TWSYS_setGRPStatus(0, 0);
            return xglTaskRemove(&task->task);
        }
        break;
    case 1:
        xglFontDebugPrintf(8, 32, "Entrance:");
        if (slider->state == 2) {
            value = slider->value;
            if (value >= 0) {
                task->loaded_map = value;
                LoadMap2(task->map_no, value);
            }
            task->owner->flags &= ~1;
            task->owner->kind = task->owner->saved_kind;
            TWSYS_setGRPStatus(0, 0);
            return xglTaskRemove(&task->task);
        }
    }
}






INCLUDE_ASM("asm/main/nonmatchings/script", DB_memory);

int DB_lightSave(ScriptFileSelect *state)
{
    char path[512];
    int result;
    char *handle;
    int file;
    ScriptPadButtons *pad = (ScriptPadButtons *)PadData;

    result = xglCdFileSelect(state);
    switch (result) {
    case 0:
        break;
    case 1:
        RSRC_init(rcFile);
        handle = RSRC_loadFile2(rcFile, state->directory, state->file_path);
        JS_exec(handle, handle + *RSRC_getItem(rcFile, handle));
        return 0;
    case 2:
        return 1;
    }
    if (pad->repeat & 0x8000)
        lightFileNum--;
    if (pad->repeat & 0x2000)
        lightFileNum++;
    if (lightFileNum < 0)
        lightFileNum = 99;
    if (lightFileNum >= 100)
        lightFileNum = 0;
    DB_reset(32, 160);
    /* JP: ●ボタン：ロード
     * EN: Filled circle button: Load */
    DB_println("\xa1\xfc\xa5\xdc\xa5\xbf\xa5\xf3\xa1\xa7\xa5\xed\xa1\xbc\xa5\xc9");
    /* JP: ■ボタン：セーブ
     * EN: Filled square button: Save */
    DB_println("\xa2\xa3\xa5\xdc\xa5\xbf\xa5\xf3\xa1\xa7\xa5\xbb\xa1\xbc\xa5\xd6");
    DB_println("lgt%02d.rc", lightFileNum);
    if (pad->pressed & 0x80) {
        sprintf(path, "%s%slgt%02d.rc", state->directory, state->file_path, lightFileNum);
        file = sceOpen(path, 0x602);
        if (file >= 0) {
            DB_lightWrite(file);
            sceClose(file);
        }
        return 1;
    }
    return 0;
}

static int DB_scriptFileSelect(ScriptMenuTask *task)
{
    ScriptFileSelect *browser;
    char *extension;
    char *handle;

    if (!(task->state_flags & 1)) {
        fileSelect->selection = 0;
        task->window = fileSelect;
        task->state_flags |= 1;
    }
    browser = task->window;
    switch (xglCdFileSelect(browser)) {
    case 0:
        break;
    case 1:
        extension = strrchr(browser->file_path, '.');
        if (extension != 0) {
            if (strncmp(extension, ".evt", 4) == 0) {
                SCRIPT_load_DBG(browser->file_path);
                SCRIPT_exec();
            } else if (strncmp(extension, ".rc", 3) == 0) {
                RSRC_init(rcFile);
                handle = RSRC_loadFile2(rcFile, browser->directory, browser->file_path);
                JS_exec(handle, handle + *RSRC_getItem(rcFile, handle));
                task->owner->flags &= ~1;
                task->owner->kind = task->owner->saved_kind;
            }
        }
        task->owner->flags &= ~1;
        TWSYS_setGRPStatus(0, 0);
        return xglTaskRemove(&task->task);
    case 2:
        task->owner->flags &= ~1;
        task->owner->kind = task->owner->saved_kind;
        TWSYS_setGRPStatus(0, 0);
        return xglTaskRemove(&task->task);
    }
}

extern ScriptWindParams dbgWind;

extern void updateWind(ScriptWindParams *params);

extern void EXM_GetWindPara(ScriptWindParams *params);

extern void EXM_SetWindStruct(ScriptWindParams *params);

extern void EXM_ClearWindStruct(void);

extern void EXM_SetWindPara(ScriptWindParams *params);

static int DB_wind(ScriptMenuTask *task)
{
    ScriptPadButtons *pad = (ScriptPadButtons *)&PadData[1];

    if (!(task->state_flags & 1)) {
        task->state_flags |= 1;
        EXM_GetWindPara(&dbgWind);
        EXM_SetWindStruct(&dbgWind);
        GameLoopState.flags &= ~1;
        GameLoopState.debug_menu_storage = 0;
        GameLoopState.kind = GameLoopState.saved_kind;
    }
    updateWind(&dbgWind);
    if (pad->pressed & 0x40) {
        EXM_ClearWindStruct();
        EXM_SetWindPara(&dbgWind);
        task->owner->flags &= ~1;
        task->owner->kind = task->owner->saved_kind;
        TWSYS_setGRPStatus(0, 0);
        return xglTaskRemove(&task->task);
    }
}

void SCRIPT_debugMenu(ScriptMenuTask *task)
{
    ScriptWindow *menu = task->window;
    ScriptMenuTask *created;

    if (!(task->state_flags & 1)) {
        TWSYS_setGRPStatus(0, 3);
        menu = TMENU_create(-1);
        menu->options = 0;
        menu->mode |= 0x10;
        TMENU_addItem(menu, "ScriptExec\nBgChange\nVirtualM\nTool\nLight\nMemoryInfo\nWind");
        menu->flags &= ~0x40;
        task->window = menu;
        task->state_flags |= 1;
    }
    if (menu->state != 0)
        return;
    menu->state = 2;
    switch (menu->selected_item) {
    case 0:
        do
        {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            (created) = (ScriptMenuTask *)xglTaskEntryNext( scheduler, (int (*)(XglTaskPrefix *))(DB_scriptFileSelect), scheduler != 0 ? scheduler->active_tail : 0);
            if ((created) != 0)
            {
                (created)->state_flags = 0;
                (created)->window = 0;
                (created)->owner = &GameLoopState;
            }
            
        }
        while (0);
        break;
    case 1:
        do
        {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            (created) = (ScriptMenuTask *)xglTaskEntryNext( scheduler, (int (*)(XglTaskPrefix *))(DB_stageSelect), scheduler != 0 ? scheduler->active_tail : 0);
            if ((created) != 0)
            {
                (created)->state_flags = 0;
                (created)->window = 0;
                (created)->owner = &GameLoopState;
            }
            
        }
        while (0);
        break;
    case 2:
        do
        {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            (created) = (ScriptMenuTask *)xglTaskEntryNext( scheduler, (int (*)(XglTaskPrefix *))(DB_vmSwitch), scheduler != 0 ? scheduler->active_tail : 0);
            if ((created) != 0)
            {
                (created)->state_flags = 0;
                (created)->window = 0;
                (created)->owner = &GameLoopState;
            }
            
        }
        while (0);
        break;
    case 3:
        initCursor();
        task->owner->kind = 7;
        break;
    case 4:
        do
        {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            (created) = (ScriptMenuTask *)xglTaskEntryNext( scheduler, (int (*)(XglTaskPrefix *))(DB_light), scheduler != 0 ? scheduler->active_tail : 0);
            if ((created) != 0)
            {
                (created)->state_flags = 0;
                (created)->window = 0;
                (created)->owner = &GameLoopState;
            }
            
        }
        while (0);
        break;
    case 5:
        do
        {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            (created) = (ScriptMenuTask *)xglTaskEntryNext( scheduler, (int (*)(XglTaskPrefix *))(DB_memory), scheduler != 0 ? scheduler->active_tail : 0);
            if ((created) != 0)
            {
                (created)->state_flags = 0;
                (created)->window = 0;
                (created)->owner = &GameLoopState;
            }
            
        }
        while (0);
        created->owner->saved_kind = task->owner->saved_kind;
        break;
    case 6:
        do
        {
            XglTaskScheduler *scheduler = GameLoopState.task_scheduler;
            (created) = (ScriptMenuTask *)xglTaskEntryNext( scheduler, (int (*)(XglTaskPrefix *))(DB_wind), scheduler != 0 ? scheduler->active_tail : 0);
            if ((created) != 0)
            {
                (created)->state_flags = 0;
                (created)->window = 0;
                (created)->owner = &GameLoopState;
            }
            
        }
        while (0);
        break;
    default:
        if (menu->cancel_item < 0) {
            task->owner->flags &= ~1;
            task->owner->kind = task->owner->saved_kind;
            TWSYS_setGRPStatus(0, 0);
        }
        break;
    }
    xglTaskRemove(&task->task);
}







INCLUDE_ASM("asm/main/nonmatchings/script", loadScriptCD);

INCLUDE_ASM("asm/main/nonmatchings/script", loadScriptCD2);

extern int sceOpen(const char *path, int flags, ...);


extern int sceClose(int descriptor);


INCLUDE_ASM("asm/main/nonmatchings/script", loadScript);

INCLUDE_ASM("asm/main/nonmatchings/script", DB_evtMonitor);












INCLUDE_ASM("asm/main/nonmatchings/script", SCRIPT_exec);


extern void *PLAY_getCurrent(void);

extern void PLAY_setupDefault(void *play);

extern void TCAMERA_init(void);

extern void *newObject(SceneClass *scene_class);

extern SceneClass *loadClass(SceneString *name, int initialize);

extern void STAGE_instance(SceneVm *vm, SceneObject stage_object);

extern void SCENE_instance(SceneVm *vm, SceneObject scene_object);

extern void SCENE_start(SceneVm *vm, SceneObject scene_object, int mode,
                        const char *method_name);

ScriptDbEntry *SCRIPT_exec2(void)
{
    ScriptDbEntry *entry;
    SceneObject object;
    SceneThread *thread;
    ScriptSceneTask *task;
    XglTaskScheduler *scheduler;
    StudioCamera *camera;
    StudioCamera *target;
    int camera_index;
    SceneThread *vm;
    int (*callback)(XglTaskPrefix *);

    XTK_setWindowOwner(0);
    currentScriptDB ^= 1;
    entry = &scriptDB[(currentScriptDB + 1) & 1];
    JNI_loadClassDB(entry->class_slot, 0);
    entry = &scriptDB[currentScriptDB];
    PLAY_setupDefault(PLAY_getCurrent());
    if (entry->active == 0) {
        s_nScriptChangeTime = 0x19;
        JNI_popFrame();
        XTK_setResourceID(1);
        GameLoopState.kind = 2;
        nmlModelSetGlobalPointLightReset();
        s_nScriptCfTime = 0;
        s_nScriptTalkLock = 0;
        TCAMERA_init();
        TWSYS_init();
        MSG_init();
        entry->thread = 0;
        JNI_loadClassDB(entry->class_slot, 0);
        JNI_pushFrame();
        JNI_loadNativeClass();
        return entry;
    }
    if (GameLoopState.flags & 0x800) {
        s_nScriptTalkLock = 0;
        TCAMERA_init();
        TWSYS_init();
        MSG_init();
    }
    if (entry->skip_count != 0) {
        if (entry->skip_count == 1) {
            camera_index = 0;
            JNI_popFrame();
            XTK_setResourceID(1);
            GameLoopState.kind = 2;
            nmlModelSetGlobalPointLightReset();
            s_nScriptCfTime = 0;
            do {
                xglStudioGetCamera(&camera, camera_index);
                target = camera;
                if (camera_index == 0) {
                    target->state = 4;
                    target->active = 1;
                } else {
                    target->active = 0;
                }
                camera_index++;
            } while (camera_index < 8);
            s_nScriptTalkLock = 0;
            TCAMERA_init();
            TWSYS_init();
            MSG_init();
            s_nScriptChangeTime = 0x19;
            JNI_pushFrame();
            JNI_loadNativeClass();
            JNI_loadClassDB(entry->class_slot, entry->class_image);
            object = newObject(
                loadClass(loadConstString((const char *)entry->active, -1), 0));
            thread = JNI_createThread(2, 6, 0x46);
            entry->thread = thread;
            thread->object = object;
            STAGE_instance(defaultVM, object);
        }
        return entry;
    }
    XTK_setResourceID(2);
    GameLoopState.kind = 3;
    GameLoopState.flags |= 0x800;
    s_nScriptEventActive = 1;
    nmlModelSetGlobalPointLightReset();
    nmlModelSetFadeIn((int)GameLoopState.fade_frames, 0);
    FX_ScreenMask(0,
                  SCRIPT_FADE_COLOR(GameLoopState.fade_red,
                                    GameLoopState.fade_green,
                                    GameLoopState.fade_blue),
                  (int)GameLoopState.fade_frames, 3);
    s_nScriptChangeTime = 0x19;
    s_nScriptTalkLock = 0;
    TCAMERA_init();
    TWSYS_init();
    MSG_init();
    JNI_popFrame();
    JNI_pushFrame();
    JNI_loadNativeClass();
    JNI_loadClassDB(entry->class_slot, entry->class_image);
    object = newObject(
        loadClass(loadConstString((const char *)entry->active, -1), 0));
    thread = JNI_createThread(2, 6, 0x46);
    callback = (int (*)(XglTaskPrefix *))sceneObserver2;
    scheduler = GameLoopState.task_scheduler;
    thread->object = object;
    entry->thread = thread;
    task = (ScriptSceneTask *)xglTaskEntryNext(
        scheduler, callback,
        scheduler != 0 ? scheduler->active_tail : 0);
    if (task != 0) {
        task->state_flags = 0;
        task->owner = &GameLoopState;
        task->thread = 0;
    }
    vm = defaultVM;
    task->thread = (SceneObject)thread;
    SCENE_instance(vm, object);
    SCENE_start(defaultVM, object, 8, "play");
    return entry;
}

void XTK_setResourceID(int id)
{
    resourceID = id;
}

int XTK_getResourceID(void)
{
    return resourceID;
}

void XTK_setWindowOwner(int owner)
{
    windowOwner = owner;
}

int XTK_getWindowOwner(void)
{
    return windowOwner;
}

int XTK_findFile(const char *path)
{
    ScriptPdbFile *file;

    file = PDB_findFile(scriptDB[currentScriptDB].pdb, path);
    if (file != 0)
        return file->data;
    return 0;
}
