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
#include "tommath_private.h"

#ifdef __CYGWIN__
#   include <wchar.h>
#endif

#ifdef __GNUC__
#pragma GCC dependency "tcl.decls"
#pragma GCC dependency "tclInt.decls"
#pragma GCC dependency "tclTomMath.decls"
#endif

/*
 * Remove macros that will interfere with the definitions below.
 */

#undef Tcl_Alloc
#undef Tcl_Free
#undef Tcl_Realloc
#undef Tcl_NewByteArrayObj
#undef Tcl_NewDoubleObj
#undef Tcl_NewListObj
#undef Tcl_NewLongObj
#undef Tcl_DbNewLongObj
#undef Tcl_NewObj
#undef Tcl_NewStringObj
#undef Tcl_GetUnicode
#undef Tcl_DumpActiveMemory
#undef Tcl_ValidateAllMemory
#undef Tcl_SetExitProc
#undef Tcl_SetPanicProc
#undef TclpGetPid
#undef TclStaticPackage
#undef Tcl_BackgroundError
#undef Tcl_UtfToUniChar
#undef Tcl_UtfToUniCharDString
#undef Tcl_UniCharToUtfDString
#define TclStaticPackage Tcl_StaticPackage

#ifdef TCL_MEM_DEBUG
#   define Tcl_Alloc TclpAlloc
#   define Tcl_Free TclpFree
#   define Tcl_Realloc TclpRealloc
#   undef Tcl_AttemptAlloc
#   define Tcl_AttemptAlloc TclpAlloc
#   undef Tcl_AttemptRealloc
#   define Tcl_AttemptRealloc TclpRealloc
#endif

MP_SET_UNSIGNED(mp_set_ull, Tcl_WideUInt)
MP_SET_SIGNED(mp_set_ll, mp_set_ull, Tcl_WideInt, Tcl_WideUInt)
MP_GET_MAG(mp_get_mag_ull, Tcl_WideUInt)

mp_err TclBN_mp_set_int(mp_int *a, unsigned long i)
{
    mp_set_ul(a, i);
    return MP_OKAY;
}

static mp_err TclBN_mp_set_long(mp_int *a, unsigned long i)
{
	mp_set_ul(a, i);
	return MP_OKAY;
}

#define TclBN_mp_set_ul (void (*)(mp_int *a, unsigned long i))TclBN_mp_set_long

mp_err MP_WUR TclBN_mp_expt_u32(const mp_int *a, unsigned int b, mp_int *c) {
	return TclBN_s_mp_expt_u32(a, b, c);
}

mp_err	TclBN_mp_add_d(const mp_int *a, unsigned int b, mp_int *c) {
   return TclBN_s_mp_add_d(a, b, c);
}
mp_err	TclBN_mp_cmp_d(const mp_int *a, unsigned int b) {
   return TclBN_s_mp_cmp_d(a, b);
}
mp_err	TclBN_mp_sub_d(const mp_int *a, unsigned int b, mp_int *c) {
   return TclBN_s_mp_sub_d(a, b, c);
}

mp_err	TclBN_mp_div_d(const mp_int *a, unsigned int b, mp_int *c, unsigned int *d) {
   mp_digit d2;
   mp_err result = TclBN_s_mp_div_d(a, b, c, (d ? &d2 : NULL));
   if (d) {
      *d = d2;
   }
   return result;
}
mp_err TclBN_mp_init_set(mp_int *a, unsigned int b) {
	return TclBN_s_mp_init_set(a, b);
}
mp_err	TclBN_mp_mul_d(const mp_int *a, unsigned int b, mp_int *c) {
	return TclBN_s_mp_mul_d(a, b, c);
}
void TclBN_mp_set(mp_int *a, unsigned int b) {
	TclBN_s_mp_set(a, b);
}



#ifdef _WIN32
#   define TclUnixWaitForFile 0
#   define TclUnixCopyFile 0
#   define TclUnixOpenTemporaryFile 0
#   define TclpIsAtty 0
#elif defined(__CYGWIN__)
#   define TclpIsAtty TclPlatIsAtty
static void
doNothing(void)
{
    /* dummy implementation, no need to do anything */
}
#   define TclWinAddProcess (void (*) (void *, size_t)) doNothing
#   define TclWinFlushDirtyChannels doNothing
static int
TclpIsAtty(int fd)
{
    return isatty(fd);
}

void *TclWinGetTclInstance()
{
    void *hInstance = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
	    (const char *)&TclpIsAtty, &hInstance);
    return hInstance;
}

#define TclWinNoBackslash winNoBackslash
static char *
TclWinNoBackslash(char *path)
{
    char *p;

    for (p = path; *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }
    return path;
}

size_t
TclpGetPid(Tcl_Pid pid)
{
    return (size_t) pid;
}

#if defined(TCL_WIDE_INT_IS_LONG)
/* On Cygwin64, long is 64-bit while on Win64 long is 32-bit. Therefore
 * we have to make sure that all stub entries on Cygwin64 follow the Win64
 * signature. Tcl 9 must find a better solution, but that cannot be done
 * without introducing a binary incompatibility.
 */
