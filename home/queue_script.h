void QueueScript(Script_fn_t hl);
void QueueFunc(void (*hl)(void));

extern Script_fn_t gQueuedScriptAddr;
extern void (*gQueuedFuncAddr)(void);
