/* 
 * tclStubInit.c --
 *
 *	This file contains the initializers for the Tcl stub vectors.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * Remove macros that will interfere with the definitions below.
 */

#undef Tcl_Alloc
#undef Tcl_Free
#undef Tcl_Realloc
#undef Tcl_NewBooleanObj
#undef Tcl_NewByteArrayObj
#undef Tcl_NewDoubleObj
#undef Tcl_NewIntObj
#undef Tcl_NewListObj
#undef Tcl_NewLongObj
#undef Tcl_NewObj
#undef Tcl_NewStringObj
#undef Tcl_DumpActiveMemory
#undef Tcl_ValidateAllMemory
#undef Tcl_FindHashEntry
#undef Tcl_CreateHashEntry
#define TclpLocaltime_unix TclpLocaltime
#define TclpGmtime_unix TclpGmtime

/*
 * Keep a record of the original Notifier procedures, created in the
 * same compilation unit as the stub tables so we can later do reliable,
 * portable comparisons to see whether a Tcl_SetNotifier() call swapped
 * new routines into the stub table.
 */

Tcl_NotifierProcs tclOriginalNotifier = {
    Tcl_SetTimer,
    Tcl_WaitForEvent,
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    Tcl_CreateFileHandler,
    Tcl_DeleteFileHandler,
#else
    NULL,
    NULL,
#endif
    NULL,
    NULL,
    NULL,
    NULL
};

#ifdef __CYGWIN__

#define TclWinGetPlatformId winGetPlatformId
#define Tcl_WinUtfToTChar winUtfToTChar
#define Tcl_WinTCharToUtf winTCharToUtf

static Tcl_Encoding winTCharEncoding;

static int
TclWinGetPlatformId()
{
    /* Don't bother to determine the real platform on cygwin,
     * because VER_PLATFORM_WIN32_NT is the only supported platform */
    return 2; /* VER_PLATFORM_WIN32_NT */;
}

static char *
Tcl_WinUtfToTChar(string, len, dsPtr)
    CONST char *string;
    int len;
    Tcl_DString *dsPtr;
{
    if (!winTCharEncoding) {
	winTCharEncoding = Tcl_GetEncoding(NULL, "unicode");
    }
    return Tcl_UtfToExternalDString(winTCharEncoding,
	    string, len, dsPtr);
}

static char *
Tcl_WinTCharToUtf(string, len, dsPtr)
    CONST char *string;
    int len;
    Tcl_DString *dsPtr;
{
    if (!winTCharEncoding) {
	winTCharEncoding = Tcl_GetEncoding(NULL, "unicode");
    }
    return Tcl_ExternalToUtfDString(winTCharEncoding,
	    string, len, dsPtr);
}

#define Tcl_MacOSXOpenBundleResources (int (*) _ANSI_ARGS_(( \
		Tcl_Interp *, CONST char *, int, int, char *))) Tcl_WinUtfToTChar
#define Tcl_MacOSXOpenVersionedBundleResources (int (*) _ANSI_ARGS_(( \
		Tcl_Interp *, CONST char *, CONST char *, int, int, char *))) Tcl_WinTCharToUtf

#elif !defined(__WIN32__) /* UNIX and MAC */
#   define TclWinGetPlatformId (int (*)()) TclpCreateTempFile
#   ifndef MAC_OSX_TCL
#	define Tcl_MacOSXOpenBundleResources 0
#	define Tcl_MacOSXOpenVersionedBundleResources 0
#   endif
#endif

/*
 * WARNING: The contents of this file is automatically generated by the
 * tools/genStubs.tcl script. Any modifications to the function declarations
 * below should be made in the generic/tcl.decls script.
 */

/* !BEGIN!: Do not edit below this line. */