static int exprInt(Tcl_Interp *interp, const char *expr, int *ptr){
    long longValue;
    int result = Tcl_ExprLong(interp, expr, &longValue);
    if (result == TCL_OK) {
	    if ((longValue >= (long)(INT_MIN))
		    && (longValue <= (long)(UINT_MAX))) {
	    *ptr = (int)longValue;
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "integer value too large to represent as non-long integer", -1));
	    result = TCL_ERROR;
	}
    }
    return result;
}
#define Tcl_ExprLong (int(*)(Tcl_Interp*,const char*,long*))exprInt
static int exprIntObj(Tcl_Interp *interp, Tcl_Obj*expr, int *ptr){
    long longValue;
    int result = Tcl_ExprLongObj(interp, expr, &longValue);
    if (result == TCL_OK) {
	    if ((longValue >= (long)(INT_MIN))
		    && (longValue <= (long)(UINT_MAX))) {
	    *ptr = (int)longValue;
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "integer value too large to represent as non-long integer", -1));
	    result = TCL_ERROR;
	}
    }
    return result;
}
#define Tcl_ExprLongObj (int(*)(Tcl_Interp*,Tcl_Obj*,long*))exprIntObj
#endif /* TCL_WIDE_INT_IS_LONG */

#endif /* __CYGWIN__ */

/*
 * WARNING: The contents of this file is automatically generated by the
 * tools/genStubs.tcl script. Any modifications to the function declarations
 * below should be made in the generic/tcl.decls script.
 */

MODULE_SCOPE const TclStubs tclStubs;
MODULE_SCOPE const TclTomMathStubs tclTomMathStubs;

#ifdef __GNUC__
/*
 * The rest of this file shouldn't warn about deprecated functions; they're
 * there because we intend them to be so and know that this file is OK to
 * touch those fields.
 */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* !BEGIN!: Do not edit below this line. */

