/*
 * tclLoad.c --
 *
 *	This file provides the generic portion (those that are the same on all
 *	platforms) of Tcl's dynamic loading facilities.
 *
 * Copyright © 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * The following structure describes a library that has been loaded either
 * dynamically (with the "load" command) or statically (as indicated by a call
 * to Tcl_StaticPackage). All such libraries are linked together into a
 * single list for the process. Library are never unloaded, until the
 * application exits, when TclFinalizeLoad is called, and these structures are
 * freed.
 */

typedef struct LoadedPackage {
    char *fileName;		/* Name of the file from which the library was
				 * loaded. An empty string means the library
				 * is loaded statically. Malloc-ed. */
    char *prefix;		/* Prefix for the library,
				 * properly capitalized (first letter UC,
				 * others LC), no "_", as in "Net".
				 * Malloc-ed. */
    Tcl_LoadHandle loadHandle;	/* Token for the loaded file which should be
				 * passed to (*unLoadProcPtr)() when the file
				 * is no longer needed. If fileName is NULL,
				 * then this field is irrelevant. */
    Tcl_PackageInitProc *initProc;
				/* Initialization function to call to
				 * incorporate this library into a trusted
				 * interpreter. */
    Tcl_PackageInitProc *safeInitProc;
				/* Initialization function to call to
				 * incorporate this library into a safe
				 * interpreter (one that will execute
				 * untrusted scripts). NULL means the library
				 * can't be used in unsafe interpreters. */
    Tcl_PackageUnloadProc *unloadProc;
				/* Finalization function to unload a library
				 * from a trusted interpreter. NULL means that
				 * the library cannot be unloaded. */
    Tcl_PackageUnloadProc *safeUnloadProc;
				/* Finalization function to unload a library
				 * from a safe interpreter. NULL means that
				 * the library cannot be unloaded. */
    int interpRefCount;		/* How many times the library has been loaded
				 * in trusted interpreters. */
    int safeInterpRefCount;	/* How many times the library has been loaded
				 * in safe interpreters. */
    struct LoadedPackage *nextPtr;
				/* Next in list of all libraries loaded into
				 * this application process. NULL means end of
				 * list. */
} LoadedPackage;

/*
 * TCL_THREADS
 * There is a global list of libraries that is anchored at firstPackagePtr.
 * Access to this list is governed by a mutex.
 */

static LoadedPackage *firstPackagePtr = NULL;
				/* First in list of all libraries loaded into
				 * this process. */

TCL_DECLARE_MUTEX(packageMutex)

/*
 * The following structure represents a particular library that has been
 * incorporated into a particular interpreter (by calling its initialization
 * function). There is a list of these structures for each interpreter, with
 * an AssocData value (key "load") for the interpreter that points to the
 * first library (if any).
 */

typedef struct InterpPackage {
    LoadedPackage *pkgPtr;	/* Points to detailed information about
				 * library. */
    struct InterpPackage *nextPtr;
				/* Next library in this interpreter, or NULL
				 * for end of list. */
} InterpPackage;

/*
 * Prototypes for functions that are private to this file:
 */

