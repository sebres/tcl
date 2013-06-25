/*
 * tclIntPlatDecls.h --
 *
 *	This file contains the declarations for all platform dependent
 *	unsupported functions that are exported by the Tcl library.  These
 *	interfaces are not guaranteed to remain the same between
 *	versions.  Use at your own risk.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * All rights reserved.
 */

#ifndef _TCLINTPLATDECLS
#define _TCLINTPLATDECLS

/*
 * WARNING: This file is automatically generated by the tools/genStubs.tcl
 * script.  Any modifications to the function declarations below should be made
 * in the generic/tclInt.decls script.
 */

/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */

#if !defined(__WIN32__) && !defined(__CYGWIN__) && !defined(MAC_OSX_TCL) /* UNIX */
/* 0 */
TCLAPI void		TclGetAndDetachPids(Tcl_Interp *interp,
				Tcl_Channel chan);
/* 1 */
TCLAPI int		TclpCloseFile(TclFile file);
/* 2 */
TCLAPI Tcl_Channel	TclpCreateCommandChannel(TclFile readFile,
				TclFile writeFile, TclFile errorFile,
				int numPids, Tcl_Pid *pidPtr);
/* 3 */
TCLAPI int		TclpCreatePipe(TclFile *readPipe, TclFile *writePipe);
/* 4 */
TCLAPI int		TclpCreateProcess(Tcl_Interp *interp, int argc,
				const char **argv, TclFile inputFile,
				TclFile outputFile, TclFile errorFile,
				Tcl_Pid *pidPtr);
/* Slot 5 is reserved */
/* 6 */
TCLAPI TclFile		TclpMakeFile(Tcl_Channel channel, int direction);
/* 7 */
TCLAPI TclFile		TclpOpenFile(const char *fname, int mode);
/* 8 */
TCLAPI int		TclUnixWaitForFile(int fd, int mask, int timeout);
/* 9 */
TCLAPI TclFile		TclpCreateTempFile(const char *contents);
/* Slot 10 is reserved */
/* Slot 11 is reserved */
/* Slot 12 is reserved */
/* Slot 13 is reserved */
/* 14 */
TCLAPI int		TclUnixCopyFile(const char *src, const char *dst,
				const Tcl_StatBuf *statBufPtr,
				int dontCopyAtts);
/* Slot 15 is reserved */
/* Slot 16 is reserved */
/* Slot 17 is reserved */
/* Slot 18 is reserved */
/* Slot 19 is reserved */
/* Slot 20 is reserved */
/* Slot 21 is reserved */
/* Slot 22 is reserved */
/* Slot 23 is reserved */
/* Slot 24 is reserved */
/* Slot 25 is reserved */
/* Slot 26 is reserved */
/* Slot 27 is reserved */
/* Slot 28 is reserved */
/* 29 */
TCLAPI int		TclWinCPUID(unsigned int index, unsigned int *regs);
/* 30 */
TCLAPI int		TclUnixOpenTemporaryFile(Tcl_Obj *dirObj,
				Tcl_Obj *basenameObj, Tcl_Obj *extensionObj,
				Tcl_Obj *resultingNameObj);
#endif /* UNIX */
#if defined(__WIN32__) || defined(__CYGWIN__) /* WIN */
/* 0 */
TCLAPI void		TclWinConvertError(DWORD errCode);
/* Slot 1 is reserved */
/* 2 */
TCLAPI struct servent *	 TclWinGetServByName(const char *nm,
				const char *proto);
/* 3 */
TCLAPI int		TclWinGetSockOpt(SOCKET s, int level, int optname,
				char *optval, int *optlen);
/* 4 */
TCLAPI HINSTANCE	TclWinGetTclInstance(void);
/* 5 */
TCLAPI int		TclUnixWaitForFile(int fd, int mask, int timeout);
/* Slot 6 is reserved */
/* 7 */
TCLAPI int		TclWinSetSockOpt(SOCKET s, int level, int optname,
				const char *optval, int optlen);
/* 8 */
TCLAPI int		TclpGetPid(Tcl_Pid pid);
/* 9 */
TCLAPI int		TclWinGetPlatformId(void);
/* Slot 10 is reserved */
/* 11 */
TCLAPI void		TclGetAndDetachPids(Tcl_Interp *interp,
				Tcl_Channel chan);
