/* This is used to enable code which passes library base references to be
 * compiled for OS4 without changes, by just redefining them as library
 * interface references. One limitation is that code that needs access to
 * both library base and interface, like library open code needs to be
 * handled separately - now handled in the startup code.
 *
 * Library function calling without changes for OS5 is still achieved by
 * -D__USE_INLINE__.
 */
#ifndef __OS4_COMPATIBILITY_H__
#define __OS4_COMPATIBILITY_H__

#ifdef __amigaos4__
struct ExecIFace;
#define ExecBase ExecIFace
#define SysBase IExec
struct DOSIFace;
#define DosLibrary DOSIFace
#define DOSBase IDOS
// Add more as needed
#endif

#endif