TclIntStubs tclIntStubs = {
    TCL_STUB_MAGIC,
    NULL,
    NULL, /* 0 */
    TclAccessDeleteProc, /* 1 */
    TclAccessInsertProc, /* 2 */
    TclAllocateFreeObjects, /* 3 */
    NULL, /* 4 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    TclCleanupChildren, /* 5 */
#endif /* UNIX */
#ifdef __WIN32__
    TclCleanupChildren, /* 5 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    NULL, /* 5 */
#endif /* MAC_TCL */
    TclCleanupCommand, /* 6 */
    TclCopyAndCollapse, /* 7 */
    TclCopyChannel, /* 8 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    TclCreatePipeline, /* 9 */
#endif /* UNIX */
#ifdef __WIN32__
    TclCreatePipeline, /* 9 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    NULL, /* 9 */
#endif /* MAC_TCL */
    TclCreateProc, /* 10 */
    TclDeleteCompiledLocalVars, /* 11 */
    TclDeleteVars, /* 12 */
    TclDoGlob, /* 13 */
    TclDumpMemoryInfo, /* 14 */
    NULL, /* 15 */
    TclExprFloatError, /* 16 */
    NULL, /* 17 */
    NULL, /* 18 */
    NULL, /* 19 */
    NULL, /* 20 */
    NULL, /* 21 */
    TclFindElement, /* 22 */
    TclFindProc, /* 23 */
    TclFormatInt, /* 24 */
    TclFreePackageInfo, /* 25 */
    NULL, /* 26 */
    TclGetDate, /* 27 */
    TclpGetDefaultStdChannel, /* 28 */
    NULL, /* 29 */
    NULL, /* 30 */
    TclGetExtension, /* 31 */
    TclGetFrame, /* 32 */
    TclGetInterpProc, /* 33 */
    TclGetIntForIndex, /* 34 */
    NULL, /* 35 */
    TclGetLong, /* 36 */
    TclGetLoadedPackages, /* 37 */
    TclGetNamespaceForQualName, /* 38 */
    TclGetObjInterpProc, /* 39 */
    TclGetOpenMode, /* 40 */
    TclGetOriginalCommand, /* 41 */
    TclpGetUserHome, /* 42 */
    TclGlobalInvoke, /* 43 */
    TclGuessPackageName, /* 44 */
    TclHideUnsafeCommands, /* 45 */
    TclInExit, /* 46 */
    NULL, /* 47 */
    NULL, /* 48 */
    TclIncrVar2, /* 49 */
    TclInitCompiledLocals, /* 50 */
    TclInterpInit, /* 51 */
    TclInvoke, /* 52 */
    TclInvokeObjectCommand, /* 53 */
    TclInvokeStringCommand, /* 54 */
    TclIsProc, /* 55 */
    NULL, /* 56 */
    NULL, /* 57 */
    TclLookupVar, /* 58 */
    NULL, /* 59 */
    TclNeedSpace, /* 60 */
    TclNewProcBodyObj, /* 61 */
    TclObjCommandComplete, /* 62 */
    TclObjInterpProc, /* 63 */
    TclObjInvoke, /* 64 */
    TclObjInvokeGlobal, /* 65 */
    TclOpenFileChannelDeleteProc, /* 66 */
    TclOpenFileChannelInsertProc, /* 67 */
    NULL, /* 68 */
    TclpAlloc, /* 69 */
    NULL, /* 70 */
    NULL, /* 71 */
    NULL, /* 72 */
    NULL, /* 73 */
    TclpFree, /* 74 */
    TclpGetClicks, /* 75 */
    TclpGetSeconds, /* 76 */
    TclpGetTime, /* 77 */
    TclpGetTimeZone, /* 78 */
    NULL, /* 79 */
    NULL, /* 80 */
    TclpRealloc, /* 81 */
    NULL, /* 82 */
    NULL, /* 83 */
    NULL, /* 84 */
    NULL, /* 85 */
    NULL, /* 86 */
    NULL, /* 87 */
    TclPrecTraceProc, /* 88 */
    TclPreventAliasLoop, /* 89 */
    NULL, /* 90 */
    TclProcCleanupProc, /* 91 */
    TclProcCompileProc, /* 92 */
    TclProcDeleteProc, /* 93 */
    TclProcInterpProc, /* 94 */
    NULL, /* 95 */
    TclRenameCommand, /* 96 */
    TclResetShadowedCmdRefs, /* 97 */
    TclServiceIdle, /* 98 */
    NULL, /* 99 */
    NULL, /* 100 */
    TclSetPreInitScript, /* 101 */
    TclSetupEnv, /* 102 */
    TclSockGetPort, /* 103 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    TclSockMinimumBuffers, /* 104 */
#endif /* UNIX */
#ifdef __WIN32__
    TclSockMinimumBuffers, /* 104 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    NULL, /* 104 */
#endif /* MAC_TCL */
    NULL, /* 105 */
    TclStatDeleteProc, /* 106 */
    TclStatInsertProc, /* 107 */
    TclTeardownNamespace, /* 108 */
    TclUpdateReturnInfo, /* 109 */
    NULL, /* 110 */
    Tcl_AddInterpResolvers, /* 111 */
    Tcl_AppendExportList, /* 112 */
    Tcl_CreateNamespace, /* 113 */
    Tcl_DeleteNamespace, /* 114 */
    Tcl_Export, /* 115 */
    Tcl_FindCommand, /* 116 */
    Tcl_FindNamespace, /* 117 */
    Tcl_GetInterpResolvers, /* 118 */
    Tcl_GetNamespaceResolvers, /* 119 */
    Tcl_FindNamespaceVar, /* 120 */
    Tcl_ForgetImport, /* 121 */
    Tcl_GetCommandFromObj, /* 122 */
    Tcl_GetCommandFullName, /* 123 */
    Tcl_GetCurrentNamespace, /* 124 */
    Tcl_GetGlobalNamespace, /* 125 */
    Tcl_GetVariableFullName, /* 126 */
    Tcl_Import, /* 127 */
    Tcl_PopCallFrame, /* 128 */
    Tcl_PushCallFrame, /* 129 */
    Tcl_RemoveInterpResolvers, /* 130 */
    Tcl_SetNamespaceResolvers, /* 131 */
    TclpHasSockets, /* 132 */
    TclpGetDate, /* 133 */
    TclpStrftime, /* 134 */
    TclpCheckStackSpace, /* 135 */
    NULL, /* 136 */
    NULL, /* 137 */
    TclGetEnv, /* 138 */
    NULL, /* 139 */
    TclLooksLikeInt, /* 140 */
    TclpGetCwd, /* 141 */
    TclSetByteCodeFromAny, /* 142 */
    TclAddLiteralObj, /* 143 */
    TclHideLiteral, /* 144 */
    TclGetAuxDataType, /* 145 */
    TclHandleCreate, /* 146 */
    TclHandleFree, /* 147 */
    TclHandlePreserve, /* 148 */
    TclHandleRelease, /* 149 */
    TclRegAbout, /* 150 */
    TclRegExpRangeUniChar, /* 151 */
    TclSetLibraryPath, /* 152 */
    TclGetLibraryPath, /* 153 */
    NULL, /* 154 */
    NULL, /* 155 */
    TclRegError, /* 156 */
    TclVarTraceExists, /* 157 */
    TclSetStartupScriptFileName, /* 158 */
    TclGetStartupScriptFileName, /* 159 */
    NULL, /* 160 */
    TclChannelTransform, /* 161 */
    TclChannelEventScriptInvoker, /* 162 */
    TclGetInstructionTable, /* 163 */
    TclExpandCodeArray, /* 164 */
    TclpSetInitialEncodings, /* 165 */
    TclListObjSetElement, /* 166 */
    TclSetStartupScriptPath, /* 167 */
    TclGetStartupScriptPath, /* 168 */
    TclpUtfNcmp2, /* 169 */
    TclCheckInterpTraces, /* 170 */
    TclCheckExecutionTraces, /* 171 */
    TclInThreadExit, /* 172 */
    TclUniCharMatch, /* 173 */
    NULL, /* 174 */
    NULL, /* 175 */
    NULL, /* 176 */
    NULL, /* 177 */
    NULL, /* 178 */
    NULL, /* 179 */
    NULL, /* 180 */
    NULL, /* 181 */
    TclpLocaltime, /* 182 */
    TclpGmtime, /* 183 */
    NULL, /* 184 */
    NULL, /* 185 */
    NULL, /* 186 */
    NULL, /* 187 */
    NULL, /* 188 */
    NULL, /* 189 */
    NULL, /* 190 */
    NULL, /* 191 */
    NULL, /* 192 */
    NULL, /* 193 */
    NULL, /* 194 */
    NULL, /* 195 */
    NULL, /* 196 */
    NULL, /* 197 */
    NULL, /* 198 */
    TclMatchIsTrivial, /* 199 */
};