/* 12 */
TCLAPI int		TclpCloseFile(TclFile file);
/* 13 */
TCLAPI Tcl_Channel	TclpCreateCommandChannel(TclFile readFile,
				TclFile writeFile, TclFile errorFile,
				int numPids, Tcl_Pid *pidPtr);
/* 14 */
TCLAPI int		TclpCreatePipe(TclFile *readPipe, TclFile *writePipe);
/* 15 */
TCLAPI int		TclpCreateProcess(Tcl_Interp *interp, int argc,
				const char **argv, TclFile inputFile,
				TclFile outputFile, TclFile errorFile,
				Tcl_Pid *pidPtr);
/* 16 */
TCLAPI int		TclpIsAtty(int fd);
/* 17 */
TCLAPI int		TclUnixCopyFile(const char *src, const char *dst,
				const Tcl_StatBuf *statBufPtr,
				int dontCopyAtts);
/* 18 */
TCLAPI TclFile		TclpMakeFile(Tcl_Channel channel, int direction);
/* 19 */
TCLAPI TclFile		TclpOpenFile(const char *fname, int mode);
/* 20 */
TCLAPI void		TclWinAddProcess(HANDLE hProcess, DWORD id);
/* Slot 21 is reserved */
/* 22 */
TCLAPI TclFile		TclpCreateTempFile(const char *contents);
/* Slot 23 is reserved */
/* 24 */
TCLAPI char *		TclWinNoBackslash(char *path);
/* Slot 25 is reserved */
/* 26 */
TCLAPI void		TclWinSetInterfaces(int wide);
/* 27 */
TCLAPI void		TclWinFlushDirtyChannels(void);
/* 28 */
TCLAPI void		TclWinResetInterfaces(void);
/* 29 */
TCLAPI int		TclWinCPUID(unsigned int index, unsigned int *regs);
/* 30 */
TCLAPI int		TclUnixOpenTemporaryFile(Tcl_Obj *dirObj,
				Tcl_Obj *basenameObj, Tcl_Obj *extensionObj,
				Tcl_Obj *resultingNameObj);
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
/* 0 */
TCLAPI void		TclGetAndDetachPids(Tcl_Interp *interp,
				Tcl_Channel chan);
/* 1 */
TCLAPI int		TclpCloseFile(TclFile file);
/* 2 */
TCLAPI Tcl_Channel	TclpCreateCommandChannel(TclFile readFile,
				TclFile writeFile, TclFile errorFile,
				int numPids, Tcl_Pid *pidPtr);
/* 3 */
TCLAPI int		TclpCreatePipe(TclFile *readPipe, TclFile *writePipe);
/* 4 */
TCLAPI int		TclpCreateProcess(Tcl_Interp *interp, int argc,
				const char **argv, TclFile inputFile,
				TclFile outputFile, TclFile errorFile,
				Tcl_Pid *pidPtr);
/* Slot 5 is reserved */
/* 6 */
TCLAPI TclFile		TclpMakeFile(Tcl_Channel channel, int direction);
/* 7 */
TCLAPI TclFile		TclpOpenFile(const char *fname, int mode);
/* 8 */
TCLAPI int		TclUnixWaitForFile(int fd, int mask, int timeout);
/* 9 */
TCLAPI TclFile		TclpCreateTempFile(const char *contents);
/* Slot 10 is reserved */
/* Slot 11 is reserved */
/* Slot 12 is reserved */
/* Slot 13 is reserved */
/* 14 */
TCLAPI int		TclUnixCopyFile(const char *src, const char *dst,
				const Tcl_StatBuf *statBufPtr,
				int dontCopyAtts);
/* 15 */
TCLAPI int		TclMacOSXGetFileAttribute(Tcl_Interp *interp,
				int objIndex, Tcl_Obj *fileName,
				Tcl_Obj **attributePtrPtr);
/* 16 */
TCLAPI int		TclMacOSXSetFileAttribute(Tcl_Interp *interp,
				int objIndex, Tcl_Obj *fileName,
				Tcl_Obj *attributePtr);
/* 17 */
TCLAPI int		TclMacOSXCopyFileAttributes(const char *src,
				const char *dst,
				const Tcl_StatBuf *statBufPtr);
/* 18 */
TCLAPI int		TclMacOSXMatchType(Tcl_Interp *interp,
				const char *pathName, const char *fileName,
				Tcl_StatBuf *statBufPtr,
				Tcl_GlobTypeData *types);
/* 19 */
TCLAPI void		TclMacOSXNotifierAddRunLoopMode(
				const void *runLoopMode);