static const TclIntStubs tclIntStubs = {
    TCL_STUB_MAGIC,
    0,
    0, /* 0 */
    0, /* 1 */
    0, /* 2 */
    TclAllocateFreeObjects, /* 3 */
    0, /* 4 */
    TclCleanupChildren, /* 5 */
    TclCleanupCommand, /* 6 */
    TclCopyAndCollapse, /* 7 */
    0, /* 8 */
    TclCreatePipeline, /* 9 */
    TclCreateProc, /* 10 */
    TclDeleteCompiledLocalVars, /* 11 */
    TclDeleteVars, /* 12 */
    0, /* 13 */
    TclDumpMemoryInfo, /* 14 */
    0, /* 15 */
    TclExprFloatError, /* 16 */
    0, /* 17 */
    0, /* 18 */
    0, /* 19 */
    0, /* 20 */
    0, /* 21 */
    TclFindElement, /* 22 */
    TclFindProc, /* 23 */
    TclFormatInt, /* 24 */
    TclFreePackageInfo, /* 25 */
    0, /* 26 */
    0, /* 27 */
    TclpGetDefaultStdChannel, /* 28 */
    0, /* 29 */
    0, /* 30 */
    TclGetExtension, /* 31 */
    TclGetFrame, /* 32 */
    0, /* 33 */
    0, /* 34 */
    0, /* 35 */
    0, /* 36 */
    TclGetLoadedPackages, /* 37 */
    TclGetNamespaceForQualName, /* 38 */
    TclGetObjInterpProc, /* 39 */
    TclGetOpenMode, /* 40 */
    TclGetOriginalCommand, /* 41 */
    TclpGetUserHome, /* 42 */
    0, /* 43 */
    TclGuessPackageName, /* 44 */
    TclHideUnsafeCommands, /* 45 */
    TclInExit, /* 46 */
    0, /* 47 */
    0, /* 48 */
    0, /* 49 */
    TclInitCompiledLocals, /* 50 */
    TclInterpInit, /* 51 */
    0, /* 52 */
    TclInvokeObjectCommand, /* 53 */
    TclInvokeStringCommand, /* 54 */
    TclIsProc, /* 55 */
    0, /* 56 */
    0, /* 57 */
    TclLookupVar, /* 58 */
    0, /* 59 */
    TclNeedSpace, /* 60 */
    TclNewProcBodyObj, /* 61 */
    TclObjCommandComplete, /* 62 */
    TclObjInterpProc, /* 63 */
    TclObjInvoke, /* 64 */
    0, /* 65 */
    0, /* 66 */
    0, /* 67 */
    0, /* 68 */
    TclpAlloc, /* 69 */
    0, /* 70 */
    0, /* 71 */
    0, /* 72 */
    0, /* 73 */
    TclpFree, /* 74 */
    TclpGetClicks, /* 75 */
    TclpGetSeconds, /* 76 */
    0, /* 77 */
    0, /* 78 */
    0, /* 79 */
    0, /* 80 */
    TclpRealloc, /* 81 */
    0, /* 82 */
    0, /* 83 */
    0, /* 84 */
    0, /* 85 */
    0, /* 86 */
    0, /* 87 */
    0, /* 88 */
    TclPreventAliasLoop, /* 89 */
    0, /* 90 */
    TclProcCleanupProc, /* 91 */
    TclProcCompileProc, /* 92 */
    TclProcDeleteProc, /* 93 */
    0, /* 94 */
    0, /* 95 */
    TclRenameCommand, /* 96 */
    TclResetShadowedCmdRefs, /* 97 */
    TclServiceIdle, /* 98 */
    0, /* 99 */
    0, /* 100 */
    TclSetPreInitScript, /* 101 */
    TclSetupEnv, /* 102 */
    TclSockGetPort, /* 103 */
    0, /* 104 */
    0, /* 105 */
    0, /* 106 */
    0, /* 107 */
    TclTeardownNamespace, /* 108 */
    TclUpdateReturnInfo, /* 109 */
    TclSockMinimumBuffers, /* 110 */
    Tcl_AddInterpResolvers, /* 111 */
    0, /* 112 */
    0, /* 113 */
    0, /* 114 */
    0, /* 115 */
    0, /* 116 */
    0, /* 117 */
    Tcl_GetInterpResolvers, /* 118 */
    Tcl_GetNamespaceResolvers, /* 119 */
    Tcl_FindNamespaceVar, /* 120 */
    0, /* 121 */
    0, /* 122 */
    0, /* 123 */
    0, /* 124 */
    0, /* 125 */
    Tcl_GetVariableFullName, /* 126 */
    0, /* 127 */
    Tcl_PopCallFrame, /* 128 */
    Tcl_PushCallFrame, /* 129 */
    Tcl_RemoveInterpResolvers, /* 130 */
    Tcl_SetNamespaceResolvers, /* 131 */
    TclpHasSockets, /* 132 */
    0, /* 133 */
    0, /* 134 */
    0, /* 135 */
    0, /* 136 */
    0, /* 137 */
    TclGetEnv, /* 138 */
    0, /* 139 */
    0, /* 140 */
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
    0, /* 154 */
    0, /* 155 */
    TclRegError, /* 156 */
    TclVarTraceExists, /* 157 */
    0, /* 158 */
    0, /* 159 */
    0, /* 160 */
    TclChannelTransform, /* 161 */
    TclChannelEventScriptInvoker, /* 162 */
    TclGetInstructionTable, /* 163 */
    TclExpandCodeArray, /* 164 */
    TclpSetInitialEncodings, /* 165 */
    TclListObjSetElement, /* 166 */
    0, /* 167 */
    0, /* 168 */
    TclpUtfNcmp2, /* 169 */
    TclCheckInterpTraces, /* 170 */
    TclCheckExecutionTraces, /* 171 */
    TclInThreadExit, /* 172 */
    TclUniCharMatch, /* 173 */
    0, /* 174 */
    TclCallVarTraces, /* 175 */
    TclCleanupVar, /* 176 */
    TclVarErrMsg, /* 177 */
    0, /* 178 */
    0, /* 179 */
    0, /* 180 */
    0, /* 181 */
    0, /* 182 */
    0, /* 183 */
    0, /* 184 */
    0, /* 185 */
    0, /* 186 */
    0, /* 187 */
    0, /* 188 */
    0, /* 189 */
    0, /* 190 */
    0, /* 191 */
    0, /* 192 */
    0, /* 193 */
    0, /* 194 */
    0, /* 195 */
    0, /* 196 */
    0, /* 197 */
    TclObjGetFrame, /* 198 */
    0, /* 199 */
    TclpObjRemoveDirectory, /* 200 */
    TclpObjCopyDirectory, /* 201 */
    TclpObjCreateDirectory, /* 202 */
    TclpObjDeleteFile, /* 203 */
    TclpObjCopyFile, /* 204 */
    TclpObjRenameFile, /* 205 */
    TclpObjStat, /* 206 */
    TclpObjAccess, /* 207 */
    TclpOpenFileChannel, /* 208 */
    0, /* 209 */
    0, /* 210 */
    0, /* 211 */
    TclpFindExecutable, /* 212 */
    TclGetObjNameOfExecutable, /* 213 */
    TclSetObjNameOfExecutable, /* 214 */
    TclStackAlloc, /* 215 */
    TclStackFree, /* 216 */
    TclPushStackFrame, /* 217 */
    TclPopStackFrame, /* 218 */
    0, /* 219 */
    0, /* 220 */
    0, /* 221 */
    0, /* 222 */
    0, /* 223 */
    TclGetPlatform, /* 224 */
    TclTraceDictPath, /* 225 */
    TclObjBeingDeleted, /* 226 */
    TclSetNsPath, /* 227 */
    0, /* 228 */
    TclPtrMakeUpvar, /* 229 */
    TclObjLookupVar, /* 230 */
    TclGetNamespaceFromObj, /* 231 */
    TclEvalObjEx, /* 232 */
    TclGetSrcInfoForPc, /* 233 */
    TclVarHashCreateVar, /* 234 */
    TclInitVarHashTable, /* 235 */
    0, /* 236 */
    TclResetCancellation, /* 237 */
    TclNRInterpProc, /* 238 */
    TclNRInterpProcCore, /* 239 */
    TclNRRunCallbacks, /* 240 */
    TclNREvalObjEx, /* 241 */
    TclNREvalObjv, /* 242 */
    TclDbDumpActiveObjects, /* 243 */
    TclGetNamespaceChildTable, /* 244 */
    TclGetNamespaceCommandTable, /* 245 */
    TclInitRewriteEnsemble, /* 246 */
    TclResetRewriteEnsemble, /* 247 */
    TclCopyChannel, /* 248 */
    TclDoubleDigits, /* 249 */
    TclSetSlaveCancelFlags, /* 250 */
    TclRegisterLiteral, /* 251 */
    TclPtrGetVar, /* 252 */
    TclPtrSetVar, /* 253 */
    TclPtrIncrObjVar, /* 254 */
    TclPtrObjMakeUpvar, /* 255 */
    TclPtrUnsetVar, /* 256 */
    TclStaticPackage, /* 257 */
    TclpCreateTemporaryDirectory, /* 258 */
    TclMSB, /* 259 */
};