static void		LoadCleanupProc(ClientData clientData,
			    Tcl_Interp *interp);

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LoadObjCmd --
 *
 *	This function is invoked to process the "load" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LoadObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Interp *target;
    LoadedPackage *pkgPtr, *defaultPtr;
    Tcl_DString pfx, tmp, initName, safeInitName;
    Tcl_DString unloadName, safeUnloadName;
    InterpPackage *ipFirstPtr, *ipPtr;
    int code, namesMatch, filesMatch, offset;
    const char *symbols[2];
    Tcl_PackageInitProc *initProc;
    const char *p, *fullFileName, *prefix;
    Tcl_LoadHandle loadHandle;
    Tcl_UniChar ch = 0;
    unsigned len;
    int index, flags = 0;
    Tcl_Obj *const *savedobjv = objv;
    static const char *const options[] = {
	"-global",		"-lazy",		"--",	NULL
    };
    enum loadOptionsEnum {
	LOAD_GLOBAL,	LOAD_LAZY,	LOAD_LAST
    };

    while (objc > 2) {
	if (TclGetString(objv[1])[0] != '-') {
	    break;
	}
	if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	++objv; --objc;
	if (LOAD_GLOBAL == (enum loadOptionsEnum) index) {
	    flags |= TCL_LOAD_GLOBAL;
	} else if (LOAD_LAZY == (enum loadOptionsEnum) index) {
	    flags |= TCL_LOAD_LAZY;
	} else {
		break;
	}
    }
    if ((objc < 2) || (objc > 4)) {
	Tcl_WrongNumArgs(interp, 1, savedobjv, "?-global? ?-lazy? ?--? fileName ?prefix? ?interp?");
	return TCL_ERROR;
    }
    if (Tcl_FSConvertToPathType(interp, objv[1]) != TCL_OK) {
	return TCL_ERROR;
    }
    fullFileName = Tcl_GetString(objv[1]);

    Tcl_DStringInit(&pfx);
    Tcl_DStringInit(&initName);
    Tcl_DStringInit(&safeInitName);
    Tcl_DStringInit(&unloadName);
    Tcl_DStringInit(&safeUnloadName);
    Tcl_DStringInit(&tmp);

    prefix = NULL;
    if (objc >= 3) {
	prefix = Tcl_GetString(objv[2]);
	if (prefix[0] == '\0') {
	    prefix = NULL;
	}
    }
    if ((fullFileName[0] == 0) && (prefix == NULL)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"must specify either file name or prefix", -1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD", "NOLIBRARY",
		NULL);
	code = TCL_ERROR;
	goto done;
    }

    /*
     * Figure out which interpreter we're going to load the library into.
     */

    target = interp;
    if (objc == 4) {
	const char *childIntName = Tcl_GetString(objv[3]);

	target = Tcl_GetChild(interp, childIntName);
	if (target == NULL) {
	    code = TCL_ERROR;
	    goto done;
	}
    }

    /*
     * Scan through the libraries that are currently loaded to see if the
     * library we want is already loaded. We'll use a loaded library if it
     * meets any of the following conditions:
     *  - Its name and file match the once we're looking for.
     *  - Its file matches, and we weren't given a name.
     *  - Its name matches, the file name was specified as empty, and there is
     *	  only no statically loaded library with the same prefix.
     */

    Tcl_MutexLock(&packageMutex);

    defaultPtr = NULL;
    for (pkgPtr = firstPackagePtr; pkgPtr != NULL; pkgPtr = pkgPtr->nextPtr) {
	if (prefix == NULL) {
	    namesMatch = 0;
	} else {
	    TclDStringClear(&pfx);
	    Tcl_DStringAppend(&pfx, prefix, -1);
	    TclDStringClear(&tmp);
	    Tcl_DStringAppend(&tmp, pkgPtr->prefix, -1);
	    Tcl_UtfToLower(Tcl_DStringValue(&pfx));
	    Tcl_UtfToLower(Tcl_DStringValue(&tmp));
	    if (strcmp(Tcl_DStringValue(&tmp),
		    Tcl_DStringValue(&pfx)) == 0) {
		namesMatch = 1;
	    } else {
		namesMatch = 0;
	    }
	}
	TclDStringClear(&pfx);

	filesMatch = (strcmp(pkgPtr->fileName, fullFileName) == 0);
	if (filesMatch && (namesMatch || (prefix == NULL))) {
	    break;
	}
	if (namesMatch && (fullFileName[0] == 0)) {
	    defaultPtr = pkgPtr;
	}
	if (filesMatch && !namesMatch && (fullFileName[0] != 0)) {
	    /*
	     * Can't have two different libraries loaded from the same file.
	     */

	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "file \"%s\" is already loaded for prefix \"%s\"",
		    fullFileName, pkgPtr->prefix));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD",
		    "SPLITPERSONALITY", NULL);
	    code = TCL_ERROR;
	    Tcl_MutexUnlock(&packageMutex);
	    goto done;
	}
    }
    Tcl_MutexUnlock(&packageMutex);
    if (pkgPtr == NULL) {
	pkgPtr = defaultPtr;
    }

    /*
     * Scan through the list of libraries already loaded in the target
     * interpreter. If the library we want is already loaded there, then
     * there's nothing for us to do.
     */

    if (pkgPtr != NULL) {
	ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);
	for (ipPtr = ipFirstPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->pkgPtr == pkgPtr) {
		code = TCL_OK;
		goto done;
	    }
	}
    }

    if (pkgPtr == NULL) {
	/*
	 * The desired file isn't currently loaded, so load it. It's an error
	 * if the desired library is a static one.
	 */

	if (fullFileName[0] == 0) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "no library with prefix \"%s\" is loaded statically", prefix));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD", "NOTSTATIC",
		    NULL);
	    code = TCL_ERROR;
	    goto done;
	}

	/*
	 * Figure out the prefix if it wasn't provided explicitly.
	 */

	if (prefix != NULL) {
	    Tcl_DStringAppend(&pfx, prefix, -1);
	} else {
	    Tcl_Obj *splitPtr, *pkgGuessPtr;
	    int pElements;
	    const char *pkgGuess;

	    /*
	     * Threading note - this call used to be protected by a mutex.
	     */

	    /*
	     * The platform-specific code couldn't figure out the prefix.
	     * Make a guess by taking the last element of the file
	     * name, stripping off any leading "lib" and/or "tcl", and
	     * then using all of the alphabetic and underline characters
	     * that follow that.
	     */

	    splitPtr = Tcl_FSSplitPath(objv[1], &pElements);
	    Tcl_ListObjIndex(NULL, splitPtr, pElements -1, &pkgGuessPtr);
	    pkgGuess = Tcl_GetString(pkgGuessPtr);
	    if ((pkgGuess[0] == 'l') && (pkgGuess[1] == 'i')
		    && (pkgGuess[2] == 'b')) {
		pkgGuess += 3;
	    }
#ifdef __CYGWIN__
	    else if ((pkgGuess[0] == 'c') && (pkgGuess[1] == 'y')
		    && (pkgGuess[2] == 'g')) {
		pkgGuess += 3;
	    }
#endif /* __CYGWIN__ */
	    if ((pkgGuess[0] == 't') && (pkgGuess[1] == 'c')
		    && (pkgGuess[2] == 'l')) {
		pkgGuess += 3;
	    }
	    for (p = pkgGuess; *p != 0; p += offset) {
		offset = TclUtfToUniChar(p, &ch);
		if ((ch > 0x100)
			|| !(isalpha(UCHAR(ch)) /* INTL: ISO only */
				|| (UCHAR(ch) == '_'))) {
		    break;
		}
	    }
	    if (p == pkgGuess) {
		Tcl_DecrRefCount(splitPtr);
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"couldn't figure out prefix for %s",
			fullFileName));
		Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD",
			"WHATLIBRARY", NULL);
		code = TCL_ERROR;
		goto done;
	    }
	    Tcl_DStringAppend(&pfx, pkgGuess, p - pkgGuess);
	    Tcl_DecrRefCount(splitPtr);
	}

	/*
	 * Fix the capitalization in the prefix so that the first
	 * character is in caps (or title case) but the others are all
	 * lower-case.
	 */

	Tcl_DStringSetLength(&pfx,
		Tcl_UtfToTitle(Tcl_DStringValue(&pfx)));

	/*
	 * Compute the names of the two initialization functions, based on the
	 * prefix.
	 */

	TclDStringAppendDString(&initName, &pfx);
	TclDStringAppendLiteral(&initName, "_Init");
	TclDStringAppendDString(&safeInitName, &pfx);
	TclDStringAppendLiteral(&safeInitName, "_SafeInit");
	TclDStringAppendDString(&unloadName, &pfx);
	TclDStringAppendLiteral(&unloadName, "_Unload");
	TclDStringAppendDString(&safeUnloadName, &pfx);
	TclDStringAppendLiteral(&safeUnloadName, "_SafeUnload");

	/*
	 * Call platform-specific code to load the library and find the two
	 * initialization functions.
	 */

	symbols[0] = Tcl_DStringValue(&initName);
	symbols[1] = NULL;

	Tcl_MutexLock(&packageMutex);
	code = Tcl_LoadFile(interp, objv[1], symbols, flags, &initProc,
		&loadHandle);
	Tcl_MutexUnlock(&packageMutex);
	if (code != TCL_OK) {
	    goto done;
	}

	/*
	 * Create a new record to describe this library.
	 */

	pkgPtr = (LoadedPackage *)ckalloc(sizeof(LoadedPackage));
	len = strlen(fullFileName) + 1;
	pkgPtr->fileName	   = (char *)ckalloc(len);
	memcpy(pkgPtr->fileName, fullFileName, len);
	len = Tcl_DStringLength(&pfx) + 1;
	pkgPtr->prefix	   = (char *)ckalloc(len);
	memcpy(pkgPtr->prefix, Tcl_DStringValue(&pfx), len);
	pkgPtr->loadHandle	   = loadHandle;
	pkgPtr->initProc	   = initProc;
	pkgPtr->safeInitProc	   = (Tcl_PackageInitProc *)
		Tcl_FindSymbol(interp, loadHandle,
			Tcl_DStringValue(&safeInitName));
	pkgPtr->unloadProc	   = (Tcl_PackageUnloadProc *)
		Tcl_FindSymbol(interp, loadHandle,
			Tcl_DStringValue(&unloadName));
	pkgPtr->safeUnloadProc	   = (Tcl_PackageUnloadProc *)
		Tcl_FindSymbol(interp, loadHandle,
			Tcl_DStringValue(&safeUnloadName));
	pkgPtr->interpRefCount	   = 0;
	pkgPtr->safeInterpRefCount = 0;

	Tcl_MutexLock(&packageMutex);
	pkgPtr->nextPtr		   = firstPackagePtr;
	firstPackagePtr		   = pkgPtr;
	Tcl_MutexUnlock(&packageMutex);

	/*
	 * The Tcl_FindSymbol calls may have left a spurious error message in
	 * the interpreter result.
	 */

	Tcl_ResetResult(interp);
    }

    /*
     * Invoke the library's initialization function (either the normal one or
     * the safe one, depending on whether or not the interpreter is safe).
     */

    if (Tcl_IsSafe(target)) {
	if (pkgPtr->safeInitProc == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "can't use library in a safe interpreter: no"
		    " %s_SafeInit procedure", pkgPtr->prefix));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD", "UNSAFE",
		    NULL);
	    code = TCL_ERROR;
	    goto done;
	}
	code = pkgPtr->safeInitProc(target);
    } else {
	if (pkgPtr->initProc == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "can't attach library to interpreter: no %s_Init procedure",
		    pkgPtr->prefix));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD", "ENTRYPOINT",
		    NULL);
	    code = TCL_ERROR;
	    goto done;
	}
	code = pkgPtr->initProc(target);
    }

    /*
     * Test for whether the initialization failed. If so, transfer the error
     * from the target interpreter to the originating one.
     */

    if (code != TCL_OK) {
#if defined(TCL_NO_DEPRECATED) || TCL_MAJOR_VERSION > 8
	Interp *iPtr = (Interp *) target;
	if (iPtr->result && *(iPtr->result) && !iPtr->freeProc) {
	    /*
	     * A call to Tcl_InitStubs() determined the caller extension and
	     * this interp are incompatible in their stubs mechanisms, and
	     * recorded the error in the oldest legacy place we have to do so.
	     */
	    Tcl_SetObjResult(target, Tcl_NewStringObj(iPtr->result, -1));
	    iPtr->result =  &tclEmptyString;
	    iPtr->freeProc = NULL;
	}
#endif /* defined(TCL_NO_DEPRECATED) */
	Tcl_TransferResult(target, code, interp);
	goto done;
    }

    /*
     * Record the fact that the library has been loaded in the target
     * interpreter.
     *
     * Update the proper reference count.
     */

    Tcl_MutexLock(&packageMutex);
    if (Tcl_IsSafe(target)) {
	pkgPtr->safeInterpRefCount++;
    } else {
	pkgPtr->interpRefCount++;
    }
    Tcl_MutexUnlock(&packageMutex);

    /*
     * Refetch ipFirstPtr: loading the library may have introduced additional
     * static libraries at the head of the linked list!
     */

    ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);
    ipPtr = (InterpPackage *)ckalloc(sizeof(InterpPackage));
    ipPtr->pkgPtr = pkgPtr;
    ipPtr->nextPtr = ipFirstPtr;
    Tcl_SetAssocData(target, "tclLoad", LoadCleanupProc, ipPtr);

  done:
    Tcl_DStringFree(&pfx);
    Tcl_DStringFree(&initName);
    Tcl_DStringFree(&safeInitName);
    Tcl_DStringFree(&unloadName);
    Tcl_DStringFree(&safeUnloadName);
    Tcl_DStringFree(&tmp);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnloadObjCmd --
 *
 *	This function is invoked to process the "unload" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UnloadObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Interp *target;		/* Which interpreter to unload from. */
    LoadedPackage *pkgPtr, *defaultPtr;
    Tcl_DString pfx, tmp;
    Tcl_PackageUnloadProc *unloadProc;
    InterpPackage *ipFirstPtr, *ipPtr;
    int i, index, code, complain = 1, keepLibrary = 0;
    int trustedRefCount = -1, safeRefCount = -1;
    const char *fullFileName = "";
    const char *prefix;
    static const char *const options[] = {
	"-nocomplain", "-keeplibrary", "--", NULL
    };
    enum unloadOptionsEnum {
	UNLOAD_NOCOMPLAIN, UNLOAD_KEEPLIB, UNLOAD_LAST
    };

    for (i = 1; i < objc; i++) {
	if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0,
		&index) != TCL_OK) {
	    fullFileName = Tcl_GetString(objv[i]);
	    if (fullFileName[0] == '-') {
		/*
		 * It looks like the command contains an option so signal an
		 * error
		 */

		return TCL_ERROR;
	    } else {
		/*
		 * This clearly isn't an option; assume it's the filename. We
		 * must clear the error.
		 */

		Tcl_ResetResult(interp);
		break;
	    }
	}
	switch ((enum unloadOptionsEnum)index) {
	case UNLOAD_NOCOMPLAIN:		/* -nocomplain */
	    complain = 0;
	    break;
	case UNLOAD_KEEPLIB:		/* -keeplibrary */
	    keepLibrary = 1;
	    break;
	case UNLOAD_LAST:		/* -- */
	    i++;
	    goto endOfForLoop;
	}
    }
  endOfForLoop:
    if ((objc-i < 1) || (objc-i > 3)) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"?-switch ...? fileName ?prefix? ?interp?");
	return TCL_ERROR;
    }
    if (Tcl_FSConvertToPathType(interp, objv[i]) != TCL_OK) {
	return TCL_ERROR;
    }

    fullFileName = Tcl_GetString(objv[i]);
    Tcl_DStringInit(&pfx);
    Tcl_DStringInit(&tmp);

    prefix = NULL;
    if (objc - i >= 2) {
	prefix = Tcl_GetString(objv[i+1]);
	if (prefix[0] == '\0') {
	    prefix = NULL;
	}
    }
    if ((fullFileName[0] == 0) && (prefix == NULL)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"must specify either file name or prefix", -1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "NOLIBRARY",
		NULL);
	code = TCL_ERROR;
	goto done;
    }

    /*
     * Figure out which interpreter we're going to load the library into.
     */

    target = interp;
    if (objc - i == 3) {
	const char *childIntName = Tcl_GetString(objv[i + 2]);

	target = Tcl_GetChild(interp, childIntName);
	if (target == NULL) {
	    return TCL_ERROR;
	}
    }

    /*
     * Scan through the libraries that are currently loaded to see if the
     * library we want is already loaded. We'll use a loaded library if it
     * meets any of the following conditions:
     *  - Its prefix and file match the once we're looking for.
     *  - Its file matches, and we weren't given a prefix.
     *  - Its prefix matches, the file name was specified as empty, and there is
     *	  only no statically loaded library with the same prefix.
     */

    Tcl_MutexLock(&packageMutex);

    defaultPtr = NULL;
    for (pkgPtr = firstPackagePtr; pkgPtr != NULL; pkgPtr = pkgPtr->nextPtr) {
	int namesMatch, filesMatch;

	if (prefix == NULL) {
	    namesMatch = 0;
	} else {
	    TclDStringClear(&pfx);
	    Tcl_DStringAppend(&pfx, prefix, -1);
	    TclDStringClear(&tmp);
	    Tcl_DStringAppend(&tmp, pkgPtr->prefix, -1);
	    Tcl_UtfToLower(Tcl_DStringValue(&pfx));
	    Tcl_UtfToLower(Tcl_DStringValue(&tmp));
	    if (strcmp(Tcl_DStringValue(&tmp),
		    Tcl_DStringValue(&pfx)) == 0) {
		namesMatch = 1;
	    } else {
		namesMatch = 0;
	    }
	}
	TclDStringClear(&pfx);

	filesMatch = (strcmp(pkgPtr->fileName, fullFileName) == 0);
	if (filesMatch && (namesMatch || (prefix == NULL))) {
	    break;
	}
	if (namesMatch && (fullFileName[0] == 0)) {
	    defaultPtr = pkgPtr;
	}
	if (filesMatch && !namesMatch && (fullFileName[0] != 0)) {
	    break;
	}
    }
    Tcl_MutexUnlock(&packageMutex);
    if (fullFileName[0] == 0) {
	/*
	 * It's an error to try unload a static library.
	 */

	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"library with prefix \"%s\" is loaded statically and cannot be unloaded",
		prefix));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "STATIC",
		NULL);
	code = TCL_ERROR;
	goto done;
    }
    if (pkgPtr == NULL) {
	/*
	 * The DLL pointed by the provided filename has never been loaded.
	 */

	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"file \"%s\" has never been loaded", fullFileName));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "NEVERLOADED",
		NULL);
	code = TCL_ERROR;
	goto done;
    }

    /*
     * Scan through the list of libraries already loaded in the target
     * interpreter. If the library we want is already loaded there, then we
     * should proceed with unloading.
     */

    code = TCL_ERROR;
    if (pkgPtr != NULL) {
	ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);
	for (ipPtr = ipFirstPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->pkgPtr == pkgPtr) {
		code = TCL_OK;
		break;
	    }
	}
    }
    if (code != TCL_OK) {
	/*
	 * The library has not been loaded in this interpreter.
	 */

	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"file \"%s\" has never been loaded in this interpreter",
		fullFileName));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "NEVERLOADED",
		NULL);
	code = TCL_ERROR;
	goto done;
    }

    /*
     * Ensure that the DLL can be unloaded. If it is a trusted interpreter,
     * pkgPtr->unloadProc must not be NULL for the DLL to be unloadable. If
     * the interpreter is a safe one, pkgPtr->safeUnloadProc must be non-NULL.
     */

    if (Tcl_IsSafe(target)) {
	if (pkgPtr->safeUnloadProc == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "file \"%s\" cannot be unloaded under a safe interpreter",
		    fullFileName));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "CANNOT",
		    NULL);
	    code = TCL_ERROR;
	    goto done;
	}
	unloadProc = pkgPtr->safeUnloadProc;
    } else {
	if (pkgPtr->unloadProc == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "file \"%s\" cannot be unloaded under a trusted interpreter",
		    fullFileName));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "CANNOT",
		    NULL);
	    code = TCL_ERROR;
	    goto done;
	}
	unloadProc = pkgPtr->unloadProc;
    }

    /*
     * We are ready to unload the library. First, evaluate the unload
     * function. If this fails, we cannot proceed with unload. Also, we must
     * specify the proper flag to pass to the unload callback.
     * TCL_UNLOAD_DETACH_FROM_INTERPRETER is defined when the callback should
     * only remove itself from the interpreter; the library will be unloaded
     * in a future call of unload. In case the library will be unloaded just
     * after the callback returns, TCL_UNLOAD_DETACH_FROM_PROCESS is passed.
     */

    code = TCL_UNLOAD_DETACH_FROM_INTERPRETER;
    if (!keepLibrary) {
	Tcl_MutexLock(&packageMutex);
	trustedRefCount = pkgPtr->interpRefCount;
	safeRefCount = pkgPtr->safeInterpRefCount;
	Tcl_MutexUnlock(&packageMutex);

	if (Tcl_IsSafe(target)) {
	    safeRefCount--;
	} else {
	    trustedRefCount--;
	}

	if (safeRefCount <= 0 && trustedRefCount <= 0) {
	    code = TCL_UNLOAD_DETACH_FROM_PROCESS;
	}
    }
    code = unloadProc(target, code);
    if (code != TCL_OK) {
	Tcl_TransferResult(target, code, interp);
	goto done;
    }

    /*
     * The unload function executed fine. Examine the reference count to see
     * if we unload the DLL.
     */

    Tcl_MutexLock(&packageMutex);
    if (Tcl_IsSafe(target)) {
	pkgPtr->safeInterpRefCount--;

	/*
	 * Do not let counter get negative.
	 */

	if (pkgPtr->safeInterpRefCount < 0) {
	    pkgPtr->safeInterpRefCount = 0;
	}
    } else {
	pkgPtr->interpRefCount--;

	/*
	 * Do not let counter get negative.
	 */

	if (pkgPtr->interpRefCount < 0) {
	    pkgPtr->interpRefCount = 0;
	}
    }
    trustedRefCount = pkgPtr->interpRefCount;
    safeRefCount = pkgPtr->safeInterpRefCount;
    Tcl_MutexUnlock(&packageMutex);

    code = TCL_OK;
    if (pkgPtr->safeInterpRefCount <= 0 && pkgPtr->interpRefCount <= 0
	    && !keepLibrary) {
	/*
	 * Unload the shared library from the application memory...
	 */

#if defined(TCL_UNLOAD_DLLS) || defined(_WIN32)
	/*
	 * Some Unix dlls are poorly behaved - registering things like atexit
	 * calls that can't be unregistered. If you unload such dlls, you get
	 * a core on exit because it wants to call a function in the dll after
	 * it's been unloaded.
	 */

	if (pkgPtr->fileName[0] != '\0') {
	    Tcl_MutexLock(&packageMutex);
	    if (Tcl_FSUnloadFile(interp, pkgPtr->loadHandle) == TCL_OK) {
		/*
		 * Remove this library from the loaded library cache.
		 */

		defaultPtr = pkgPtr;
		if (defaultPtr == firstPackagePtr) {
		    firstPackagePtr = pkgPtr->nextPtr;
		} else {
		    for (pkgPtr = firstPackagePtr; pkgPtr != NULL;
			    pkgPtr = pkgPtr->nextPtr) {
			if (pkgPtr->nextPtr == defaultPtr) {
			    pkgPtr->nextPtr = defaultPtr->nextPtr;
			    break;
			}
		    }
		}

		/*
		 * Remove this library from the interpreter's library cache.
		 */

		ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);
		ipPtr = ipFirstPtr;
		if (ipPtr->pkgPtr == defaultPtr) {
		    ipFirstPtr = ipFirstPtr->nextPtr;
		} else {
		    InterpPackage *ipPrevPtr;

		    for (ipPrevPtr = ipPtr; ipPtr != NULL;
			    ipPrevPtr = ipPtr, ipPtr = ipPtr->nextPtr) {
			if (ipPtr->pkgPtr == defaultPtr) {
			    ipPrevPtr->nextPtr = ipPtr->nextPtr;
			    break;
			}
		    }
		}
		Tcl_SetAssocData(target, "tclLoad", LoadCleanupProc,
			ipFirstPtr);
		ckfree(defaultPtr->fileName);
		ckfree(defaultPtr->prefix);
		ckfree(defaultPtr);
		ckfree(ipPtr);
		Tcl_MutexUnlock(&packageMutex);
	    } else {
		code = TCL_ERROR;
	    }
	}