/* Slot 20 is reserved */
/* Slot 21 is reserved */
/* Slot 22 is reserved */
/* Slot 23 is reserved */
/* Slot 24 is reserved */
/* Slot 25 is reserved */
/* Slot 26 is reserved */
/* Slot 27 is reserved */
/* Slot 28 is reserved */
/* 29 */
TCLAPI int		TclWinCPUID(unsigned int index, unsigned int *regs);
/* 30 */
TCLAPI int		TclUnixOpenTemporaryFile(Tcl_Obj *dirObj,
				Tcl_Obj *basenameObj, Tcl_Obj *extensionObj,
				Tcl_Obj *resultingNameObj);
#endif /* MACOSX */

typedef struct TclIntPlatStubs {
    int magic;
    void *hooks;

#if !defined(__WIN32__) && !defined(__CYGWIN__) && !defined(MAC_OSX_TCL) /* UNIX */
    void (*tclGetAndDetachPids) (Tcl_Interp *interp, Tcl_Channel chan); /* 0 */
    int (*tclpCloseFile) (TclFile file); /* 1 */
    Tcl_Channel (*tclpCreateCommandChannel) (TclFile readFile, TclFile writeFile, TclFile errorFile, int numPids, Tcl_Pid *pidPtr); /* 2 */
    int (*tclpCreatePipe) (TclFile *readPipe, TclFile *writePipe); /* 3 */
    int (*tclpCreateProcess) (Tcl_Interp *interp, int argc, const char **argv, TclFile inputFile, TclFile outputFile, TclFile errorFile, Tcl_Pid *pidPtr); /* 4 */
    void (*reserved5)(void);
    TclFile (*tclpMakeFile) (Tcl_Channel channel, int direction); /* 6 */
    TclFile (*tclpOpenFile) (const char *fname, int mode); /* 7 */
    int (*tclUnixWaitForFile) (int fd, int mask, int timeout); /* 8 */
    TclFile (*tclpCreateTempFile) (const char *contents); /* 9 */
    void (*reserved10)(void);
    void (*reserved11)(void);
    void (*reserved12)(void);
    void (*reserved13)(void);
    int (*tclUnixCopyFile) (const char *src, const char *dst, const Tcl_StatBuf *statBufPtr, int dontCopyAtts); /* 14 */
    void (*reserved15)(void);
    void (*reserved16)(void);
    void (*reserved17)(void);
    void (*reserved18)(void);
    void (*reserved19)(void);
    void (*reserved20)(void);
    void (*reserved21)(void);
    void (*reserved22)(void);
    void (*reserved23)(void);
    void (*reserved24)(void);
    void (*reserved25)(void);
    void (*reserved26)(void);
    void (*reserved27)(void);
    void (*reserved28)(void);
    int (*tclWinCPUID) (unsigned int index, unsigned int *regs); /* 29 */
    int (*tclUnixOpenTemporaryFile) (Tcl_Obj *dirObj, Tcl_Obj *basenameObj, Tcl_Obj *extensionObj, Tcl_Obj *resultingNameObj); /* 30 */
#endif /* UNIX */
#if defined(__WIN32__) || defined(__CYGWIN__) /* WIN */
    void (*tclWinConvertError) (DWORD errCode); /* 0 */
    void (*reserved1)(void);
    struct servent * (*tclWinGetServByName) (const char *nm, const char *proto); /* 2 */
    int (*tclWinGetSockOpt) (SOCKET s, int level, int optname, char *optval, int *optlen); /* 3 */
    HINSTANCE (*tclWinGetTclInstance) (void); /* 4 */
    int (*tclUnixWaitForFile) (int fd, int mask, int timeout); /* 5 */
    void (*reserved6)(void);
    int (*tclWinSetSockOpt) (SOCKET s, int level, int optname, const char *optval, int optlen); /* 7 */
    int (*tclpGetPid) (Tcl_Pid pid); /* 8 */
    int (*tclWinGetPlatformId) (void); /* 9 */
    void (*reserved10)(void);
    void (*tclGetAndDetachPids) (Tcl_Interp *interp, Tcl_Channel chan); /* 11 */
    int (*tclpCloseFile) (TclFile file); /* 12 */
    Tcl_Channel (*tclpCreateCommandChannel) (TclFile readFile, TclFile writeFile, TclFile errorFile, int numPids, Tcl_Pid *pidPtr); /* 13 */
    int (*tclpCreatePipe) (TclFile *readPipe, TclFile *writePipe); /* 14 */
    int (*tclpCreateProcess) (Tcl_Interp *interp, int argc, const char **argv, TclFile inputFile, TclFile outputFile, TclFile errorFile, Tcl_Pid *pidPtr); /* 15 */
    int (*tclpIsAtty) (int fd); /* 16 */
    int (*tclUnixCopyFile) (const char *src, const char *dst, const Tcl_StatBuf *statBufPtr, int dontCopyAtts); /* 17 */
    TclFile (*tclpMakeFile) (Tcl_Channel channel, int direction); /* 18 */
    TclFile (*tclpOpenFile) (const char *fname, int mode); /* 19 */
    void (*tclWinAddProcess) (HANDLE hProcess, DWORD id); /* 20 */
    void (*reserved21)(void);
    TclFile (*tclpCreateTempFile) (const char *contents); /* 22 */
    void (*reserved23)(void);
    char * (*tclWinNoBackslash) (char *path); /* 24 */
    void (*reserved25)(void);
    void (*tclWinSetInterfaces) (int wide); /* 26 */
    void (*tclWinFlushDirtyChannels) (void); /* 27 */
    void (*tclWinResetInterfaces) (void); /* 28 */
    int (*tclWinCPUID) (unsigned int index, unsigned int *regs); /* 29 */
    int (*tclUnixOpenTemporaryFile) (Tcl_Obj *dirObj, Tcl_Obj *basenameObj, Tcl_Obj *extensionObj, Tcl_Obj *resultingNameObj); /* 30 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
    void (*tclGetAndDetachPids) (Tcl_Interp *interp, Tcl_Channel chan); /* 0 */
    int (*tclpCloseFile) (TclFile file); /* 1 */
    Tcl_Channel (*tclpCreateCommandChannel) (TclFile readFile, TclFile writeFile, TclFile errorFile, int numPids, Tcl_Pid *pidPtr); /* 2 */
    int (*tclpCreatePipe) (TclFile *readPipe, TclFile *writePipe); /* 3 */
    int (*tclpCreateProcess) (Tcl_Interp *interp, int argc, const char **argv, TclFile inputFile, TclFile outputFile, TclFile errorFile, Tcl_Pid *pidPtr); /* 4 */
    void (*reserved5)(void);
    TclFile (*tclpMakeFile) (Tcl_Channel channel, int direction); /* 6 */
    TclFile (*tclpOpenFile) (const char *fname, int mode); /* 7 */
    int (*tclUnixWaitForFile) (int fd, int mask, int timeout); /* 8 */
    TclFile (*tclpCreateTempFile) (const char *contents); /* 9 */
    void (*reserved10)(void);
    void (*reserved11)(void);
    void (*reserved12)(void);
    void (*reserved13)(void);
    int (*tclUnixCopyFile) (const char *src, const char *dst, const Tcl_StatBuf *statBufPtr, int dontCopyAtts); /* 14 */
    int (*tclMacOSXGetFileAttribute) (Tcl_Interp *interp, int objIndex, Tcl_Obj *fileName, Tcl_Obj **attributePtrPtr); /* 15 */
    int (*tclMacOSXSetFileAttribute) (Tcl_Interp *interp, int objIndex, Tcl_Obj *fileName, Tcl_Obj *attributePtr); /* 16 */
    int (*tclMacOSXCopyFileAttributes) (const char *src, const char *dst, const Tcl_StatBuf *statBufPtr); /* 17 */
    int (*tclMacOSXMatchType) (Tcl_Interp *interp, const char *pathName, const char *fileName, Tcl_StatBuf *statBufPtr, Tcl_GlobTypeData *types); /* 18 */
    void (*tclMacOSXNotifierAddRunLoopMode) (const void *runLoopMode); /* 19 */
    void (*reserved20)(void);
    void (*reserved21)(void);
    void (*reserved22)(void);
    void (*reserved23)(void);
    void (*reserved24)(void);
    void (*reserved25)(void);
    void (*reserved26)(void);
    void (*reserved27)(void);
    void (*reserved28)(void);
    int (*tclWinCPUID) (unsigned int index, unsigned int *regs); /* 29 */
    int (*tclUnixOpenTemporaryFile) (Tcl_Obj *dirObj, Tcl_Obj *basenameObj, Tcl_Obj *extensionObj, Tcl_Obj *resultingNameObj); /* 30 */
#endif /* MACOSX */
} TclIntPlatStubs;

