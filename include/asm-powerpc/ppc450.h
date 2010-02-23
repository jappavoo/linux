#ifndef __POWERPC_PPC450__
#define __POWERPC_PPC450__

#ifndef __ASSEMBLY__
asm(".macro lfpdx   frt, idx, reg; .long ((31<<26)|((\\frt)<<21)|(\\idx<<16)|(\\reg<<11)|(462<<1)); .endm");
asm(".macro lfpdux  frt, idx, reg; .long ((31<<26)|((\\frt)<<21)|(\\idx<<16)|(\\reg<<11)|(494<<1)); .endm");
asm(".macro stfpdx  frt, idx, reg; .long ((31<<26)|((\\frt)<<21)|(\\idx<<16)|(\\reg<<11)|(974<<1)); .endm");
asm(".macro stfpdux frt, idx, reg; .long ((31<<26)|((\\frt)<<21)|(\\idx<<16)|(\\reg<<11)|(1006<<1)); .endm");
#endif

#endif /* __POWERPC_PPC450__ */