static const TclIntPlatStubs tclIntPlatStubs = {
    TCL_STUB_MAGIC,
    0,
#if !defined(_WIN32) && !defined(__CYGWIN__) && !defined(MAC_OSX_TCL) /* UNIX */
    TclGetAndDetachPids, /* 0 */
    TclpCloseFile, /* 1 */
    TclpCreateCommandChannel, /* 2 */
    TclpCreatePipe, /* 3 */
    TclpCreateProcess, /* 4 */
    0, /* 5 */
    TclpMakeFile, /* 6 */
    TclpOpenFile, /* 7 */
    TclUnixWaitForFile, /* 8 */
    TclpCreateTempFile, /* 9 */
    0, /* 10 */
    0, /* 11 */
    0, /* 12 */
    0, /* 13 */
    TclUnixCopyFile, /* 14 */
    0, /* 15 */
    0, /* 16 */
    0, /* 17 */
    0, /* 18 */
    0, /* 19 */
    0, /* 20 */
    0, /* 21 */
    0, /* 22 */
    0, /* 23 */
    0, /* 24 */
    0, /* 25 */
    0, /* 26 */
    0, /* 27 */
    0, /* 28 */
    TclWinCPUID, /* 29 */
    TclUnixOpenTemporaryFile, /* 30 */
#endif /* UNIX */
#if defined(_WIN32) || defined(__CYGWIN__) /* WIN */
    TclWinConvertError, /* 0 */
    0, /* 1 */
    0, /* 2 */
    0, /* 3 */
    TclWinGetTclInstance, /* 4 */
    TclUnixWaitForFile, /* 5 */
    0, /* 6 */
    0, /* 7 */
    TclpGetPid, /* 8 */
    0, /* 9 */
    0, /* 10 */
    TclGetAndDetachPids, /* 11 */
    TclpCloseFile, /* 12 */
    TclpCreateCommandChannel, /* 13 */
    TclpCreatePipe, /* 14 */
    TclpCreateProcess, /* 15 */
    TclpIsAtty, /* 16 */
    TclUnixCopyFile, /* 17 */
    TclpMakeFile, /* 18 */
    TclpOpenFile, /* 19 */
    TclWinAddProcess, /* 20 */
    0, /* 21 */
    TclpCreateTempFile, /* 22 */
    0, /* 23 */
    TclWinNoBackslash, /* 24 */
    0, /* 25 */
    0, /* 26 */
    TclWinFlushDirtyChannels, /* 27 */
    0, /* 28 */
    TclWinCPUID, /* 29 */
    TclUnixOpenTemporaryFile, /* 30 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
    TclGetAndDetachPids, /* 0 */
    TclpCloseFile, /* 1 */
    TclpCreateCommandChannel, /* 2 */
    TclpCreatePipe, /* 3 */
    TclpCreateProcess, /* 4 */
    0, /* 5 */
    TclpMakeFile, /* 6 */
    TclpOpenFile, /* 7 */
    TclUnixWaitForFile, /* 8 */
    TclpCreateTempFile, /* 9 */
    0, /* 10 */
    0, /* 11 */
    0, /* 12 */
    0, /* 13 */
    TclUnixCopyFile, /* 14 */
    TclMacOSXGetFileAttribute, /* 15 */
    TclMacOSXSetFileAttribute, /* 16 */
    TclMacOSXCopyFileAttributes, /* 17 */
    TclMacOSXMatchType, /* 18 */
    TclMacOSXNotifierAddRunLoopMode, /* 19 */
    0, /* 20 */
    0, /* 21 */
    0, /* 22 */
    0, /* 23 */
    0, /* 24 */
    0, /* 25 */
    0, /* 26 */
    0, /* 27 */
    0, /* 28 */
    TclWinCPUID, /* 29 */
    TclUnixOpenTemporaryFile, /* 30 */
#endif /* MACOSX */
};

static const TclPlatStubs tclPlatStubs = {
    TCL_STUB_MAGIC,
    0,
#ifdef MAC_OSX_TCL /* MACOSX */
    Tcl_MacOSXOpenBundleResources, /* 0 */
    Tcl_MacOSXOpenVersionedBundleResources, /* 1 */
#endif /* MACOSX */
};

