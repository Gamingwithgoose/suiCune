#include "../constants.h"
#include "queue_script.h"

Script_fn_t gQueuedScriptAddr;
void (*gQueuedFuncAddr)(void);

//  Push pointer hl to gQueuedScriptAddr.
void QueueScript(Script_fn_t hl){
    // LD_addr_A(wQueuedScriptBank);
    // LD_A_L;
    // LD_addr_A(wQueuedScriptAddr);
    // LD_A_H;
    // LD_addr_A(wQueuedScriptAddr + 1);
    gQueuedScriptAddr = hl;
    // RET;
}

//  Push pointer hl to gQueuedScriptFunc.
void QueueFunc(void (*hl)(void)) {
    gQueuedFuncAddr = hl;
}
