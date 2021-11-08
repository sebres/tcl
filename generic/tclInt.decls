# tclInt.decls --
#
#	This file contains the declarations for all unsupported
#	functions that are exported by the Tcl library.  This file
#	is used to generate the tclIntDecls.h, tclIntPlatDecls.h
#	and tclStubInit.c files
#
# Copyright © 1998-1999 Scriptics Corporation.
# Copyright © 2001 Kevin B. Kenny.  All rights reserved.
# Copyright © 2007 Daniel A. Steffen <das@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

library tcl

# Define the unsupported generic interfaces.

interface tclInt
scspec EXTERN

# Declare each of the functions in the unsupported internal Tcl
# interface.  These interfaces are allowed to changed between versions.
# Use at your own risk.  Note that the position of functions should not
# be changed between versions to avoid gratuitous incompatibilities.

declare 3 {
    void TclAllocateFreeObjects(void)
}
declare 5 {
    int TclCleanupChildren(Tcl_Interp *interp, int numPids, Tcl_Pid *pidPtr,
	    Tcl_Channel errorChan)
}
declare 6 {
    void TclCleanupCommand(Command *cmdPtr)
}
declare 7 {
    size_t TclCopyAndCollapse(size_t count, const char *src, char *dst)
}
# TclCreatePipeline unofficially exported for use by BLT.
declare 9 {
    int TclCreatePipeline(Tcl_Interp *interp, int argc, const char **argv,
	    Tcl_Pid **pidArrayPtr, TclFile *inPipePtr, TclFile *outPipePtr,
	    TclFile *errFilePtr)
}
declare 10 {
    int TclCreateProc(Tcl_Interp *interp, Namespace *nsPtr,
	    const char *procName,
	    Tcl_Obj *argsPtr, Tcl_Obj *bodyPtr, Proc **procPtrPtr)
}
declare 11 {
    void TclDeleteCompiledLocalVars(Interp *iPtr, CallFrame *framePtr)
}
declare 12 {
    void TclDeleteVars(Interp *iPtr, TclVarHashTable *tablePtr)
}
declare 14 {
    int TclDumpMemoryInfo(void *clientData, int flags)
}
declare 16 {
    void TclExprFloatError(Tcl_Interp *interp, double value)
}
declare 22 {
    int TclFindElement(Tcl_Interp *interp, const char *listStr,
	    int listLength, const char **elementPtr, const char **nextPtr,
	    size_t *sizePtr, int *bracePtr)
}
declare 23 {
    Proc *TclFindProc(Interp *iPtr, const char *procName)
}
# Replaced with macro (see tclInt.h) in Tcl 8.5.0, restored in 8.5.10
declare 24 {
    size_t TclFormatInt(char *buffer, Tcl_WideInt n)
}
declare 25 {
    void TclFreePackageInfo(Interp *iPtr)
}
declare 28 {
    Tcl_Channel TclpGetDefaultStdChannel(int type)
}
declare 31 {
    const char *TclGetExtension(const char *name)
}
declare 32 {
    int TclGetFrame(Tcl_Interp *interp, const char *str,
	    CallFrame **framePtrPtr)
}
declare 38 {
    int TclGetNamespaceForQualName(Tcl_Interp *interp, const char *qualName,
	    Namespace *cxtNsPtr, int flags, Namespace **nsPtrPtr,
	    Namespace **altNsPtrPtr, Namespace **actualCxtPtrPtr,
	    const char **simpleNamePtr)
}
declare 39 {
    TclObjCmdProcType TclGetObjInterpProc(void)
}
declare 40 {
    int TclGetOpenMode(Tcl_Interp *interp, const char *str, int *seekFlagPtr)
}
declare 41 {
    Tcl_Command TclGetOriginalCommand(Tcl_Command command)
}
declare 42 {
    const char *TclpGetUserHome(const char *name, Tcl_DString *bufferPtr)
}
declare 45 {
    int TclHideUnsafeCommands(Tcl_Interp *interp)
}
declare 46 {
    int TclInExit(void)
}
declare 51 {
    int TclInterpInit(Tcl_Interp *interp)
}
declare 53 {
    int TclInvokeObjectCommand(void *clientData, Tcl_Interp *interp,
	    int argc, const char **argv)
}
declare 54 {
    int TclInvokeStringCommand(void *clientData, Tcl_Interp *interp,
	    int objc, Tcl_Obj *const objv[])
}
declare 55 {
    Proc *TclIsProc(Command *cmdPtr)
}
declare 58 {
    Var *TclLookupVar(Tcl_Interp *interp, const char *part1, const char *part2,
	    int flags, const char *msg, int createPart1, int createPart2,
	    Var **arrayPtrPtr)
}
declare 60 {
    int TclNeedSpace(const char *start, const char *end)
}
declare 61 {
    Tcl_Obj *TclNewProcBodyObj(Proc *procPtr)
}
declare 62 {
    int TclObjCommandComplete(Tcl_Obj *cmdPtr)
}
declare 63 {
    int TclObjInterpProc(void *clientData, Tcl_Interp *interp,
	    int objc, Tcl_Obj *const objv[])
}
declare 64 {
    int TclObjInvoke(Tcl_Interp *interp, int objc, Tcl_Obj *const objv[],
	    int flags)
}
declare 69 {
    void *TclpAlloc(size_t size)
}
declare 74 {
    void TclpFree(void *ptr)
}
declare 75 {
    unsigned long long TclpGetClicks(void)
}
declare 76 {
    unsigned long long TclpGetSeconds(void)
}
declare 81 {
    void *TclpRealloc(void *ptr, size_t size)
}
declare 89 {
    int TclPreventAliasLoop(Tcl_Interp *interp, Tcl_Interp *cmdInterp,
	    Tcl_Command cmd)
}
declare 91 {
    void TclProcCleanupProc(Proc *procPtr)
}
declare 92 {
    int TclProcCompileProc(Tcl_Interp *interp, Proc *procPtr,
	    Tcl_Obj *bodyPtr, Namespace *nsPtr, const char *description,
	    const char *procName)
}
declare 93 {
    void TclProcDeleteProc(void *clientData)
}
declare 96 {
    int TclRenameCommand(Tcl_Interp *interp, const char *oldName,
            const char *newName)
}
declare 97 {
    void TclResetShadowedCmdRefs(Tcl_Interp *interp, Command *newCmdPtr)
}
declare 98 {
    int TclServiceIdle(void)
}
# Removed in 9.0:
#declare 101 {
#    const char *TclSetPreInitScript(const char *string)
#}
declare 102 {
    void TclSetupEnv(Tcl_Interp *interp)
}
declare 103 {
    int TclSockGetPort(Tcl_Interp *interp, const char *str, const char *proto,
	    int *portPtr)
}
declare 108 {
    void TclTeardownNamespace(Namespace *nsPtr)
}
declare 109 {
    int TclUpdateReturnInfo(Interp *iPtr)
}
declare 110 {
    int TclSockMinimumBuffers(void *sock, int size)
}
# Removed in 8.1:
#  declare 110 {
#      char *TclWordEnd(char *start, char *lastChar, int nested, int *semiPtr)
#  }