#ifdef __cplusplus
extern "C" {
#endif
extern const TclIntPlatStubs *tclIntPlatStubsPtr;
#ifdef __cplusplus
}
#endif

#if !defined(BUILD_tcl) || defined(USE_TCL_STUBS)

/*
 * Inline function declarations:
 */

#if !defined(__WIN32__) && !defined(__CYGWIN__) && !defined(MAC_OSX_TCL) /* UNIX */
#define TclGetAndDetachPids \
	(tclIntPlatStubsPtr->tclGetAndDetachPids) /* 0 */
#define TclpCloseFile \
	(tclIntPlatStubsPtr->tclpCloseFile) /* 1 */
#define TclpCreateCommandChannel \
	(tclIntPlatStubsPtr->tclpCreateCommandChannel) /* 2 */
#define TclpCreatePipe \
	(tclIntPlatStubsPtr->tclpCreatePipe) /* 3 */
#define TclpCreateProcess \
	(tclIntPlatStubsPtr->tclpCreateProcess) /* 4 */
/* Slot 5 is reserved */
#define TclpMakeFile \
	(tclIntPlatStubsPtr->tclpMakeFile) /* 6 */
#define TclpOpenFile \
	(tclIntPlatStubsPtr->tclpOpenFile) /* 7 */
#define TclUnixWaitForFile \
	(tclIntPlatStubsPtr->tclUnixWaitForFile) /* 8 */