const TclTomMathStubs tclTomMathStubs = {
    TCL_STUB_MAGIC,
    0,
    TclBN_epoch, /* 0 */
    TclBN_revision, /* 1 */
    TclBN_mp_add, /* 2 */
    TclBN_mp_add_d, /* 3 */
    TclBN_mp_and, /* 4 */
    TclBN_mp_clamp, /* 5 */
    TclBN_mp_clear, /* 6 */
    TclBN_mp_clear_multi, /* 7 */
    TclBN_mp_cmp, /* 8 */
    TclBN_mp_cmp_d, /* 9 */
    TclBN_mp_cmp_mag, /* 10 */
    TclBN_mp_copy, /* 11 */
    TclBN_mp_count_bits, /* 12 */
    TclBN_mp_div, /* 13 */
    TclBN_mp_div_d, /* 14 */
    TclBN_mp_div_2, /* 15 */
    TclBN_mp_div_2d, /* 16 */
    0, /* 17 */
    TclBN_mp_exch, /* 18 */
    TclBN_mp_expt_u32, /* 19 */
    TclBN_mp_grow, /* 20 */
    TclBN_mp_init, /* 21 */
    TclBN_mp_init_copy, /* 22 */
    TclBN_mp_init_multi, /* 23 */
    TclBN_mp_init_set, /* 24 */
    TclBN_mp_init_size, /* 25 */
    TclBN_mp_lshd, /* 26 */
    TclBN_mp_mod, /* 27 */
    TclBN_mp_mod_2d, /* 28 */
    TclBN_mp_mul, /* 29 */
    TclBN_mp_mul_d, /* 30 */
    TclBN_mp_mul_2, /* 31 */
    TclBN_mp_mul_2d, /* 32 */
    TclBN_mp_neg, /* 33 */
    TclBN_mp_or, /* 34 */
    TclBN_mp_radix_size, /* 35 */
    TclBN_mp_read_radix, /* 36 */
    TclBN_mp_rshd, /* 37 */
    TclBN_mp_shrink, /* 38 */
    TclBN_mp_set, /* 39 */
    0, /* 40 */
    TclBN_mp_sqrt, /* 41 */
    TclBN_mp_sub, /* 42 */
    TclBN_mp_sub_d, /* 43 */
    0, /* 44 */
    0, /* 45 */
    0, /* 46 */
    TclBN_mp_ubin_size, /* 47 */
    TclBN_mp_xor, /* 48 */
    TclBN_mp_zero, /* 49 */
    0, /* 50 */
    0, /* 51 */
    0, /* 52 */
    0, /* 53 */
    0, /* 54 */
    0, /* 55 */
    0, /* 56 */
    0, /* 57 */
    0, /* 58 */
    0, /* 59 */
    0, /* 60 */
    TclBN_mp_init_ul, /* 61 */
    TclBN_mp_set_ul, /* 62 */
    TclBN_mp_cnt_lsb, /* 63 */
    TclBNInitBignumFromLong, /* 64 */
    TclBNInitBignumFromWideInt, /* 65 */
    TclBNInitBignumFromWideUInt, /* 66 */
    0, /* 67 */
    TclBN_mp_set_ull, /* 68 */
    TclBN_mp_get_mag_ull, /* 69 */
    TclBN_mp_set_ll, /* 70 */
    TclBN_mp_get_mag_ul, /* 71 */
    TclBN_mp_set_l, /* 72 */
    0, /* 73 */
    0, /* 74 */
    0, /* 75 */
    TclBN_mp_signed_rsh, /* 76 */
    0, /* 77 */
    TclBN_mp_to_ubin, /* 78 */
    0, /* 79 */
    TclBN_mp_to_radix, /* 80 */
};

static const TclStubHooks tclStubHooks = {
    &tclPlatStubs,
    &tclIntStubs,
    &tclIntPlatStubs
};

