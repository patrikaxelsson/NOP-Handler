#ifdef __DEBUG__
	#ifdef __AROS__
	#include <aros/debug.h>
	#else
	#include <clib/debug_protos.h>
	#endif
#else
#define kprintf(...)
#endif

const char *ActionToName(LONG action);