#define TclpCreateTempFile \
	(tclIntPlatStubsPtr->tclpCreateTempFile) /* 9 */
/* Slot 10 is reserved */
/* Slot 11 is reserved */
/* Slot 12 is reserved */
/* Slot 13 is reserved */
#define TclUnixCopyFile \
	(tclIntPlatStubsPtr->tclUnixCopyFile) /* 14 */
/* Slot 15 is reserved */
/* Slot 16 is reserved */
/* Slot 17 is reserved */
/* Slot 18 is reserved */
/* Slot 19 is reserved */
/* Slot 20 is reserved */
/* Slot 21 is reserved */
/* Slot 22 is reserved */
/* Slot 23 is reserved */
/* Slot 24 is reserved */
/* Slot 25 is reserved */
/* Slot 26 is reserved */
/* Slot 27 is reserved */
/* Slot 28 is reserved */
#define TclWinCPUID \
	(tclIntPlatStubsPtr->tclWinCPUID) /* 29 */
#define TclUnixOpenTemporaryFile \
	(tclIntPlatStubsPtr->tclUnixOpenTemporaryFile) /* 30 */
#endif /* UNIX */
#if defined(__WIN32__) || defined(__CYGWIN__) /* WIN */
#define TclWinConvertError \
	(tclIntPlatStubsPtr->tclWinConvertError) /* 0 */
/* Slot 1 is reserved */
#define TclWinGetServByName \
	(tclIntPlatStubsPtr->tclWinGetServByName) /* 2 */
#define TclWinGetSockOpt \
	(tclIntPlatStubsPtr->tclWinGetSockOpt) /* 3 */
#define TclWinGetTclInstance \
	(tclIntPlatStubsPtr->tclWinGetTclInstance) /* 4 */
#define TclUnixWaitForFile \
	(tclIntPlatStubsPtr->tclUnixWaitForFile) /* 5 */
/* Slot 6 is reserved */
#define TclWinSetSockOpt \
	(tclIntPlatStubsPtr->tclWinSetSockOpt) /* 7 */
#define TclpGetPid \
	(tclIntPlatStubsPtr->tclpGetPid) /* 8 */
#define TclWinGetPlatformId \
	(tclIntPlatStubsPtr->tclWinGetPlatformId) /* 9 */
/* Slot 10 is reserved */
#define TclGetAndDetachPids \
	(tclIntPlatStubsPtr->tclGetAndDetachPids) /* 11 */
#define TclpCloseFile \
	(tclIntPlatStubsPtr->tclpCloseFile) /* 12 */
#define TclpCreateCommandChannel \
	(tclIntPlatStubsPtr->tclpCreateCommandChannel) /* 13 */
#define TclpCreatePipe \
	(tclIntPlatStubsPtr->tclpCreatePipe) /* 14 */
#define TclpCreateProcess \
	(tclIntPlatStubsPtr->tclpCreateProcess) /* 15 */
#define TclpIsAtty \
	(tclIntPlatStubsPtr->tclpIsAtty) /* 16 */