# Procedures used in conjunction with Tcl namespaces. They are
# defined here instead of in tcl.decls since they are not stable yet.

declare 111 {
    void Tcl_AddInterpResolvers(Tcl_Interp *interp, const char *name,
	    Tcl_ResolveCmdProc *cmdProc, Tcl_ResolveVarProc *varProc,
	    Tcl_ResolveCompiledVarProc *compiledVarProc)
}
declare 118 {
    int Tcl_GetInterpResolvers(Tcl_Interp *interp, const char *name,
	    Tcl_ResolverInfo *resInfo)
}
declare 119 {
    int Tcl_GetNamespaceResolvers(Tcl_Namespace *namespacePtr,
	    Tcl_ResolverInfo *resInfo)
}
declare 120 {
    Tcl_Var Tcl_FindNamespaceVar(Tcl_Interp *interp, const char *name,
	    Tcl_Namespace *contextNsPtr, int flags)
}
declare 126 {
    void Tcl_GetVariableFullName(Tcl_Interp *interp, Tcl_Var variable,
	    Tcl_Obj *objPtr)
}
declare 128 {
    void Tcl_PopCallFrame(Tcl_Interp *interp)
}
declare 129 {
    int Tcl_PushCallFrame(Tcl_Interp *interp, Tcl_CallFrame *framePtr,
	    Tcl_Namespace *nsPtr, int isProcCallFrame)
}
declare 130 {
    int Tcl_RemoveInterpResolvers(Tcl_Interp *interp, const char *name)
}
declare 131 {
    void Tcl_SetNamespaceResolvers(Tcl_Namespace *namespacePtr,
	    Tcl_ResolveCmdProc *cmdProc, Tcl_ResolveVarProc *varProc,
	    Tcl_ResolveCompiledVarProc *compiledVarProc)
}
declare 132 {
    int TclpHasSockets(Tcl_Interp *interp)
}
declare 138 {
    const char *TclGetEnv(const char *name, Tcl_DString *valuePtr)
}
# This is used by TclX, but should otherwise be considered private
declare 141 {
    const char *TclpGetCwd(Tcl_Interp *interp, Tcl_DString *cwdPtr)
}
declare 142 {
    int TclSetByteCodeFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    CompileHookProc *hookProc, void *clientData)
}
declare 143 {
    int TclAddLiteralObj(struct CompileEnv *envPtr, Tcl_Obj *objPtr,
	    LiteralEntry **litPtrPtr)
}
declare 144 {
    void TclHideLiteral(Tcl_Interp *interp, struct CompileEnv *envPtr,
	    int index)
}
declare 145 {
    const struct AuxDataType *TclGetAuxDataType(const char *typeName)
}
declare 146 {
    TclHandle TclHandleCreate(void *ptr)
}
declare 147 {
    void TclHandleFree(TclHandle handle)
}
declare 148 {
    TclHandle TclHandlePreserve(TclHandle handle)
}
declare 149 {
    void TclHandleRelease(TclHandle handle)
}
declare 150 {
    int TclRegAbout(Tcl_Interp *interp, Tcl_RegExp re)
}
declare 151 {
    void TclRegExpRangeUniChar(Tcl_RegExp re, size_t index, size_t *startPtr,
	    size_t *endPtr)
}
declare 152 {
    void TclSetLibraryPath(Tcl_Obj *pathPtr)
}
declare 153 {
    Tcl_Obj *TclGetLibraryPath(void)
}
declare 156 {
    void TclRegError(Tcl_Interp *interp, const char *msg,
	    int status)
}
declare 157 {
    Var *TclVarTraceExists(Tcl_Interp *interp, const char *varName)
}
declare 161 {
    int TclChannelTransform(Tcl_Interp *interp, Tcl_Channel chan,
	    Tcl_Obj *cmdObjPtr)
}
declare 162 {
    void TclChannelEventScriptInvoker(void *clientData, int flags)
}