#else
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"file \"%s\" cannot be unloaded: unloading disabled",
		fullFileName));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "DISABLED",
		NULL);
	code = TCL_ERROR;
#endif
    }

  done:
    Tcl_DStringFree(&pfx);
    Tcl_DStringFree(&tmp);
    if (!complain && (code != TCL_OK)) {
	code = TCL_OK;
	Tcl_ResetResult(interp);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_StaticPackage --
 *
 *	This function is invoked to indicate that a particular library has
 *	been linked statically with an application.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Once this function completes, the library becomes loadable via the
 *	"load" command with an empty file name.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_StaticPackage(
    Tcl_Interp *interp,		/* If not NULL, it means that the library has
				 * already been loaded into the given
				 * interpreter by calling the appropriate init
				 * proc. */
    const char *prefix,	/* Prefix (must be properly
				 * capitalized: first letter upper case,
				 * others lower case). */
    Tcl_PackageInitProc *initProc,
				/* Function to call to incorporate this
				 * library into a trusted interpreter. */
    Tcl_PackageInitProc *safeInitProc)
				/* Function to call to incorporate this
				 * library into a safe interpreter (one that
				 * will execute untrusted scripts). NULL means
				 * the library can't be used in safe
				 * interpreters. */
{
    LoadedPackage *pkgPtr;
    InterpPackage *ipPtr, *ipFirstPtr;

    /*
     * Check to see if someone else has already reported this library as
     * statically loaded in the process.
     */

    Tcl_MutexLock(&packageMutex);
    for (pkgPtr = firstPackagePtr; pkgPtr != NULL; pkgPtr = pkgPtr->nextPtr) {
	if ((pkgPtr->initProc == initProc)
		&& (pkgPtr->safeInitProc == safeInitProc)
		&& (strcmp(pkgPtr->prefix, prefix) == 0)) {
	    break;
	}
    }
    Tcl_MutexUnlock(&packageMutex);

    /*
     * If the library is not yet recorded as being loaded statically, add it
     * to the list now.
     */

    if (pkgPtr == NULL) {
	pkgPtr = (LoadedPackage *)ckalloc(sizeof(LoadedPackage));
	pkgPtr->fileName	= (char *)ckalloc(1);
	pkgPtr->fileName[0]	= 0;
	pkgPtr->prefix	= (char *)ckalloc(strlen(prefix) + 1);
	strcpy(pkgPtr->prefix, prefix);
	pkgPtr->loadHandle	= NULL;
	pkgPtr->initProc	= initProc;
	pkgPtr->safeInitProc	= safeInitProc;
	Tcl_MutexLock(&packageMutex);
	pkgPtr->nextPtr		= firstPackagePtr;
	firstPackagePtr		= pkgPtr;
	Tcl_MutexUnlock(&packageMutex);
    }

    if (interp != NULL) {

	/*
	 * If we're loading the library into an interpreter, determine whether
	 * it's already loaded.
	 */

	ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(interp, "tclLoad", NULL);
	for (ipPtr = ipFirstPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->pkgPtr == pkgPtr) {
		return;
	    }
	}

	/*
	 * Library isn't loaded in the current interp yet. Mark it as now being
	 * loaded.
	 */

	ipPtr = (InterpPackage *)ckalloc(sizeof(InterpPackage));
	ipPtr->pkgPtr = pkgPtr;
	ipPtr->nextPtr = ipFirstPtr;
	Tcl_SetAssocData(interp, "tclLoad", LoadCleanupProc, ipPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetLoadedPackagesEx --
 *
 *	This function returns information about all of the files that are
 *	loaded (either in a particular interpreter, or for all interpreters).
 *
 * Results:
 *	The return value is a standard Tcl completion code. If successful, a
 *	list of lists is placed in the interp's result. Each sublist
 *	corresponds to one loaded file; its first element is the name of the
 *	file (or an empty string for something that's statically loaded) and
 *	the second element is the prefix of the library in that file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetLoadedPackagesEx(
    Tcl_Interp *interp,		/* Interpreter in which to return information
				 * or error message. */
    const char *targetName,	/* Name of target interpreter or NULL. If
				 * NULL, return info about all interps;
				 * otherwise, just return info about this
				 * interpreter. */
    const char *prefix)	/* Prefix or NULL. If NULL, return info
				 * for all prefixes.
				 */
{
    Tcl_Interp *target;
    LoadedPackage *pkgPtr;
    InterpPackage *ipPtr;
    Tcl_Obj *resultObj, *pkgDesc[2];

    if (targetName == NULL) {
	TclNewObj(resultObj);
	Tcl_MutexLock(&packageMutex);
	for (pkgPtr = firstPackagePtr; pkgPtr != NULL;
		pkgPtr = pkgPtr->nextPtr) {
	    pkgDesc[0] = Tcl_NewStringObj(pkgPtr->fileName, -1);
	    pkgDesc[1] = Tcl_NewStringObj(pkgPtr->prefix, -1);
	    Tcl_ListObjAppendElement(NULL, resultObj,
		    Tcl_NewListObj(2, pkgDesc));
	}
	Tcl_MutexUnlock(&packageMutex);
	Tcl_SetObjResult(interp, resultObj);
	return TCL_OK;
    }

    target = Tcl_GetChild(interp, targetName);
    if (target == NULL) {
	return TCL_ERROR;
    }
    ipPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);

    /*
     * Return information about all of the available libraries.
     */
    if (prefix) {
	resultObj = NULL;

	for (; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    pkgPtr = ipPtr->pkgPtr;

	    if (!strcmp(prefix, pkgPtr->prefix)) {
		resultObj = Tcl_NewStringObj(pkgPtr->fileName, -1);
		break;
	    }
	}

	if (resultObj) {
	    Tcl_SetObjResult(interp, resultObj);
	}
	return TCL_OK;
    }

    /*
     * Return information about only the libraries that are loaded in a given
     * interpreter.
     */

    TclNewObj(resultObj);
    for (; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	pkgPtr = ipPtr->pkgPtr;
	pkgDesc[0] = Tcl_NewStringObj(pkgPtr->fileName, -1);
	pkgDesc[1] = Tcl_NewStringObj(pkgPtr->prefix, -1);
	Tcl_ListObjAppendElement(NULL, resultObj, Tcl_NewListObj(2, pkgDesc));
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LoadCleanupProc --
 *
 *	This function is called to delete all of the InterpPackage structures
 *	for an interpreter when the interpreter is deleted. It gets invoked
 *	via the Tcl AssocData mechanism.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage for all of the InterpPackage functions for interp get deleted.
 *
 *----------------------------------------------------------------------
 */

static void
LoadCleanupProc(
    ClientData clientData,	/* Pointer to first InterpPackage structure
				 * for interp. */
    TCL_UNUSED(Tcl_Interp *))
{
    InterpPackage *ipPtr, *nextPtr;

    ipPtr = (InterpPackage *)clientData;
    while (ipPtr != NULL) {
	nextPtr = ipPtr->nextPtr;
	ckfree(ipPtr);
	ipPtr = nextPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeLoad --
 *
 *	This function is invoked just before the application exits. It frees
 *	all of the LoadedPackage structures.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeLoad(void)
{
    LoadedPackage *pkgPtr;

    /*
     * No synchronization here because there should just be one thread alive
     * at this point. Logically, packageMutex should be grabbed at this point,
     * but the Mutexes get finalized before the call to this routine. The only
     * subsystem left alive at this point is the memory allocator.
     */

    while (firstPackagePtr != NULL) {
	pkgPtr = firstPackagePtr;
	firstPackagePtr = pkgPtr->nextPtr;

#if defined(TCL_UNLOAD_DLLS) || defined(_WIN32)
	/*
	 * Some Unix dlls are poorly behaved - registering things like atexit
	 * calls that can't be unregistered. If you unload such dlls, you get
	 * a core on exit because it wants to call a function in the dll after
	 * it has been unloaded.
	 */

	if (pkgPtr->fileName[0] != '\0') {
	    Tcl_FSUnloadFile(NULL, pkgPtr->loadHandle);
	}
#endif

	ckfree(pkgPtr->fileName);
	ckfree(pkgPtr->prefix);
	ckfree(pkgPtr);
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
