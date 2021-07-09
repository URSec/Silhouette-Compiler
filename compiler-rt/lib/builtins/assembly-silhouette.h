#ifndef COMPILERRT_ASSEMBLY_SILHOUETTE_H
#define COMPILERRT_ASSEMBLY_SILHOUETTE_H

#ifdef SILHOUETTE

#ifndef SILHOUETTE_SS_OFFSET
#error Shadow stack offset not defined!
#endif /* SILHOUETTE_SS_OFFSET */

#if (SILHOUETTE_SS_OFFSET >= 0) && (SILHOUETTE_SS_OFFSET <= 0xffc)
#define SAVE_TO_SS(reg)   str reg, [sp, #SILHOUETTE_SS_OFFSET]
#define LOAD_FROM_SS(reg) ldr reg, [sp, #SILHOUETTE_SS_OFFSET]
#else /* (SILHOUETTE_SS_OFFSET < 0) || (SILHOUETTE_SS_OFFSET > 0xffc) */

#define ENCODE_SS_OFFSET_LO(reg) movw reg, #(SILHOUETTE_SS_OFFSET & 0xffff)

#if (SILHOUETTE_SS_OFFSET >> 16) != 0
#define ENCODE_SS_OFFSET_HI(reg) movt reg, #((SILHOUETTE_SS_OFFSET >> 16) & 0xffff)
#else
#define ENCODE_SS_OFFSET_HI(reg)
#endif /* (SILHOUETTE_SS_OFFSET >> 16) != 0 */

#define SAVE_TO_SS(reg)                                    \
        ENCODE_SS_OFFSET_LO(ip)                  SEPARATOR \
        ENCODE_SS_OFFSET_HI(ip)                  SEPARATOR \
        str reg, [sp, ip]
#define LOAD_FROM_SS(reg)                                  \
        ENCODE_SS_OFFSET_LO(ip)                  SEPARATOR \
        ENCODE_SS_OFFSET_HI(ip)                  SEPARATOR \
        ldr reg, [sp, ip]
#endif /* (SILHOUETTE_SS_OFFSET >= 0) && (SILHOUETTE_SS_OFFSET <= 0xffc) */

#endif /* SILHOUETTE */

#endif /* COMPILERRT_ASSEMBLY_SILHOUETTE_H */