# ALERT: The result of 'TclGetInstructionTable' is actually a
# "const InstructionDesc*" but we do not want to describe this structure in
# "tclInt.h". It is described in "tclCompile.h". Use a cast to the
# correct type when calling this procedure.

declare 163 {
    const void *TclGetInstructionTable(void)
}

# ALERT: The argument of 'TclExpandCodeArray' is actually a
# "CompileEnv*" but we do not want to describe this structure in
# "tclInt.h". It is described in "tclCompile.h".

declare 164 {
    void TclExpandCodeArray(void *envPtr)
}

# These functions are vfs aware, but are generally only useful internally.
declare 165 {
    void TclpSetInitialEncodings(void)
}

# New function due to TIP #33
declare 166 {
    int TclListObjSetElement(Tcl_Interp *interp, Tcl_Obj *listPtr,
	    int index, Tcl_Obj *valuePtr)
}

# variant of Tcl_UtfNCmp that takes n as bytes, not chars
declare 169 {
    int TclpUtfNcmp2(const char *s1, const char *s2, size_t n)
}
declare 170 {
    int TclCheckInterpTraces(Tcl_Interp *interp, const char *command,
	    size_t numChars, Command *cmdPtr, int result, int traceFlags,
	    int objc, Tcl_Obj *const objv[])
}
declare 171 {
    int TclCheckExecutionTraces(Tcl_Interp *interp, const char *command,
	    size_t numChars, Command *cmdPtr, int result, int traceFlags,
	    int objc, Tcl_Obj *const objv[])
}
declare 172 {
    int TclInThreadExit(void)
}
declare 173 {
    int TclUniCharMatch(const Tcl_UniChar *string, size_t strLen,
	    const Tcl_UniChar *pattern, size_t ptnLen, int flags)
}
declare 175 {
    int TclCallVarTraces(Interp *iPtr, Var *arrayPtr, Var *varPtr,
	    const char *part1, const char *part2, int flags, int leaveErrMsg)
}
declare 176 {
    void TclCleanupVar(Var *varPtr, Var *arrayPtr)
}
declare 177 {
    void TclVarErrMsg(Tcl_Interp *interp, const char *part1, const char *part2,
	    const char *operation, const char *reason)
}
declare 198 {
    int TclObjGetFrame(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    CallFrame **framePtrPtr)
}
# 200-208 exported for use by the test suite [Bug 1054748]
declare 200 {
    int TclpObjRemoveDirectory(Tcl_Obj *pathPtr, int recursive,
	Tcl_Obj **errorPtr)
}
declare 201 {
    int TclpObjCopyDirectory(Tcl_Obj *srcPathPtr, Tcl_Obj *destPathPtr,
	Tcl_Obj **errorPtr)
}
declare 202 {
    int TclpObjCreateDirectory(Tcl_Obj *pathPtr)
}
declare 203 {
    int TclpObjDeleteFile(Tcl_Obj *pathPtr)
}
declare 204 {
    int TclpObjCopyFile(Tcl_Obj *srcPathPtr, Tcl_Obj *destPathPtr)
}
declare 205 {
    int TclpObjRenameFile(Tcl_Obj *srcPathPtr, Tcl_Obj *destPathPtr)
}
declare 206 {
    int TclpObjStat(Tcl_Obj *pathPtr, Tcl_StatBuf *buf)
}
declare 207 {
    int TclpObjAccess(Tcl_Obj *pathPtr, int mode)
}
declare 208 {
    Tcl_Channel TclpOpenFileChannel(Tcl_Interp *interp,
	    Tcl_Obj *pathPtr, int mode, int permissions)
}
declare 212 {
    void TclpFindExecutable(const char *argv0)
}
declare 213 {
    Tcl_Obj *TclGetObjNameOfExecutable(void)
}
declare 214 {
    void TclSetObjNameOfExecutable(Tcl_Obj *name, Tcl_Encoding encoding)
}
declare 215 {
    void *TclStackAlloc(Tcl_Interp *interp, size_t numBytes)
}
declare 216 {
    void TclStackFree(Tcl_Interp *interp, void *freePtr)
}
declare 217 {
    int TclPushStackFrame(Tcl_Interp *interp, Tcl_CallFrame **framePtrPtr,
            Tcl_Namespace *namespacePtr, int isProcCallFrame)
}
declare 218 {
    void TclPopStackFrame(Tcl_Interp *interp)
}