TclIntPlatStubs tclIntPlatStubs = {
    TCL_STUB_MAGIC,
    NULL,
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    TclGetAndDetachPids, /* 0 */
    TclpCloseFile, /* 1 */
    TclpCreateCommandChannel, /* 2 */
    TclpCreatePipe, /* 3 */
    TclpCreateProcess, /* 4 */
    NULL, /* 5 */
    TclpMakeFile, /* 6 */
    TclpOpenFile, /* 7 */
    TclUnixWaitForFile, /* 8 */
    TclWinGetPlatformId, /* 9 */
    TclpReaddir, /* 10 */
    TclpLocaltime_unix, /* 11 */
    TclpGmtime_unix, /* 12 */
    TclpInetNtoa, /* 13 */
    NULL, /* 14 */
    NULL, /* 15 */
    NULL, /* 16 */
    NULL, /* 17 */
    NULL, /* 18 */
    NULL, /* 19 */
    NULL, /* 20 */
    NULL, /* 21 */
    TclpCreateTempFile, /* 22 */
    NULL, /* 23 */
    NULL, /* 24 */
    NULL, /* 25 */
    NULL, /* 26 */
    NULL, /* 27 */
    NULL, /* 28 */
    TclWinCPUID, /* 29 */
#endif /* UNIX */
#ifdef __WIN32__
    TclWinConvertError, /* 0 */
    TclWinConvertWSAError, /* 1 */
    TclWinGetServByName, /* 2 */
    TclWinGetSockOpt, /* 3 */
    TclWinGetTclInstance, /* 4 */
    NULL, /* 5 */
    TclWinNToHS, /* 6 */
    TclWinSetSockOpt, /* 7 */
    TclpGetPid, /* 8 */
    TclWinGetPlatformId, /* 9 */
    NULL, /* 10 */
    TclGetAndDetachPids, /* 11 */
    TclpCloseFile, /* 12 */
    TclpCreateCommandChannel, /* 13 */
    TclpCreatePipe, /* 14 */
    TclpCreateProcess, /* 15 */
    NULL, /* 16 */
    NULL, /* 17 */
    TclpMakeFile, /* 18 */
    TclpOpenFile, /* 19 */
    TclWinAddProcess, /* 20 */
    NULL, /* 21 */
    TclpCreateTempFile, /* 22 */
    TclpGetTZName, /* 23 */
    TclWinNoBackslash, /* 24 */
    TclWinGetPlatform, /* 25 */
    TclWinSetInterfaces, /* 26 */
    TclWinFlushDirtyChannels, /* 27 */
    TclWinResetInterfaces, /* 28 */
    TclWinCPUID, /* 29 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TclpSysAlloc, /* 0 */
    TclpSysFree, /* 1 */
    TclpSysRealloc, /* 2 */
    TclpExit, /* 3 */
    FSpGetDefaultDir, /* 4 */
    FSpSetDefaultDir, /* 5 */
    FSpFindFolder, /* 6 */
    GetGlobalMouseTcl, /* 7 */
    FSpGetDirectoryIDTcl, /* 8 */
    FSpOpenResFileCompatTcl, /* 9 */
    FSpCreateResFileCompatTcl, /* 10 */
    FSpLocationFromPath, /* 11 */
    FSpPathFromLocation, /* 12 */
    TclMacExitHandler, /* 13 */
    TclMacInitExitToShell, /* 14 */
    TclMacInstallExitToShellPatch, /* 15 */
    TclMacOSErrorToPosixError, /* 16 */
    TclMacRemoveTimer, /* 17 */
    TclMacStartTimer, /* 18 */
    TclMacTimerExpired, /* 19 */
    TclMacRegisterResourceFork, /* 20 */
    TclMacUnRegisterResourceFork, /* 21 */
    TclMacCreateEnv, /* 22 */
    TclMacFOpenHack, /* 23 */
    TclpGetTZName, /* 24 */
    TclMacChmod, /* 25 */
    FSpLLocationFromPath, /* 26 */
#endif /* MAC_TCL */
};

TclPlatStubs tclPlatStubs = {
    TCL_STUB_MAGIC,
    NULL,
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    Tcl_MacOSXOpenBundleResources, /* 0 */
    Tcl_MacOSXOpenVersionedBundleResources, /* 1 */
#endif /* UNIX */
#ifdef __WIN32__
    Tcl_WinUtfToTChar, /* 0 */
    Tcl_WinTCharToUtf, /* 1 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    Tcl_MacSetEventProc, /* 0 */
    Tcl_MacConvertTextResource, /* 1 */
    Tcl_MacEvalResource, /* 2 */
    Tcl_MacFindResource, /* 3 */
    Tcl_GetOSTypeFromObj, /* 4 */
    Tcl_SetOSTypeObj, /* 5 */
    Tcl_NewOSTypeObj, /* 6 */
    strncasecmp, /* 7 */
    strcasecmp, /* 8 */
#endif /* MAC_TCL */
};

static TclStubHooks tclStubHooks = {
    &tclPlatStubs,
    &tclIntStubs,
    &tclIntPlatStubs
};

