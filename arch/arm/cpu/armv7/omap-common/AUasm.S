
#include <asm-offsets.h>
#include <config.h>
#include <linux/linkage.h>

// U32 GetCycleCounter(void)
ENTRY(GetCycleCounter)

    mrc     p15, 0, r0, c9, c13, 0  // Read CP15 Cycle Counter
    bx      lr
ENDPROC(GetCycleCounter)