#define TclUnixCopyFile \
	(tclIntPlatStubsPtr->tclUnixCopyFile) /* 17 */
#define TclpMakeFile \
	(tclIntPlatStubsPtr->tclpMakeFile) /* 18 */
#define TclpOpenFile \
	(tclIntPlatStubsPtr->tclpOpenFile) /* 19 */
#define TclWinAddProcess \
	(tclIntPlatStubsPtr->tclWinAddProcess) /* 20 */
/* Slot 21 is reserved */
#define TclpCreateTempFile \
	(tclIntPlatStubsPtr->tclpCreateTempFile) /* 22 */
/* Slot 23 is reserved */
#define TclWinNoBackslash \
	(tclIntPlatStubsPtr->tclWinNoBackslash) /* 24 */
/* Slot 25 is reserved */
#define TclWinSetInterfaces \
	(tclIntPlatStubsPtr->tclWinSetInterfaces) /* 26 */
#define TclWinFlushDirtyChannels \
	(tclIntPlatStubsPtr->tclWinFlushDirtyChannels) /* 27 */
#define TclWinResetInterfaces \
	(tclIntPlatStubsPtr->tclWinResetInterfaces) /* 28 */
#define TclWinCPUID \
	(tclIntPlatStubsPtr->tclWinCPUID) /* 29 */
#define TclUnixOpenTemporaryFile \
	(tclIntPlatStubsPtr->tclUnixOpenTemporaryFile) /* 30 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
#define TclGetAndDetachPids \
	(tclIntPlatStubsPtr->tclGetAndDetachPids) /* 0 */
#define TclpCloseFile \
	(tclIntPlatStubsPtr->tclpCloseFile) /* 1 */
#define TclpCreateCommandChannel \
	(tclIntPlatStubsPtr->tclpCreateCommandChannel) /* 2 */
#define TclpCreatePipe \
	(tclIntPlatStubsPtr->tclpCreatePipe) /* 3 */
#define TclpCreateProcess \
	(tclIntPlatStubsPtr->tclpCreateProcess) /* 4 */
/* Slot 5 is reserved */
#define TclpMakeFile \
	(tclIntPlatStubsPtr->tclpMakeFile) /* 6 */
#define TclpOpenFile \
	(tclIntPlatStubsPtr->tclpOpenFile) /* 7 */
#define TclUnixWaitForFile \
	(tclIntPlatStubsPtr->tclUnixWaitForFile) /* 8 */
#define TclpCreateTempFile \
	(tclIntPlatStubsPtr->tclpCreateTempFile) /* 9 */
/* Slot 10 is reserved */
/* Slot 11 is reserved */
/* Slot 12 is reserved */
/* Slot 13 is reserved */
#define TclUnixCopyFile \
	(tclIntPlatStubsPtr->tclUnixCopyFile) /* 14 */
#define TclMacOSXGetFileAttribute \
	(tclIntPlatStubsPtr->tclMacOSXGetFileAttribute) /* 15 */
#define TclMacOSXSetFileAttribute \
	(tclIntPlatStubsPtr->tclMacOSXSetFileAttribute) /* 16 */
#define TclMacOSXCopyFileAttributes \
	(tclIntPlatStubsPtr->tclMacOSXCopyFileAttributes) /* 17 */
#define TclMacOSXMatchType \
	(tclIntPlatStubsPtr->tclMacOSXMatchType) /* 18 */
#define TclMacOSXNotifierAddRunLoopMode \
	(tclIntPlatStubsPtr->tclMacOSXNotifierAddRunLoopMode) /* 19 */
/* Slot 20 is reserved */
/* Slot 21 is reserved */
/* Slot 22 is reserved */
/* Slot 23 is reserved */
/* Slot 24 is reserved */
/* Slot 25 is reserved */
/* Slot 26 is reserved */
/* Slot 27 is reserved */
/* Slot 28 is reserved */
#define TclWinCPUID \
	(tclIntPlatStubsPtr->tclWinCPUID) /* 29 */
#define TclUnixOpenTemporaryFile \
	(tclIntPlatStubsPtr->tclUnixOpenTemporaryFile) /* 30 */
#endif /* MACOSX */

#endif /* !BUILD_tcl || USE_TCL_STUBS */

/* !END!: Do not edit above this line. */

#if !defined(__WIN32__) && !defined(__CYGWIN__)
#   undef TclpGetPid
#   define TclpGetPid(pid) ((unsigned long) (pid))
#endif

#endif /* _TCLINTPLATDECLS */