const TclStubs tclStubs = {
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
#if !defined(_WIN32) && !defined(MAC_OSX_TCL) /* UNIX */
    Tcl_CreateFileHandler, /* 9 */
#endif /* UNIX */
#if defined(_WIN32) /* WIN */
    0, /* 9 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
    Tcl_CreateFileHandler, /* 9 */
#endif /* MACOSX */
#if !defined(_WIN32) && !defined(MAC_OSX_TCL) /* UNIX */
    Tcl_DeleteFileHandler, /* 10 */
#endif /* UNIX */
#if defined(_WIN32) /* WIN */
    0, /* 10 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
    Tcl_DeleteFileHandler, /* 10 */
#endif /* MACOSX */
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
    0, /* 22 */
    Tcl_DbNewByteArrayObj, /* 23 */
    Tcl_DbNewDoubleObj, /* 24 */
    Tcl_DbNewListObj, /* 25 */
    0, /* 26 */
    Tcl_DbNewObj, /* 27 */
    Tcl_DbNewStringObj, /* 28 */
    Tcl_DuplicateObj, /* 29 */
    0, /* 30 */
    Tcl_GetBoolean, /* 31 */
    Tcl_GetBooleanFromObj, /* 32 */
    Tcl_GetByteArrayFromObj, /* 33 */
    Tcl_GetDouble, /* 34 */
    Tcl_GetDoubleFromObj, /* 35 */
    0, /* 36 */
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
    0, /* 49 */
    Tcl_NewByteArrayObj, /* 50 */
    Tcl_NewDoubleObj, /* 51 */
    0, /* 52 */
    Tcl_NewListObj, /* 53 */
    0, /* 54 */
    Tcl_NewObj, /* 55 */
    Tcl_NewStringObj, /* 56 */
    0, /* 57 */
    Tcl_SetByteArrayLength, /* 58 */
    Tcl_SetByteArrayObj, /* 59 */
    Tcl_SetDoubleObj, /* 60 */
    0, /* 61 */
    Tcl_SetListObj, /* 62 */
    0, /* 63 */
    Tcl_SetObjLength, /* 64 */
    Tcl_SetStringObj, /* 65 */
    0, /* 66 */
    0, /* 67 */
    Tcl_AllowExceptions, /* 68 */
    Tcl_AppendElement, /* 69 */
    Tcl_AppendResult, /* 70 */
    Tcl_AsyncCreate, /* 71 */
    Tcl_AsyncDelete, /* 72 */
    Tcl_AsyncInvoke, /* 73 */
    Tcl_AsyncMark, /* 74 */
    Tcl_AsyncReady, /* 75 */
    0, /* 76 */
    0, /* 77 */
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
    0, /* 95 */
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
    Tcl_DetachPids, /* 111 */
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
    0, /* 129 */
    Tcl_EvalFile, /* 130 */
    0, /* 131 */
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
    0, /* 144 */
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
#if !defined(_WIN32) && !defined(MAC_OSX_TCL) /* UNIX */
    Tcl_GetOpenFile, /* 167 */
#endif /* UNIX */
#if defined(_WIN32) /* WIN */
    0, /* 167 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
    Tcl_GetOpenFile, /* 167 */
#endif /* MACOSX */
    Tcl_GetPathType, /* 168 */
    Tcl_Gets, /* 169 */
    Tcl_GetsObj, /* 170 */
    Tcl_GetServiceMode, /* 171 */
    Tcl_GetSlave, /* 172 */
    Tcl_GetStdChannel, /* 173 */
    0, /* 174 */
    0, /* 175 */
    Tcl_GetVar2, /* 176 */
    0, /* 177 */
    0, /* 178 */
    Tcl_HideCommand, /* 179 */
    Tcl_Init, /* 180 */
    Tcl_InitHashTable, /* 181 */
    Tcl_InputBlocked, /* 182 */
    Tcl_InputBuffered, /* 183 */
    Tcl_InterpDeleted, /* 184 */
    Tcl_IsSafe, /* 185 */
    Tcl_JoinPath, /* 186 */
    Tcl_LinkVar, /* 187 */
    0, /* 188 */
    Tcl_MakeFileChannel, /* 189 */
    Tcl_MakeSafe, /* 190 */
    Tcl_MakeTcpClientChannel, /* 191 */
    Tcl_Merge, /* 192 */
    Tcl_NextHashEntry, /* 193 */
    Tcl_NotifyChannel, /* 194 */
    Tcl_ObjGetVar2, /* 195 */
    Tcl_ObjSetVar2, /* 196 */
    Tcl_OpenCommandChannel, /* 197 */
    Tcl_OpenFileChannel, /* 198 */
    Tcl_OpenTcpClient, /* 199 */
    Tcl_OpenTcpServer, /* 200 */
    Tcl_Preserve, /* 201 */
    Tcl_PrintDouble, /* 202 */
    Tcl_PutEnv, /* 203 */
    Tcl_PosixError, /* 204 */
    Tcl_QueueEvent, /* 205 */
    Tcl_Read, /* 206 */
    Tcl_ReapDetachedProcs, /* 207 */
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
    0, /* 220 */
    Tcl_ServiceAll, /* 221 */
    Tcl_ServiceEvent, /* 222 */
    Tcl_SetAssocData, /* 223 */
    Tcl_SetChannelBufferSize, /* 224 */
    Tcl_SetChannelOption, /* 225 */
    Tcl_SetCommandInfo, /* 226 */
    Tcl_SetErrno, /* 227 */
    Tcl_SetErrorCode, /* 228 */
    Tcl_SetMaxBlockTime, /* 229 */
    0, /* 230 */
    Tcl_SetRecursionLimit, /* 231 */
    0, /* 232 */
    Tcl_SetServiceMode, /* 233 */
    Tcl_SetObjErrorCode, /* 234 */
    Tcl_SetObjResult, /* 235 */
    Tcl_SetStdChannel, /* 236 */
    0, /* 237 */
    Tcl_SetVar2, /* 238 */
    Tcl_SignalId, /* 239 */
    Tcl_SignalMsg, /* 240 */
    Tcl_SourceRCFile, /* 241 */
    Tcl_SplitList, /* 242 */
    Tcl_SplitPath, /* 243 */
    0, /* 244 */
    0, /* 245 */
    0, /* 246 */
    0, /* 247 */
    Tcl_TraceVar2, /* 248 */
    Tcl_TranslateFileName, /* 249 */
    Tcl_Ungets, /* 250 */
    Tcl_UnlinkVar, /* 251 */
    Tcl_UnregisterChannel, /* 252 */
    0, /* 253 */
    Tcl_UnsetVar2, /* 254 */
    0, /* 255 */
    Tcl_UntraceVar2, /* 256 */
    Tcl_UpdateLinkedVar, /* 257 */
    0, /* 258 */
    Tcl_UpVar2, /* 259 */
    Tcl_VarEval, /* 260 */
    0, /* 261 */
    Tcl_VarTraceInfo2, /* 262 */
    Tcl_Write, /* 263 */
    Tcl_WrongNumArgs, /* 264 */
    Tcl_DumpActiveMemory, /* 265 */
    Tcl_ValidateAllMemory, /* 266 */
    0, /* 267 */
    0, /* 268 */
    Tcl_HashStats, /* 269 */
    Tcl_ParseVar, /* 270 */
    0, /* 271 */
    Tcl_PkgPresentEx, /* 272 */
    0, /* 273 */
    0, /* 274 */
    0, /* 275 */
    0, /* 276 */
    Tcl_WaitPid, /* 277 */
    0, /* 278 */
    Tcl_GetVersion, /* 279 */
    Tcl_InitMemory, /* 280 */
    Tcl_StackChannel, /* 281 */
    Tcl_UnstackChannel, /* 282 */
    Tcl_GetStackedChannel, /* 283 */
    Tcl_SetMainLoop, /* 284 */
    0, /* 285 */
    Tcl_AppendObjToObj, /* 286 */
    Tcl_CreateEncoding, /* 287 */
    Tcl_CreateThreadExitHandler, /* 288 */
    Tcl_DeleteThreadExitHandler, /* 289 */
    0, /* 290 */
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
    0, /* 314 */
    0, /* 315 */
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
    Tcl_UtfToChar16, /* 336 */
    Tcl_UtfToUpper, /* 337 */
    Tcl_WriteChars, /* 338 */
    Tcl_WriteObj, /* 339 */
    Tcl_GetString, /* 340 */
    0, /* 341 */
    0, /* 342 */
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
    Tcl_Char16ToUtfDString, /* 354 */
    Tcl_UtfToChar16DString, /* 355 */
    Tcl_GetRegExpFromObj, /* 356 */
    0, /* 357 */
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
    0, /* 382 */
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
    0, /* 421 */
    0, /* 422 */
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
    0, /* 435 */
    0, /* 436 */
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
    Tcl_DictObjPut, /* 494 */
    Tcl_DictObjGet, /* 495 */
    Tcl_DictObjRemove, /* 496 */
    Tcl_DictObjSize, /* 497 */
    Tcl_DictObjFirst, /* 498 */
    Tcl_DictObjNext, /* 499 */
    Tcl_DictObjDone, /* 500 */
    Tcl_DictObjPutKeyList, /* 501 */
    Tcl_DictObjRemoveKeyList, /* 502 */
    Tcl_NewDictObj, /* 503 */
    Tcl_DbNewDictObj, /* 504 */
    Tcl_RegisterConfig, /* 505 */
    Tcl_CreateNamespace, /* 506 */
    Tcl_DeleteNamespace, /* 507 */
    Tcl_AppendExportList, /* 508 */
    Tcl_Export, /* 509 */
    Tcl_Import, /* 510 */
    Tcl_ForgetImport, /* 511 */
    Tcl_GetCurrentNamespace, /* 512 */
    Tcl_GetGlobalNamespace, /* 513 */
    Tcl_FindNamespace, /* 514 */
    Tcl_FindCommand, /* 515 */
    Tcl_GetCommandFromObj, /* 516 */
    Tcl_GetCommandFullName, /* 517 */
    Tcl_FSEvalFileEx, /* 518 */
    0, /* 519 */
    Tcl_LimitAddHandler, /* 520 */
    Tcl_LimitRemoveHandler, /* 521 */
    Tcl_LimitReady, /* 522 */
    Tcl_LimitCheck, /* 523 */
    Tcl_LimitExceeded, /* 524 */
    Tcl_LimitSetCommands, /* 525 */
    Tcl_LimitSetTime, /* 526 */
    Tcl_LimitSetGranularity, /* 527 */
    Tcl_LimitTypeEnabled, /* 528 */
    Tcl_LimitTypeExceeded, /* 529 */
    Tcl_LimitTypeSet, /* 530 */
    Tcl_LimitTypeReset, /* 531 */
    Tcl_LimitGetCommands, /* 532 */
    Tcl_LimitGetTime, /* 533 */
    Tcl_LimitGetGranularity, /* 534 */
    Tcl_SaveInterpState, /* 535 */
    Tcl_RestoreInterpState, /* 536 */
    Tcl_DiscardInterpState, /* 537 */
    Tcl_SetReturnOptions, /* 538 */
    Tcl_GetReturnOptions, /* 539 */
    Tcl_IsEnsemble, /* 540 */
    Tcl_CreateEnsemble, /* 541 */
    Tcl_FindEnsemble, /* 542 */
    Tcl_SetEnsembleSubcommandList, /* 543 */
    Tcl_SetEnsembleMappingDict, /* 544 */
    Tcl_SetEnsembleUnknownHandler, /* 545 */
    Tcl_SetEnsembleFlags, /* 546 */
    Tcl_GetEnsembleSubcommandList, /* 547 */
    Tcl_GetEnsembleMappingDict, /* 548 */
    Tcl_GetEnsembleUnknownHandler, /* 549 */
    Tcl_GetEnsembleFlags, /* 550 */
    Tcl_GetEnsembleNamespace, /* 551 */
    Tcl_SetTimeProc, /* 552 */
    Tcl_QueryTimeProc, /* 553 */
    Tcl_ChannelThreadActionProc, /* 554 */
    Tcl_NewBignumObj, /* 555 */
    Tcl_DbNewBignumObj, /* 556 */
    Tcl_SetBignumObj, /* 557 */
    Tcl_GetBignumFromObj, /* 558 */
    Tcl_TakeBignumFromObj, /* 559 */
    Tcl_TruncateChannel, /* 560 */
    Tcl_ChannelTruncateProc, /* 561 */
    Tcl_SetChannelErrorInterp, /* 562 */
    Tcl_GetChannelErrorInterp, /* 563 */
    Tcl_SetChannelError, /* 564 */
    Tcl_GetChannelError, /* 565 */
    Tcl_InitBignumFromDouble, /* 566 */
    Tcl_GetNamespaceUnknownHandler, /* 567 */
    Tcl_SetNamespaceUnknownHandler, /* 568 */
    Tcl_GetEncodingFromObj, /* 569 */
    Tcl_GetEncodingSearchPath, /* 570 */
    Tcl_SetEncodingSearchPath, /* 571 */
    Tcl_GetEncodingNameFromEnvironment, /* 572 */
    Tcl_PkgRequireProc, /* 573 */
    Tcl_AppendObjToErrorInfo, /* 574 */
    Tcl_AppendLimitedToObj, /* 575 */
    Tcl_Format, /* 576 */
    Tcl_AppendFormatToObj, /* 577 */
    Tcl_ObjPrintf, /* 578 */
    Tcl_AppendPrintfToObj, /* 579 */
    Tcl_CancelEval, /* 580 */
    Tcl_Canceled, /* 581 */
    Tcl_CreatePipe, /* 582 */
    Tcl_NRCreateCommand, /* 583 */
    Tcl_NREvalObj, /* 584 */
    Tcl_NREvalObjv, /* 585 */
    Tcl_NRCmdSwap, /* 586 */
    Tcl_NRAddCallback, /* 587 */
    Tcl_NRCallObjProc, /* 588 */
    Tcl_GetFSDeviceFromStat, /* 589 */
    Tcl_GetFSInodeFromStat, /* 590 */
    Tcl_GetModeFromStat, /* 591 */
    Tcl_GetLinkCountFromStat, /* 592 */
    Tcl_GetUserIdFromStat, /* 593 */
    Tcl_GetGroupIdFromStat, /* 594 */
    Tcl_GetDeviceTypeFromStat, /* 595 */
    Tcl_GetAccessTimeFromStat, /* 596 */
    Tcl_GetModificationTimeFromStat, /* 597 */
    Tcl_GetChangeTimeFromStat, /* 598 */
    Tcl_GetSizeFromStat, /* 599 */
    Tcl_GetBlocksFromStat, /* 600 */
    Tcl_GetBlockSizeFromStat, /* 601 */
    Tcl_SetEnsembleParameterList, /* 602 */
    Tcl_GetEnsembleParameterList, /* 603 */
    Tcl_ParseArgsObjv, /* 604 */
    Tcl_GetErrorLine, /* 605 */
    Tcl_SetErrorLine, /* 606 */
    Tcl_TransferResult, /* 607 */
    Tcl_InterpActive, /* 608 */
    Tcl_BackgroundException, /* 609 */
    Tcl_ZlibDeflate, /* 610 */
    Tcl_ZlibInflate, /* 611 */
    Tcl_ZlibCRC32, /* 612 */
    Tcl_ZlibAdler32, /* 613 */
    Tcl_ZlibStreamInit, /* 614 */
    Tcl_ZlibStreamGetCommandName, /* 615 */
    Tcl_ZlibStreamEof, /* 616 */
    Tcl_ZlibStreamChecksum, /* 617 */
    Tcl_ZlibStreamPut, /* 618 */
    Tcl_ZlibStreamGet, /* 619 */
    Tcl_ZlibStreamClose, /* 620 */
    Tcl_ZlibStreamReset, /* 621 */
    Tcl_SetStartupScript, /* 622 */
    Tcl_GetStartupScript, /* 623 */
    Tcl_CloseEx, /* 624 */
    Tcl_NRExprObj, /* 625 */
    Tcl_NRSubstObj, /* 626 */
    Tcl_LoadFile, /* 627 */
    Tcl_FindSymbol, /* 628 */
    Tcl_FSUnloadFile, /* 629 */
    Tcl_ZlibStreamSetCompressionDictionary, /* 630 */
    Tcl_OpenTcpServerEx, /* 631 */
    TclZipfs_Mount, /* 632 */
    TclZipfs_Unmount, /* 633 */
    TclZipfs_TclLibrary, /* 634 */
    TclZipfs_MountBuffer, /* 635 */
    Tcl_FreeIntRep, /* 636 */
    Tcl_InitStringRep, /* 637 */
    Tcl_FetchIntRep, /* 638 */
    Tcl_StoreIntRep, /* 639 */
    Tcl_HasStringRep, /* 640 */
    Tcl_IncrRefCount, /* 641 */
    Tcl_DecrRefCount, /* 642 */
    Tcl_IsShared, /* 643 */
    Tcl_LinkArray, /* 644 */
    Tcl_GetIntForIndex, /* 645 */
    Tcl_UtfToUniChar, /* 646 */
    Tcl_UniCharToUtfDString, /* 647 */
    Tcl_UtfToUniCharDString, /* 648 */
};

/* !END!: Do not edit above this line. */