TclStubs tclStubs = {
    TCL_STUB_MAGIC,
    &tclStubHooks,
    Tcl_PkgProvideEx, /* 0 */
    Tcl_PkgRequireEx, /* 1 */
    Tcl_Panic, /* 2 */
    Tcl_Alloc, /* 3 */
    Tcl_Free, /* 4 */
    Tcl_Realloc, /* 5 */
    Tcl_DbCkalloc, /* 6 */
    Tcl_DbCkfree, /* 7 */
    Tcl_DbCkrealloc, /* 8 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    Tcl_CreateFileHandler, /* 9 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 9 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    NULL, /* 9 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    Tcl_DeleteFileHandler, /* 10 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 10 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    NULL, /* 10 */
#endif /* MAC_TCL */
    Tcl_SetTimer, /* 11 */
    Tcl_Sleep, /* 12 */
    Tcl_WaitForEvent, /* 13 */
    Tcl_AppendAllObjTypes, /* 14 */
    Tcl_AppendStringsToObj, /* 15 */
    Tcl_AppendToObj, /* 16 */
    Tcl_ConcatObj, /* 17 */
    Tcl_ConvertToType, /* 18 */
    Tcl_DbDecrRefCount, /* 19 */
    Tcl_DbIncrRefCount, /* 20 */
    Tcl_DbIsShared, /* 21 */
    Tcl_DbNewBooleanObj, /* 22 */
    Tcl_DbNewByteArrayObj, /* 23 */
    Tcl_DbNewDoubleObj, /* 24 */
    Tcl_DbNewListObj, /* 25 */
    Tcl_DbNewLongObj, /* 26 */
    Tcl_DbNewObj, /* 27 */
    Tcl_DbNewStringObj, /* 28 */
    Tcl_DuplicateObj, /* 29 */
    TclFreeObj, /* 30 */
    Tcl_GetBoolean, /* 31 */
    Tcl_GetBooleanFromObj, /* 32 */
    Tcl_GetByteArrayFromObj, /* 33 */
    Tcl_GetDouble, /* 34 */
    Tcl_GetDoubleFromObj, /* 35 */
    Tcl_GetIndexFromObj, /* 36 */
    Tcl_GetInt, /* 37 */
    Tcl_GetIntFromObj, /* 38 */
    Tcl_GetLongFromObj, /* 39 */
    Tcl_GetObjType, /* 40 */
    Tcl_GetStringFromObj, /* 41 */
    Tcl_InvalidateStringRep, /* 42 */
    Tcl_ListObjAppendList, /* 43 */
    Tcl_ListObjAppendElement, /* 44 */
    Tcl_ListObjGetElements, /* 45 */
    Tcl_ListObjIndex, /* 46 */
    Tcl_ListObjLength, /* 47 */
    Tcl_ListObjReplace, /* 48 */
    Tcl_NewBooleanObj, /* 49 */
    Tcl_NewByteArrayObj, /* 50 */
    Tcl_NewDoubleObj, /* 51 */
    Tcl_NewIntObj, /* 52 */
    Tcl_NewListObj, /* 53 */
    Tcl_NewLongObj, /* 54 */
    Tcl_NewObj, /* 55 */
    Tcl_NewStringObj, /* 56 */
    Tcl_SetBooleanObj, /* 57 */
    Tcl_SetByteArrayLength, /* 58 */
    Tcl_SetByteArrayObj, /* 59 */
    Tcl_SetDoubleObj, /* 60 */
    Tcl_SetIntObj, /* 61 */
    Tcl_SetListObj, /* 62 */
    Tcl_SetLongObj, /* 63 */
    Tcl_SetObjLength, /* 64 */
    Tcl_SetStringObj, /* 65 */
    Tcl_AddErrorInfo, /* 66 */
    Tcl_AddObjErrorInfo, /* 67 */
    Tcl_AllowExceptions, /* 68 */
    Tcl_AppendElement, /* 69 */
    Tcl_AppendResult, /* 70 */
    Tcl_AsyncCreate, /* 71 */
    Tcl_AsyncDelete, /* 72 */
    Tcl_AsyncInvoke, /* 73 */
    Tcl_AsyncMark, /* 74 */
    Tcl_AsyncReady, /* 75 */
    Tcl_BackgroundError, /* 76 */
    Tcl_Backslash, /* 77 */
    Tcl_BadChannelOption, /* 78 */
    Tcl_CallWhenDeleted, /* 79 */
    Tcl_CancelIdleCall, /* 80 */
    Tcl_Close, /* 81 */
    Tcl_CommandComplete, /* 82 */
    Tcl_Concat, /* 83 */
    Tcl_ConvertElement, /* 84 */
    Tcl_ConvertCountedElement, /* 85 */
    Tcl_CreateAlias, /* 86 */
    Tcl_CreateAliasObj, /* 87 */
    Tcl_CreateChannel, /* 88 */
    Tcl_CreateChannelHandler, /* 89 */
    Tcl_CreateCloseHandler, /* 90 */
    Tcl_CreateCommand, /* 91 */
    Tcl_CreateEventSource, /* 92 */
    Tcl_CreateExitHandler, /* 93 */
    Tcl_CreateInterp, /* 94 */
    Tcl_CreateMathFunc, /* 95 */
    Tcl_CreateObjCommand, /* 96 */
    Tcl_CreateSlave, /* 97 */
    Tcl_CreateTimerHandler, /* 98 */
    Tcl_CreateTrace, /* 99 */
    Tcl_DeleteAssocData, /* 100 */
    Tcl_DeleteChannelHandler, /* 101 */
    Tcl_DeleteCloseHandler, /* 102 */
    Tcl_DeleteCommand, /* 103 */
    Tcl_DeleteCommandFromToken, /* 104 */
    Tcl_DeleteEvents, /* 105 */
    Tcl_DeleteEventSource, /* 106 */
    Tcl_DeleteExitHandler, /* 107 */
    Tcl_DeleteHashEntry, /* 108 */
    Tcl_DeleteHashTable, /* 109 */
    Tcl_DeleteInterp, /* 110 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    Tcl_DetachPids, /* 111 */
#endif /* UNIX */
#ifdef __WIN32__
    Tcl_DetachPids, /* 111 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    NULL, /* 111 */
#endif /* MAC_TCL */
    Tcl_DeleteTimerHandler, /* 112 */
    Tcl_DeleteTrace, /* 113 */
    Tcl_DontCallWhenDeleted, /* 114 */
    Tcl_DoOneEvent, /* 115 */
    Tcl_DoWhenIdle, /* 116 */
    Tcl_DStringAppend, /* 117 */
    Tcl_DStringAppendElement, /* 118 */
    Tcl_DStringEndSublist, /* 119 */
    Tcl_DStringFree, /* 120 */
    Tcl_DStringGetResult, /* 121 */
    Tcl_DStringInit, /* 122 */
    Tcl_DStringResult, /* 123 */
    Tcl_DStringSetLength, /* 124 */
    Tcl_DStringStartSublist, /* 125 */
    Tcl_Eof, /* 126 */
    Tcl_ErrnoId, /* 127 */
    Tcl_ErrnoMsg, /* 128 */
    Tcl_Eval, /* 129 */
    Tcl_EvalFile, /* 130 */
    Tcl_EvalObj, /* 131 */
    Tcl_EventuallyFree, /* 132 */
    Tcl_Exit, /* 133 */
    Tcl_ExposeCommand, /* 134 */
    Tcl_ExprBoolean, /* 135 */
    Tcl_ExprBooleanObj, /* 136 */
    Tcl_ExprDouble, /* 137 */
    Tcl_ExprDoubleObj, /* 138 */
    Tcl_ExprLong, /* 139 */
    Tcl_ExprLongObj, /* 140 */
    Tcl_ExprObj, /* 141 */
    Tcl_ExprString, /* 142 */
    Tcl_Finalize, /* 143 */
    Tcl_FindExecutable, /* 144 */
    Tcl_FirstHashEntry, /* 145 */
    Tcl_Flush, /* 146 */
    Tcl_FreeResult, /* 147 */
    Tcl_GetAlias, /* 148 */
    Tcl_GetAliasObj, /* 149 */
    Tcl_GetAssocData, /* 150 */
    Tcl_GetChannel, /* 151 */
    Tcl_GetChannelBufferSize, /* 152 */
    Tcl_GetChannelHandle, /* 153 */
    Tcl_GetChannelInstanceData, /* 154 */
    Tcl_GetChannelMode, /* 155 */
    Tcl_GetChannelName, /* 156 */
    Tcl_GetChannelOption, /* 157 */
    Tcl_GetChannelType, /* 158 */
    Tcl_GetCommandInfo, /* 159 */
    Tcl_GetCommandName, /* 160 */
    Tcl_GetErrno, /* 161 */
    Tcl_GetHostName, /* 162 */
    Tcl_GetInterpPath, /* 163 */
    Tcl_GetMaster, /* 164 */
    Tcl_GetNameOfExecutable, /* 165 */
    Tcl_GetObjResult, /* 166 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    Tcl_GetOpenFile, /* 167 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 167 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    NULL, /* 167 */
#endif /* MAC_TCL */
    Tcl_GetPathType, /* 168 */
    Tcl_Gets, /* 169 */
    Tcl_GetsObj, /* 170 */
    Tcl_GetServiceMode, /* 171 */
    Tcl_GetSlave, /* 172 */
    Tcl_GetStdChannel, /* 173 */
    Tcl_GetStringResult, /* 174 */
    Tcl_GetVar, /* 175 */
    Tcl_GetVar2, /* 176 */
    Tcl_GlobalEval, /* 177 */
    Tcl_GlobalEvalObj, /* 178 */
    Tcl_HideCommand, /* 179 */
    Tcl_Init, /* 180 */
    Tcl_InitHashTable, /* 181 */
    Tcl_InputBlocked, /* 182 */
    Tcl_InputBuffered, /* 183 */
    Tcl_InterpDeleted, /* 184 */
    Tcl_IsSafe, /* 185 */
    Tcl_JoinPath, /* 186 */
    Tcl_LinkVar, /* 187 */
    NULL, /* 188 */
    Tcl_MakeFileChannel, /* 189 */
    Tcl_MakeSafe, /* 190 */
    Tcl_MakeTcpClientChannel, /* 191 */
    Tcl_Merge, /* 192 */
    Tcl_NextHashEntry, /* 193 */
    Tcl_NotifyChannel, /* 194 */
    Tcl_ObjGetVar2, /* 195 */
    Tcl_ObjSetVar2, /* 196 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    Tcl_OpenCommandChannel, /* 197 */
#endif /* UNIX */
#ifdef __WIN32__
    Tcl_OpenCommandChannel, /* 197 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    NULL, /* 197 */
#endif /* MAC_TCL */
    Tcl_OpenFileChannel, /* 198 */
    Tcl_OpenTcpClient, /* 199 */
    Tcl_OpenTcpServer, /* 200 */
    Tcl_Preserve, /* 201 */
    Tcl_PrintDouble, /* 202 */
    Tcl_PutEnv, /* 203 */
    Tcl_PosixError, /* 204 */
    Tcl_QueueEvent, /* 205 */
    Tcl_Read, /* 206 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    Tcl_ReapDetachedProcs, /* 207 */
#endif /* UNIX */
#ifdef __WIN32__
    Tcl_ReapDetachedProcs, /* 207 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    NULL, /* 207 */
#endif /* MAC_TCL */
    Tcl_RecordAndEval, /* 208 */
    Tcl_RecordAndEvalObj, /* 209 */
    Tcl_RegisterChannel, /* 210 */
    Tcl_RegisterObjType, /* 211 */
    Tcl_RegExpCompile, /* 212 */
    Tcl_RegExpExec, /* 213 */
    Tcl_RegExpMatch, /* 214 */
    Tcl_RegExpRange, /* 215 */
    Tcl_Release, /* 216 */
    Tcl_ResetResult, /* 217 */
    Tcl_ScanElement, /* 218 */
    Tcl_ScanCountedElement, /* 219 */
    Tcl_SeekOld, /* 220 */
    Tcl_ServiceAll, /* 221 */
    Tcl_ServiceEvent, /* 222 */
    Tcl_SetAssocData, /* 223 */
    Tcl_SetChannelBufferSize, /* 224 */
    Tcl_SetChannelOption, /* 225 */
    Tcl_SetCommandInfo, /* 226 */
    Tcl_SetErrno, /* 227 */
    Tcl_SetErrorCode, /* 228 */
    Tcl_SetMaxBlockTime, /* 229 */
    Tcl_SetPanicProc, /* 230 */
    Tcl_SetRecursionLimit, /* 231 */
    Tcl_SetResult, /* 232 */
    Tcl_SetServiceMode, /* 233 */
    Tcl_SetObjErrorCode, /* 234 */
    Tcl_SetObjResult, /* 235 */
    Tcl_SetStdChannel, /* 236 */
    Tcl_SetVar, /* 237 */
    Tcl_SetVar2, /* 238 */
    Tcl_SignalId, /* 239 */
    Tcl_SignalMsg, /* 240 */
    Tcl_SourceRCFile, /* 241 */
    Tcl_SplitList, /* 242 */
    Tcl_SplitPath, /* 243 */
    Tcl_StaticPackage, /* 244 */
    Tcl_StringMatch, /* 245 */
    Tcl_TellOld, /* 246 */
    Tcl_TraceVar, /* 247 */
    Tcl_TraceVar2, /* 248 */
    Tcl_TranslateFileName, /* 249 */
    Tcl_Ungets, /* 250 */
    Tcl_UnlinkVar, /* 251 */
    Tcl_UnregisterChannel, /* 252 */
    Tcl_UnsetVar, /* 253 */
    Tcl_UnsetVar2, /* 254 */
    Tcl_UntraceVar, /* 255 */
    Tcl_UntraceVar2, /* 256 */
    Tcl_UpdateLinkedVar, /* 257 */
    Tcl_UpVar, /* 258 */
    Tcl_UpVar2, /* 259 */
    Tcl_VarEval, /* 260 */
    Tcl_VarTraceInfo, /* 261 */
    Tcl_VarTraceInfo2, /* 262 */
    Tcl_Write, /* 263 */
    Tcl_WrongNumArgs, /* 264 */
    Tcl_DumpActiveMemory, /* 265 */
    Tcl_ValidateAllMemory, /* 266 */
    Tcl_AppendResultVA, /* 267 */
    Tcl_AppendStringsToObjVA, /* 268 */
    Tcl_HashStats, /* 269 */
    Tcl_ParseVar, /* 270 */
    Tcl_PkgPresent, /* 271 */
    Tcl_PkgPresentEx, /* 272 */
    Tcl_PkgProvide, /* 273 */
    Tcl_PkgRequire, /* 274 */
    Tcl_SetErrorCodeVA, /* 275 */
    Tcl_VarEvalVA, /* 276 */
    Tcl_WaitPid, /* 277 */
    Tcl_PanicVA, /* 278 */
    Tcl_GetVersion, /* 279 */
    Tcl_InitMemory, /* 280 */
    Tcl_StackChannel, /* 281 */
    Tcl_UnstackChannel, /* 282 */
    Tcl_GetStackedChannel, /* 283 */
    Tcl_SetMainLoop, /* 284 */
    NULL, /* 285 */
    Tcl_AppendObjToObj, /* 286 */
    Tcl_CreateEncoding, /* 287 */
    Tcl_CreateThreadExitHandler, /* 288 */
    Tcl_DeleteThreadExitHandler, /* 289 */
    Tcl_DiscardResult, /* 290 */
    Tcl_EvalEx, /* 291 */
    Tcl_EvalObjv, /* 292 */
    Tcl_EvalObjEx, /* 293 */
    Tcl_ExitThread, /* 294 */
    Tcl_ExternalToUtf, /* 295 */
    Tcl_ExternalToUtfDString, /* 296 */
    Tcl_FinalizeThread, /* 297 */
    Tcl_FinalizeNotifier, /* 298 */
    Tcl_FreeEncoding, /* 299 */
    Tcl_GetCurrentThread, /* 300 */
    Tcl_GetEncoding, /* 301 */
    Tcl_GetEncodingName, /* 302 */
    Tcl_GetEncodingNames, /* 303 */
    Tcl_GetIndexFromObjStruct, /* 304 */
    Tcl_GetThreadData, /* 305 */
    Tcl_GetVar2Ex, /* 306 */
    Tcl_InitNotifier, /* 307 */
    Tcl_MutexLock, /* 308 */
    Tcl_MutexUnlock, /* 309 */
    Tcl_ConditionNotify, /* 310 */
    Tcl_ConditionWait, /* 311 */
    Tcl_NumUtfChars, /* 312 */
    Tcl_ReadChars, /* 313 */
    Tcl_RestoreResult, /* 314 */
    Tcl_SaveResult, /* 315 */
    Tcl_SetSystemEncoding, /* 316 */
    Tcl_SetVar2Ex, /* 317 */
    Tcl_ThreadAlert, /* 318 */
    Tcl_ThreadQueueEvent, /* 319 */
    Tcl_UniCharAtIndex, /* 320 */
    Tcl_UniCharToLower, /* 321 */
    Tcl_UniCharToTitle, /* 322 */
    Tcl_UniCharToUpper, /* 323 */
    Tcl_UniCharToUtf, /* 324 */
    Tcl_UtfAtIndex, /* 325 */
    Tcl_UtfCharComplete, /* 326 */
    Tcl_UtfBackslash, /* 327 */
    Tcl_UtfFindFirst, /* 328 */
    Tcl_UtfFindLast, /* 329 */
    Tcl_UtfNext, /* 330 */
    Tcl_UtfPrev, /* 331 */
    Tcl_UtfToExternal, /* 332 */
    Tcl_UtfToExternalDString, /* 333 */
    Tcl_UtfToLower, /* 334 */
    Tcl_UtfToTitle, /* 335 */
    Tcl_UtfToUniChar, /* 336 */
    Tcl_UtfToUpper, /* 337 */
    Tcl_WriteChars, /* 338 */
    Tcl_WriteObj, /* 339 */
    Tcl_GetString, /* 340 */
    Tcl_GetDefaultEncodingDir, /* 341 */
    Tcl_SetDefaultEncodingDir, /* 342 */
    Tcl_AlertNotifier, /* 343 */
    Tcl_ServiceModeHook, /* 344 */
    Tcl_UniCharIsAlnum, /* 345 */
    Tcl_UniCharIsAlpha, /* 346 */
    Tcl_UniCharIsDigit, /* 347 */
    Tcl_UniCharIsLower, /* 348 */
    Tcl_UniCharIsSpace, /* 349 */
    Tcl_UniCharIsUpper, /* 350 */
    Tcl_UniCharIsWordChar, /* 351 */
    Tcl_UniCharLen, /* 352 */
    Tcl_UniCharNcmp, /* 353 */
    Tcl_UniCharToUtfDString, /* 354 */
    Tcl_UtfToUniCharDString, /* 355 */
    Tcl_GetRegExpFromObj, /* 356 */
    Tcl_EvalTokens, /* 357 */
    Tcl_FreeParse, /* 358 */
    Tcl_LogCommandInfo, /* 359 */
    Tcl_ParseBraces, /* 360 */
    Tcl_ParseCommand, /* 361 */
    Tcl_ParseExpr, /* 362 */
    Tcl_ParseQuotedString, /* 363 */
    Tcl_ParseVarName, /* 364 */
    Tcl_GetCwd, /* 365 */
    Tcl_Chdir, /* 366 */
    Tcl_Access, /* 367 */
    Tcl_Stat, /* 368 */
    Tcl_UtfNcmp, /* 369 */
    Tcl_UtfNcasecmp, /* 370 */
    Tcl_StringCaseMatch, /* 371 */
    Tcl_UniCharIsControl, /* 372 */
    Tcl_UniCharIsGraph, /* 373 */
    Tcl_UniCharIsPrint, /* 374 */
    Tcl_UniCharIsPunct, /* 375 */
    Tcl_RegExpExecObj, /* 376 */
    Tcl_RegExpGetInfo, /* 377 */
    Tcl_NewUnicodeObj, /* 378 */
    Tcl_SetUnicodeObj, /* 379 */
    Tcl_GetCharLength, /* 380 */
    Tcl_GetUniChar, /* 381 */
    Tcl_GetUnicode, /* 382 */
    Tcl_GetRange, /* 383 */
    Tcl_AppendUnicodeToObj, /* 384 */
    Tcl_RegExpMatchObj, /* 385 */
    Tcl_SetNotifier, /* 386 */
    Tcl_GetAllocMutex, /* 387 */
    Tcl_GetChannelNames, /* 388 */
    Tcl_GetChannelNamesEx, /* 389 */
    Tcl_ProcObjCmd, /* 390 */
    Tcl_ConditionFinalize, /* 391 */
    Tcl_MutexFinalize, /* 392 */
    Tcl_CreateThread, /* 393 */
    Tcl_ReadRaw, /* 394 */
    Tcl_WriteRaw, /* 395 */
    Tcl_GetTopChannel, /* 396 */
    Tcl_ChannelBuffered, /* 397 */
    Tcl_ChannelName, /* 398 */
    Tcl_ChannelVersion, /* 399 */
    Tcl_ChannelBlockModeProc, /* 400 */
    Tcl_ChannelCloseProc, /* 401 */
    Tcl_ChannelClose2Proc, /* 402 */
    Tcl_ChannelInputProc, /* 403 */
    Tcl_ChannelOutputProc, /* 404 */
    Tcl_ChannelSeekProc, /* 405 */
    Tcl_ChannelSetOptionProc, /* 406 */
    Tcl_ChannelGetOptionProc, /* 407 */
    Tcl_ChannelWatchProc, /* 408 */
    Tcl_ChannelGetHandleProc, /* 409 */
    Tcl_ChannelFlushProc, /* 410 */
    Tcl_ChannelHandlerProc, /* 411 */
    Tcl_JoinThread, /* 412 */
    Tcl_IsChannelShared, /* 413 */
    Tcl_IsChannelRegistered, /* 414 */
    Tcl_CutChannel, /* 415 */
    Tcl_SpliceChannel, /* 416 */
    Tcl_ClearChannelHandlers, /* 417 */
    Tcl_IsChannelExisting, /* 418 */
    Tcl_UniCharNcasecmp, /* 419 */
    Tcl_UniCharCaseMatch, /* 420 */
    Tcl_FindHashEntry, /* 421 */
    Tcl_CreateHashEntry, /* 422 */
    Tcl_InitCustomHashTable, /* 423 */
    Tcl_InitObjHashTable, /* 424 */
    Tcl_CommandTraceInfo, /* 425 */
    Tcl_TraceCommand, /* 426 */
    Tcl_UntraceCommand, /* 427 */
    Tcl_AttemptAlloc, /* 428 */
    Tcl_AttemptDbCkalloc, /* 429 */
    Tcl_AttemptRealloc, /* 430 */
    Tcl_AttemptDbCkrealloc, /* 431 */
    Tcl_AttemptSetObjLength, /* 432 */
    Tcl_GetChannelThread, /* 433 */
    Tcl_GetUnicodeFromObj, /* 434 */
    Tcl_GetMathFuncInfo, /* 435 */
    Tcl_ListMathFuncs, /* 436 */
    Tcl_SubstObj, /* 437 */
    Tcl_DetachChannel, /* 438 */
    Tcl_IsStandardChannel, /* 439 */
    Tcl_FSCopyFile, /* 440 */
    Tcl_FSCopyDirectory, /* 441 */
    Tcl_FSCreateDirectory, /* 442 */
    Tcl_FSDeleteFile, /* 443 */
    Tcl_FSLoadFile, /* 444 */
    Tcl_FSMatchInDirectory, /* 445 */
    Tcl_FSLink, /* 446 */
    Tcl_FSRemoveDirectory, /* 447 */
    Tcl_FSRenameFile, /* 448 */
    Tcl_FSLstat, /* 449 */
    Tcl_FSUtime, /* 450 */
    Tcl_FSFileAttrsGet, /* 451 */
    Tcl_FSFileAttrsSet, /* 452 */
    Tcl_FSFileAttrStrings, /* 453 */
    Tcl_FSStat, /* 454 */
    Tcl_FSAccess, /* 455 */
    Tcl_FSOpenFileChannel, /* 456 */
    Tcl_FSGetCwd, /* 457 */
    Tcl_FSChdir, /* 458 */
    Tcl_FSConvertToPathType, /* 459 */
    Tcl_FSJoinPath, /* 460 */
    Tcl_FSSplitPath, /* 461 */
    Tcl_FSEqualPaths, /* 462 */
    Tcl_FSGetNormalizedPath, /* 463 */
    Tcl_FSJoinToPath, /* 464 */
    Tcl_FSGetInternalRep, /* 465 */
    Tcl_FSGetTranslatedPath, /* 466 */
    Tcl_FSEvalFile, /* 467 */
    Tcl_FSNewNativePath, /* 468 */
    Tcl_FSGetNativePath, /* 469 */
    Tcl_FSFileSystemInfo, /* 470 */
    Tcl_FSPathSeparator, /* 471 */
    Tcl_FSListVolumes, /* 472 */
    Tcl_FSRegister, /* 473 */
    Tcl_FSUnregister, /* 474 */
    Tcl_FSData, /* 475 */
    Tcl_FSGetTranslatedStringPath, /* 476 */
    Tcl_FSGetFileSystemForPath, /* 477 */
    Tcl_FSGetPathType, /* 478 */
    Tcl_OutputBuffered, /* 479 */
    Tcl_FSMountsChanged, /* 480 */
    Tcl_EvalTokensStandard, /* 481 */
    Tcl_GetTime, /* 482 */
    Tcl_CreateObjTrace, /* 483 */
    Tcl_GetCommandInfoFromToken, /* 484 */
    Tcl_SetCommandInfoFromToken, /* 485 */
    Tcl_DbNewWideIntObj, /* 486 */
    Tcl_GetWideIntFromObj, /* 487 */
    Tcl_NewWideIntObj, /* 488 */
    Tcl_SetWideIntObj, /* 489 */
    Tcl_AllocStatBuf, /* 490 */
    Tcl_Seek, /* 491 */
    Tcl_Tell, /* 492 */
    Tcl_ChannelWideSeekProc, /* 493 */
    NULL, /* 494 */
    NULL, /* 495 */
    NULL, /* 496 */
    NULL, /* 497 */
    NULL, /* 498 */
    NULL, /* 499 */
    NULL, /* 500 */
    NULL, /* 501 */
    NULL, /* 502 */
    NULL, /* 503 */
    NULL, /* 504 */
    NULL, /* 505 */
    NULL, /* 506 */
    NULL, /* 507 */
    NULL, /* 508 */
    NULL, /* 509 */
    NULL, /* 510 */
    NULL, /* 511 */
    NULL, /* 512 */
    NULL, /* 513 */
    NULL, /* 514 */
    NULL, /* 515 */
    NULL, /* 516 */
    NULL, /* 517 */
    NULL, /* 518 */
    NULL, /* 519 */
    NULL, /* 520 */
    NULL, /* 521 */
    NULL, /* 522 */
    NULL, /* 523 */
    NULL, /* 524 */
    NULL, /* 525 */
    NULL, /* 526 */
    NULL, /* 527 */
    NULL, /* 528 */
    NULL, /* 529 */
    NULL, /* 530 */
    NULL, /* 531 */
    NULL, /* 532 */
    NULL, /* 533 */
    NULL, /* 534 */
    NULL, /* 535 */
    NULL, /* 536 */
    NULL, /* 537 */
    NULL, /* 538 */
    NULL, /* 539 */
    NULL, /* 540 */
    NULL, /* 541 */
    NULL, /* 542 */
    NULL, /* 543 */
    NULL, /* 544 */
    NULL, /* 545 */
    NULL, /* 546 */
    NULL, /* 547 */
    NULL, /* 548 */
    NULL, /* 549 */
    NULL, /* 550 */
    NULL, /* 551 */
    NULL, /* 552 */
    NULL, /* 553 */
    Tcl_ChannelThreadActionProc, /* 554 */
    NULL, /* 555 */
    NULL, /* 556 */
    NULL, /* 557 */
    NULL, /* 558 */
    NULL, /* 559 */
    NULL, /* 560 */
    NULL, /* 561 */
    NULL, /* 562 */
    NULL, /* 563 */
    NULL, /* 564 */
    NULL, /* 565 */
    NULL, /* 566 */
    NULL, /* 567 */
    NULL, /* 568 */
    NULL, /* 569 */
    NULL, /* 570 */
    NULL, /* 571 */
    NULL, /* 572 */
    Tcl_PkgRequireProc, /* 573 */
};

/* !END!: Do not edit above this line. */