# for use in tclTest.c
declare 224 {
    TclPlatformType *TclGetPlatform(void)
}
declare 225 {
    Tcl_Obj *TclTraceDictPath(Tcl_Interp *interp, Tcl_Obj *rootPtr,
	    int keyc, Tcl_Obj *const keyv[], int flags)
}
declare 226 {
    int TclObjBeingDeleted(Tcl_Obj *objPtr)
}
declare 227 {
    void TclSetNsPath(Namespace *nsPtr, size_t pathLength,
            Tcl_Namespace *pathAry[])
}
declare 229 {
    int	TclPtrMakeUpvar(Tcl_Interp *interp, Var *otherP1Ptr,
	    const char *myName, int myFlags, int index)
}
declare 230 {
    Var *TclObjLookupVar(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
	    const char *part2, int flags, const char *msg,
	    const int createPart1, const int createPart2, Var **arrayPtrPtr)
}
declare 231 {
    int	TclGetNamespaceFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    Tcl_Namespace **nsPtrPtr)
}

# Bits and pieces of TIP#280's guts
declare 232 {
    int TclEvalObjEx(Tcl_Interp *interp, Tcl_Obj *objPtr, int flags,
	    const CmdFrame *invoker, int word)
}
declare 233 {
    void TclGetSrcInfoForPc(CmdFrame *contextPtr)
}

# Exports for VarReform compat: Itcl, XOTcl like to peek into our varTables :(
declare 234 {
    Var *TclVarHashCreateVar(TclVarHashTable *tablePtr, const char *key,
             int *newPtr)
}
declare 235 {
    void TclInitVarHashTable(TclVarHashTable *tablePtr, Namespace *nsPtr)
}
# TIP 542
declare 236 {
    void TclAppendUnicodeToObj(Tcl_Obj *objPtr,
	    const Tcl_UniChar *unicode, size_t length)
}

# TIP #285: Script cancellation support.
declare 237 {
    int TclResetCancellation(Tcl_Interp *interp, int force)
}

# NRE functions for "rogue" extensions to exploit NRE; they will need to
# include NRE.h too.
declare 238 {
    int TclNRInterpProc(void *clientData, Tcl_Interp *interp,
	    int objc, Tcl_Obj *const objv[])
}
declare 239 {
    int TclNRInterpProcCore(Tcl_Interp *interp, Tcl_Obj *procNameObj,
			    int skip, ProcErrorProc *errorProc)
}
declare 240 {
    int TclNRRunCallbacks(Tcl_Interp *interp, int result,
	      struct NRE_callback *rootPtr)
}
declare 241 {
    int TclNREvalObjEx(Tcl_Interp *interp, Tcl_Obj *objPtr, int flags,
	    const CmdFrame *invoker, int word)
}
declare 242 {
    int TclNREvalObjv(Tcl_Interp *interp, int objc,
	      Tcl_Obj *const objv[], int flags, Command *cmdPtr)
}

# Tcl_Obj leak detection support.
declare 243 {
    void TclDbDumpActiveObjects(FILE *outFile)
}

# Functions to make things better for itcl
declare 244 {
    Tcl_HashTable *TclGetNamespaceChildTable(Tcl_Namespace *nsPtr)
}
declare 245 {
    Tcl_HashTable *TclGetNamespaceCommandTable(Tcl_Namespace *nsPtr)
}
declare 246 {
    int TclInitRewriteEnsemble(Tcl_Interp *interp, size_t numRemoved,
	    size_t numInserted, Tcl_Obj *const *objv)
}
declare 247 {
    void TclResetRewriteEnsemble(Tcl_Interp *interp, int isRootEnsemble)
}

declare 248 {
    int TclCopyChannel(Tcl_Interp *interp, Tcl_Channel inChan,
	    Tcl_Channel outChan, long long toRead, Tcl_Obj *cmdPtr)
}

declare 249 {
    char *TclDoubleDigits(double dv, int ndigits, int flags,
			  int *decpt, int *signum, char **endPtr)
}
# TIP #285: Script cancellation support.
declare 250 {
    void TclSetChildCancelFlags(Tcl_Interp *interp, int flags, int force)
}

# Allow extensions for optimization
declare 251 {
    int TclRegisterLiteral(void *envPtr,
	    const char *bytes, size_t length, int flags)
}

# Exporting of the internal API to variables.

declare 252 {
    Tcl_Obj *TclPtrGetVar(Tcl_Interp *interp, Tcl_Var varPtr,
	    Tcl_Var arrayPtr, Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr,
	    const int flags)
}
declare 253 {
    Tcl_Obj *TclPtrSetVar(Tcl_Interp *interp, Tcl_Var varPtr,
	    Tcl_Var arrayPtr, Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr,
	    Tcl_Obj *newValuePtr, const int flags)
}
declare 254 {
    Tcl_Obj *TclPtrIncrObjVar(Tcl_Interp *interp, Tcl_Var varPtr,
	    Tcl_Var arrayPtr, Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr,
	    Tcl_Obj *incrPtr, const int flags)
}
declare 255 {
    int	TclPtrObjMakeUpvar(Tcl_Interp *interp, Tcl_Var otherPtr,
	    Tcl_Obj *myNamePtr, int myFlags)
}
declare 256 {
    int	TclPtrUnsetVar(Tcl_Interp *interp, Tcl_Var varPtr, Tcl_Var arrayPtr,
	    Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr, const int flags)
}
declare 257 {
    void TclStaticLibrary(Tcl_Interp *interp, const char *prefix,
	    Tcl_LibraryInitProc *initProc, Tcl_LibraryInitProc *safeInitProc)
}

# TIP 431: temporary directory creation function
declare 258 {
    Tcl_Obj *TclpCreateTemporaryDirectory(Tcl_Obj *dirObj,
	    Tcl_Obj *basenameObj)
}
declare 261 {
    int	TclMSB(size_t n)
}

##############################################################################

# Define the platform specific internal Tcl interface. These functions are
# only available on the designated platform.

interface tclIntPlat

################################
# Platform specific functions

# Removed in 9.0
#declare 0 {unix win} {
#    void TclWinConvertError(unsigned errCode)
#}
declare 1 {
    int TclpCloseFile(TclFile file)
}
declare 2 {
    Tcl_Channel TclpCreateCommandChannel(TclFile readFile,
	    TclFile writeFile, TclFile errorFile, int numPids, Tcl_Pid *pidPtr)
}
declare 3 {
    int TclpCreatePipe(TclFile *readPipe, TclFile *writePipe)
}
declare 4 {
    void *TclWinGetTclInstance(void)
}
declare 5 {
    int TclUnixWaitForFile(int fd, int mask, int timeout)
}
declare 6 {
    TclFile TclpMakeFile(Tcl_Channel channel, int direction)
}
declare 7 {
    TclFile TclpOpenFile(const char *fname, int mode)
}
declare 8 {
    size_t TclpGetPid(Tcl_Pid pid)
}
declare 9 {
    TclFile TclpCreateTempFile(const char *contents)
}
declare 11 {
    void TclGetAndDetachPids(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 15 {
    int TclpCreateProcess(Tcl_Interp *interp, int argc,
	    const char **argv, TclFile inputFile, TclFile outputFile,
	    TclFile errorFile, Tcl_Pid *pidPtr)
}
declare 16 {
    int TclpIsAtty(int fd)
}
declare 17 {
    int TclUnixCopyFile(const char *src, const char *dst,
	    const Tcl_StatBuf *statBufPtr, int dontCopyAtts)
}
declare 20 {
    void TclWinAddProcess(void *hProcess, size_t id)
}
declare 24 {
    char *TclWinNoBackslash(char *path)
}
declare 27 {
    void TclWinFlushDirtyChannels(void)
}
declare 29 {
    int TclWinCPUID(int index, int *regs)
}
declare 30 {
    int TclUnixOpenTemporaryFile(Tcl_Obj *dirObj, Tcl_Obj *basenameObj,
	    Tcl_Obj *extensionObj, Tcl_Obj *resultingNameObj)
}

# Local Variables:
# mode: tcl
# End:
