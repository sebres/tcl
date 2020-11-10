/*
 * tclLoad.c --
 *
 *	This file provides the generic portion (those that are the same on all
 *	platforms) of Tcl's dynamic loading facilities.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * The following structure describes a package that has been loaded either
 * dynamically (with the "load" command) or statically (as indicated by a call
 * to TclGetLoadedLibraries). All such packages are linked together into a
 * single list for the process. Packages are never unloaded, until the
 * application exits, when TclFinalizeLoad is called, and these structures are
 * freed.
 */

typedef struct LoadedLibrary {
    char *fileName;		/* Name of the file from which the package was
				 * loaded. An empty string means the package
				 * is loaded statically. Malloc-ed. */
    char *prefix;		/* Prefix for the library.
				 * Malloc-ed. */
    Tcl_LoadHandle loadHandle;	/* Token for the loaded file which should be
				 * passed to (*unLoadProcPtr)() when the file
				 * is no longer needed. If fileName is NULL,
				 * then this field is irrelevant. */
    Tcl_PackageInitProc *initProc;
				/* Initialization function to call to
				 * incorporate this package into a trusted
				 * interpreter. */
    Tcl_PackageInitProc *safeInitProc;
				/* Initialization function to call to
				 * incorporate this package into a safe
				 * interpreter (one that will execute
				 * untrusted scripts). NULL means the package
				 * can't be used in unsafe interpreters. */
    Tcl_PackageUnloadProc *unloadProc;
				/* Finalisation function to unload a package
				 * from a trusted interpreter. NULL means that
				 * the package cannot be unloaded. */
    Tcl_PackageUnloadProc *safeUnloadProc;
				/* Finalisation function to unload a package
				 * from a safe interpreter. NULL means that
				 * the package cannot be unloaded. */
    int interpRefCount;		/* How many times the package has been loaded
				 * in trusted interpreters. */
    int safeInterpRefCount;	/* How many times the package has been loaded
				 * in safe interpreters. */
    struct LoadedLibrary *nextPtr;
				/* Next in list of all packages loaded into
				 * this application process. NULL means end of
				 * list. */
} LoadedLibrary;

/*
 * TCL_THREADS
 * There is a global list of packages that is anchored at firstLibraryPtr.
 * Access to this list is governed by a mutex.
 */

static LoadedLibrary *firstLibraryPtr = NULL;
				/* First in list of all packages loaded into
				 * this process. */

TCL_DECLARE_MUTEX(libraryMutex)

/*
 * The following structure represents a particular package that has been
 * incorporated into a particular interpreter (by calling its initialization
 * function). There is a list of these structures for each interpreter, with
 * an AssocData value (key "load") for the interpreter that points to the
 * first package (if any).
 */

typedef struct InterpPackage {
    LoadedLibrary *libraryPtr;	/* Points to detailed information about
				 * package. */
    struct InterpPackage *nextPtr;
				/* Next package in this interpreter, or NULL
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
    LoadedLibrary *libraryPtr, *defaultPtr;
    Tcl_DString pkgName, tmp, initName, safeInitName;
    Tcl_DString unloadName, safeUnloadName;
    InterpPackage *ipFirstPtr, *ipPtr;
    int code, namesMatch, filesMatch, offset;
    const char *symbols[2];
    Tcl_PackageInitProc *initProc;
    const char *p, *fullFileName, *prefix;
    Tcl_LoadHandle loadHandle;
    Tcl_UniChar ch = 0;
    size_t len;
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
    fullFileName = TclGetString(objv[1]);

    Tcl_DStringInit(&pkgName);
    Tcl_DStringInit(&initName);
    Tcl_DStringInit(&safeInitName);
    Tcl_DStringInit(&unloadName);
    Tcl_DStringInit(&safeUnloadName);
    Tcl_DStringInit(&tmp);

    prefix = NULL;
    if (objc >= 3) {
	prefix = TclGetString(objv[2]);
	if (prefix[0] == '\0') {
	    prefix = NULL;
	}
    }
    if ((fullFileName[0] == 0) && (prefix == NULL)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"must specify either file name or package name", -1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD", "NOLIBRARY",
		NULL);
	code = TCL_ERROR;
	goto done;
    }

    /*
     * Figure out which interpreter we're going to load the package into.
     */

    target = interp;
    if (objc == 4) {
	const char *childIntName = TclGetString(objv[3]);

	target = Tcl_GetChild(interp, childIntName);
	if (target == NULL) {
	    code = TCL_ERROR;
	    goto done;
	}
    }

    /*
     * Scan through the packages that are currently loaded to see if the
     * package we want is already loaded. We'll use a loaded package if it
     * meets any of the following conditions:
     *  - Its name and file match the once we're looking for.
     *  - Its file matches, and we weren't given a name.
     *  - Its name matches, the file name was specified as empty, and there is
     *	  only no statically loaded package with the same name.
     */

    Tcl_MutexLock(&libraryMutex);

    defaultPtr = NULL;
    for (libraryPtr = firstLibraryPtr; libraryPtr != NULL; libraryPtr = libraryPtr->nextPtr) {
	if (prefix == NULL) {
	    namesMatch = 0;
	} else {
	    TclDStringClear(&pkgName);
	    Tcl_DStringAppend(&pkgName, prefix, -1);
	    TclDStringClear(&tmp);
	    Tcl_DStringAppend(&tmp, libraryPtr->prefix, -1);
	    if (strcmp(Tcl_DStringValue(&tmp),
		    Tcl_DStringValue(&pkgName)) == 0) {
		namesMatch = 1;
	    } else {
		namesMatch = 0;
	    }
	}
	TclDStringClear(&pkgName);

	filesMatch = (strcmp(libraryPtr->fileName, fullFileName) == 0);
	if (filesMatch && (namesMatch || (prefix == NULL))) {
	    break;
	}
	if (namesMatch && (fullFileName[0] == 0)) {
	    defaultPtr = libraryPtr;
	}
	if (filesMatch && !namesMatch && (fullFileName[0] != 0)) {
	    /*
	     * Can't have two different packages loaded from the same file.
	     */

	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "file \"%s\" is already loaded for package \"%s\"",
		    fullFileName, libraryPtr->prefix));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD",
		    "SPLITPERSONALITY", NULL);
	    code = TCL_ERROR;
	    Tcl_MutexUnlock(&libraryMutex);
	    goto done;
	}
    }
    Tcl_MutexUnlock(&libraryMutex);
    if (libraryPtr == NULL) {
	libraryPtr = defaultPtr;
    }

    /*
     * Scan through the list of packages already loaded in the target
     * interpreter. If the package we want is already loaded there, then
     * there's nothing for us to do.
     */

    if (libraryPtr != NULL) {
	ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);
	for (ipPtr = ipFirstPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->libraryPtr == libraryPtr) {
		code = TCL_OK;
		goto done;
	    }
	}
    }

    if (libraryPtr == NULL) {
	/*
	 * The desired file isn't currently loaded, so load it. It's an error
	 * if the desired package is a static one.
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
	 * Figure out the module name if it wasn't provided explicitly.
	 */

	if (prefix != NULL) {
	    Tcl_DStringAppend(&pkgName, prefix, -1);
	} else {
	    Tcl_Obj *splitPtr, *pkgGuessPtr;
	    int pElements;
	    const char *pkgGuess;

	    /*
	     * Threading note - this call used to be protected by a mutex.
	     */

	    /*
	     * The platform-specific code couldn't figure out the module
	     * name. Make a guess by taking the last element of the file
	     * name, stripping off any leading "lib", and then using all
	     * of the alphabetic and underline characters that follow
	     * that.
	     */

	    splitPtr = Tcl_FSSplitPath(objv[1], &pElements);
	    Tcl_ListObjIndex(NULL, splitPtr, pElements -1, &pkgGuessPtr);
	    pkgGuess = TclGetString(pkgGuessPtr);
#if defined(_WIN32) || defined(__CYGWIN__)
	    if ((pkgGuess[0] == 'w') && (pkgGuess[1] == 'i')
		    && (pkgGuess[2] == 'n')) {
		pkgGuess += 3;
	    } else
#endif /* __CYGWIN__ */
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
			"couldn't figure out package name for %s",
			fullFileName));
		Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD",
			"WHATPACKAGE", NULL);
		code = TCL_ERROR;
		goto done;
	    }
	    Tcl_DStringAppend(&pkgName, pkgGuess, p - pkgGuess);
	    Tcl_DecrRefCount(splitPtr);

	    /*
	     * Fix the capitalization in the package name so that the first
	     * character is in caps (or title case) but the others are all
	     * lower-case.
	     */

	    Tcl_DStringSetLength(&pkgName,
		    Tcl_UtfToTitle(Tcl_DStringValue(&pkgName)));

	}

	/*
	 * Compute the names of the two initialization functions, based on the
	 * package name.
	 */

	TclDStringAppendDString(&initName, &pkgName);
	TclDStringAppendLiteral(&initName, "_Init");
	TclDStringAppendDString(&safeInitName, &pkgName);
	TclDStringAppendLiteral(&safeInitName, "_SafeInit");
	TclDStringAppendDString(&unloadName, &pkgName);
	TclDStringAppendLiteral(&unloadName, "_Unload");
	TclDStringAppendDString(&safeUnloadName, &pkgName);
	TclDStringAppendLiteral(&safeUnloadName, "_SafeUnload");

	/*
	 * Call platform-specific code to load the package and find the two
	 * initialization functions.
	 */

	symbols[0] = Tcl_DStringValue(&initName);
	symbols[1] = NULL;

	Tcl_MutexLock(&libraryMutex);
	code = Tcl_LoadFile(interp, objv[1], symbols, flags, &initProc,
		&loadHandle);
	Tcl_MutexUnlock(&libraryMutex);
	if (code != TCL_OK) {
	    goto done;
	}

	/*
	 * Create a new record to describe this package.
	 */

	libraryPtr = (LoadedLibrary *)Tcl_Alloc(sizeof(LoadedLibrary));
	len = strlen(fullFileName) + 1;
	libraryPtr->fileName	   = (char *)Tcl_Alloc(len);
	memcpy(libraryPtr->fileName, fullFileName, len);
	len = Tcl_DStringLength(&pkgName) + 1;
	libraryPtr->prefix	   = (char *)Tcl_Alloc(len);
	memcpy(libraryPtr->prefix, Tcl_DStringValue(&pkgName), len);
	libraryPtr->loadHandle	   = loadHandle;
	libraryPtr->initProc	   = initProc;
	libraryPtr->safeInitProc	   = (Tcl_PackageInitProc *)
		Tcl_FindSymbol(interp, loadHandle,
			Tcl_DStringValue(&safeInitName));
	libraryPtr->unloadProc	   = (Tcl_PackageUnloadProc *)
		Tcl_FindSymbol(interp, loadHandle,
			Tcl_DStringValue(&unloadName));
	libraryPtr->safeUnloadProc	   = (Tcl_PackageUnloadProc *)
		Tcl_FindSymbol(interp, loadHandle,
			Tcl_DStringValue(&safeUnloadName));
	libraryPtr->interpRefCount	   = 0;
	libraryPtr->safeInterpRefCount = 0;

	Tcl_MutexLock(&libraryMutex);
	libraryPtr->nextPtr		   = firstLibraryPtr;
	firstLibraryPtr		   = libraryPtr;
	Tcl_MutexUnlock(&libraryMutex);

	/*
	 * The Tcl_FindSymbol calls may have left a spurious error message in
	 * the interpreter result.
	 */

	Tcl_ResetResult(interp);
    }

    /*
     * Invoke the package's initialization function (either the normal one or
     * the safe one, depending on whether or not the interpreter is safe).
     */

    if (Tcl_IsSafe(target)) {
	if (libraryPtr->safeInitProc == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "can't use package in a safe interpreter: no"
		    " %s_SafeInit procedure", libraryPtr->prefix));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD", "UNSAFE",
		    NULL);
	    code = TCL_ERROR;
	    goto done;
	}
	code = libraryPtr->safeInitProc(target);
    } else {
	if (libraryPtr->initProc == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "can't attach package to interpreter: no %s_Init procedure",
		    libraryPtr->prefix));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LOAD", "ENTRYPOINT",
		    NULL);
	    code = TCL_ERROR;
	    goto done;
	}
	code = libraryPtr->initProc(target);
    }

    /*
     * Test for whether the initialization failed. If so, transfer the error
     * from the target interpreter to the originating one.
     */

    if (code != TCL_OK) {
	Interp *iPtr = (Interp *) target;
	if (iPtr->legacyResult && *(iPtr->legacyResult) && !iPtr->legacyFreeProc) {
	    /*
	     * A call to Tcl_InitStubs() determined the caller extension and
	     * this interp are incompatible in their stubs mechanisms, and
	     * recorded the error in the oldest legacy place we have to do so.
	     */
	    Tcl_SetObjResult(target, Tcl_NewStringObj(iPtr->legacyResult, -1));
	    iPtr->legacyResult = NULL;
	    iPtr->legacyFreeProc = (void (*) (void))-1;
	}
	Tcl_TransferResult(target, code, interp);
	goto done;
    }

    /*
     * Record the fact that the package has been loaded in the target
     * interpreter.
     *
     * Update the proper reference count.
     */

    Tcl_MutexLock(&libraryMutex);
    if (Tcl_IsSafe(target)) {
	libraryPtr->safeInterpRefCount++;
    } else {
	libraryPtr->interpRefCount++;
    }
    Tcl_MutexUnlock(&libraryMutex);

    /*
     * Refetch ipFirstPtr: loading the package may have introduced additional
     * static packages at the head of the linked list!
     */

    ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);
    ipPtr = (InterpPackage *)Tcl_Alloc(sizeof(InterpPackage));
    ipPtr->libraryPtr = libraryPtr;
    ipPtr->nextPtr = ipFirstPtr;
    Tcl_SetAssocData(target, "tclLoad", LoadCleanupProc, ipPtr);

  done:
    Tcl_DStringFree(&pkgName);
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
    LoadedLibrary *libraryPtr, *defaultPtr;
    Tcl_DString pkgName, tmp;
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
	    fullFileName = TclGetString(objv[i]);
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

    fullFileName = TclGetString(objv[i]);
    Tcl_DStringInit(&pkgName);
    Tcl_DStringInit(&tmp);

    prefix = NULL;
    if (objc - i >= 2) {
	prefix = TclGetString(objv[i+1]);
	if (prefix[0] == '\0') {
	    prefix = NULL;
	}
    }
    if ((fullFileName[0] == 0) && (prefix == NULL)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"must specify either file name or package name", -1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "NOLIBRARY",
		NULL);
	code = TCL_ERROR;
	goto done;
    }

    /*
     * Figure out which interpreter we're going to load the package into.
     */

    target = interp;
    if (objc - i == 3) {
	const char *childIntName = TclGetString(objv[i + 2]);

	target = Tcl_GetChild(interp, childIntName);
	if (target == NULL) {
	    return TCL_ERROR;
	}
    }

    /*
     * Scan through the packages that are currently loaded to see if the
     * package we want is already loaded. We'll use a loaded package if it
     * meets any of the following conditions:
     *  - Its name and file match the once we're looking for.
     *  - Its file matches, and we weren't given a name.
     *  - Its name matches, the file name was specified as empty, and there is
     *	  only no statically loaded package with the same name.
     */

    Tcl_MutexLock(&libraryMutex);

    defaultPtr = NULL;
    for (libraryPtr = firstLibraryPtr; libraryPtr != NULL; libraryPtr = libraryPtr->nextPtr) {
	int namesMatch, filesMatch;

	if (prefix == NULL) {
	    namesMatch = 0;
	} else {
	    TclDStringClear(&pkgName);
	    Tcl_DStringAppend(&pkgName, prefix, -1);
	    TclDStringClear(&tmp);
	    Tcl_DStringAppend(&tmp, libraryPtr->prefix, -1);
	    if (strcmp(Tcl_DStringValue(&tmp),
		    Tcl_DStringValue(&pkgName)) == 0) {
		namesMatch = 1;
	    } else {
		namesMatch = 0;
	    }
	}
	TclDStringClear(&pkgName);

	filesMatch = (strcmp(libraryPtr->fileName, fullFileName) == 0);
	if (filesMatch && (namesMatch || (prefix == NULL))) {
	    break;
	}
	if (namesMatch && (fullFileName[0] == 0)) {
	    defaultPtr = libraryPtr;
	}
	if (filesMatch && !namesMatch && (fullFileName[0] != 0)) {
	    break;
	}
    }
    Tcl_MutexUnlock(&libraryMutex);
    if (fullFileName[0] == 0) {
	/*
	 * It's an error to try unload a static package.
	 */

	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"library with prefix \"%s\" is loaded statically and cannot be unloaded",
		prefix));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "STATIC",
		NULL);
	code = TCL_ERROR;
	goto done;
    }
    if (libraryPtr == NULL) {
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
     * Scan through the list of packages already loaded in the target
     * interpreter. If the package we want is already loaded there, then we
     * should proceed with unloading.
     */

    code = TCL_ERROR;
    if (libraryPtr != NULL) {
	ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);
	for (ipPtr = ipFirstPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->libraryPtr == libraryPtr) {
		code = TCL_OK;
		break;
	    }
	}
    }
    if (code != TCL_OK) {
	/*
	 * The package has not been loaded in this interpreter.
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
     * libraryPtr->unloadProc must not be NULL for the DLL to be unloadable. If
     * the interpreter is a safe one, libraryPtr->safeUnloadProc must be non-NULL.
     */

    if (Tcl_IsSafe(target)) {
	if (libraryPtr->safeUnloadProc == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "file \"%s\" cannot be unloaded under a safe interpreter",
		    fullFileName));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "CANNOT",
		    NULL);
	    code = TCL_ERROR;
	    goto done;
	}
	unloadProc = libraryPtr->safeUnloadProc;
    } else {
	if (libraryPtr->unloadProc == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "file \"%s\" cannot be unloaded under a trusted interpreter",
		    fullFileName));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "UNLOAD", "CANNOT",
		    NULL);
	    code = TCL_ERROR;
	    goto done;
	}
	unloadProc = libraryPtr->unloadProc;
    }

    /*
     * We are ready to unload the package. First, evaluate the unload
     * function. If this fails, we cannot proceed with unload. Also, we must
     * specify the proper flag to pass to the unload callback.
     * TCL_UNLOAD_DETACH_FROM_INTERPRETER is defined when the callback should
     * only remove itself from the interpreter; the library will be unloaded
     * in a future call of unload. In case the library will be unloaded just
     * after the callback returns, TCL_UNLOAD_DETACH_FROM_PROCESS is passed.
     */

    code = TCL_UNLOAD_DETACH_FROM_INTERPRETER;
    if (!keepLibrary) {
	Tcl_MutexLock(&libraryMutex);
	trustedRefCount = libraryPtr->interpRefCount;
	safeRefCount = libraryPtr->safeInterpRefCount;
	Tcl_MutexUnlock(&libraryMutex);

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

    Tcl_MutexLock(&libraryMutex);
    if (Tcl_IsSafe(target)) {
	libraryPtr->safeInterpRefCount--;

	/*
	 * Do not let counter get negative.
	 */

	if (libraryPtr->safeInterpRefCount < 0) {
	    libraryPtr->safeInterpRefCount = 0;
	}
    } else {
	libraryPtr->interpRefCount--;

	/*
	 * Do not let counter get negative.
	 */

	if (libraryPtr->interpRefCount < 0) {
	    libraryPtr->interpRefCount = 0;
	}
    }
    trustedRefCount = libraryPtr->interpRefCount;
    safeRefCount = libraryPtr->safeInterpRefCount;
    Tcl_MutexUnlock(&libraryMutex);

    code = TCL_OK;
    if (libraryPtr->safeInterpRefCount <= 0 && libraryPtr->interpRefCount <= 0
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

	if (libraryPtr->fileName[0] != '\0') {
	    Tcl_MutexLock(&libraryMutex);
	    if (Tcl_FSUnloadFile(interp, libraryPtr->loadHandle) == TCL_OK) {
		/*
		 * Remove this library from the loaded library cache.
		 */

		defaultPtr = libraryPtr;
		if (defaultPtr == firstLibraryPtr) {
		    firstLibraryPtr = libraryPtr->nextPtr;
		} else {
		    for (libraryPtr = firstLibraryPtr; libraryPtr != NULL;
			    libraryPtr = libraryPtr->nextPtr) {
			if (libraryPtr->nextPtr == defaultPtr) {
			    libraryPtr->nextPtr = defaultPtr->nextPtr;
			    break;
			}
		    }
		}

		/*
		 * Remove this library from the interpreter's library cache.
		 */

		ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);
		ipPtr = ipFirstPtr;
		if (ipPtr->libraryPtr == defaultPtr) {
		    ipFirstPtr = ipFirstPtr->nextPtr;
		} else {
		    InterpPackage *ipPrevPtr;

		    for (ipPrevPtr = ipPtr; ipPtr != NULL;
			    ipPrevPtr = ipPtr, ipPtr = ipPtr->nextPtr) {
			if (ipPtr->libraryPtr == defaultPtr) {
			    ipPrevPtr->nextPtr = ipPtr->nextPtr;
			    break;
			}
		    }
		}
		Tcl_SetAssocData(target, "tclLoad", LoadCleanupProc,
			ipFirstPtr);
		Tcl_Free(defaultPtr->fileName);
		Tcl_Free(defaultPtr->prefix);
		Tcl_Free(defaultPtr);
		Tcl_Free(ipPtr);
		Tcl_MutexUnlock(&libraryMutex);
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
    Tcl_DStringFree(&pkgName);
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
 * Tcl_StaticLibrary --
 *
 *	This function is invoked to indicate that a particular package has
 *	been linked statically with an application.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Once this function completes, the package becomes loadable via the
 *	"load" command with an empty file name.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_StaticLibrary(
    Tcl_Interp *interp,		/* If not NULL, it means that the package has
				 * already been loaded into the given
				 * interpreter by calling the appropriate init
				 * proc. */
    const char *prefix,	/* Prefix. */
    Tcl_PackageInitProc *initProc,
				/* Function to call to incorporate this
				 * package into a trusted interpreter. */
    Tcl_PackageInitProc *safeInitProc)
				/* Function to call to incorporate this
				 * package into a safe interpreter (one that
				 * will execute untrusted scripts). NULL means
				 * the package can't be used in safe
				 * interpreters. */
{
    LoadedLibrary *libraryPtr;
    InterpPackage *ipPtr, *ipFirstPtr;

    /*
     * Check to see if someone else has already reported this package as
     * statically loaded in the process.
     */

    Tcl_MutexLock(&libraryMutex);
    for (libraryPtr = firstLibraryPtr; libraryPtr != NULL; libraryPtr = libraryPtr->nextPtr) {
	if ((libraryPtr->initProc == initProc)
		&& (libraryPtr->safeInitProc == safeInitProc)
		&& (strcmp(libraryPtr->prefix, prefix) == 0)) {
	    break;
	}
    }
    Tcl_MutexUnlock(&libraryMutex);

    /*
     * If the package is not yet recorded as being loaded statically, add it
     * to the list now.
     */

    if (libraryPtr == NULL) {
	libraryPtr = (LoadedLibrary *)Tcl_Alloc(sizeof(LoadedLibrary));
	libraryPtr->fileName	= (char *)Tcl_Alloc(1);
	libraryPtr->fileName[0]	= 0;
	libraryPtr->prefix	= (char *)Tcl_Alloc(strlen(prefix) + 1);
	strcpy(libraryPtr->prefix, prefix);
	libraryPtr->loadHandle	= NULL;
	libraryPtr->initProc	= initProc;
	libraryPtr->safeInitProc	= safeInitProc;
	Tcl_MutexLock(&libraryMutex);
	libraryPtr->nextPtr		= firstLibraryPtr;
	firstLibraryPtr		= libraryPtr;
	Tcl_MutexUnlock(&libraryMutex);
    }

    if (interp != NULL) {

	/*
	 * If we're loading the package into an interpreter, determine whether
	 * it's already loaded.
	 */

	ipFirstPtr = (InterpPackage *)Tcl_GetAssocData(interp, "tclLoad", NULL);
	for (ipPtr = ipFirstPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->libraryPtr == libraryPtr) {
		return;
	    }
	}

	/*
	 * Package isn't loaded in the current interp yet. Mark it as now being
	 * loaded.
	 */

	ipPtr = (InterpPackage *)Tcl_Alloc(sizeof(InterpPackage));
	ipPtr->libraryPtr = libraryPtr;
	ipPtr->nextPtr = ipFirstPtr;
	Tcl_SetAssocData(interp, "tclLoad", LoadCleanupProc, ipPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetLoadedLibraries --
 *
 *	This function returns information about all of the files that are
 *	loaded (either in a particular interpreter, or for all interpreters).
 *
 * Results:
 *	The return value is a standard Tcl completion code. If successful, a
 *	list of lists is placed in the interp's result. Each sublist
 *	corresponds to one loaded file; its first element is the name of the
 *	file (or an empty string for something that's statically loaded) and
 *	the second element is the name of the package in that file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetLoadedLibraries(
    Tcl_Interp *interp,		/* Interpreter in which to return information
				 * or error message. */
    const char *targetName,	/* Name of target interpreter or NULL. If
				 * NULL, return info about all interps;
				 * otherwise, just return info about this
				 * interpreter. */
    const char *prefix)	/* Package name or NULL. If NULL, return info
				 * for all packages.
				 */
{
    Tcl_Interp *target;
    LoadedLibrary *libraryPtr;
    InterpPackage *ipPtr;
    Tcl_Obj *resultObj, *pkgDesc[2];

    if (targetName == NULL) {
	TclNewObj(resultObj);
	Tcl_MutexLock(&libraryMutex);
	for (libraryPtr = firstLibraryPtr; libraryPtr != NULL;
		libraryPtr = libraryPtr->nextPtr) {
	    pkgDesc[0] = Tcl_NewStringObj(libraryPtr->fileName, -1);
	    pkgDesc[1] = Tcl_NewStringObj(libraryPtr->prefix, -1);
	    Tcl_ListObjAppendElement(NULL, resultObj,
		    Tcl_NewListObj(2, pkgDesc));
	}
	Tcl_MutexUnlock(&libraryMutex);
	Tcl_SetObjResult(interp, resultObj);
	return TCL_OK;
    }

    target = Tcl_GetChild(interp, targetName);
    if (target == NULL) {
	return TCL_ERROR;
    }
    ipPtr = (InterpPackage *)Tcl_GetAssocData(target, "tclLoad", NULL);

    /*
     * Return information about all of the available packages.
     */
    if (prefix) {
	resultObj = NULL;

	for (; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    libraryPtr = ipPtr->libraryPtr;

	    if (!strcmp(prefix, libraryPtr->prefix)) {
		resultObj = Tcl_NewStringObj(libraryPtr->fileName, -1);
		break;
	    }
	}

	if (resultObj) {
	    Tcl_SetObjResult(interp, resultObj);
	}
	return TCL_OK;
    }

    /*
     * Return information about only the packages that are loaded in a given
     * interpreter.
     */

    TclNewObj(resultObj);
    for (; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	libraryPtr = ipPtr->libraryPtr;
	pkgDesc[0] = Tcl_NewStringObj(libraryPtr->fileName, -1);
	pkgDesc[1] = Tcl_NewStringObj(libraryPtr->prefix, -1);
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
	Tcl_Free(ipPtr);
	ipPtr = nextPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeLoad --
 *
 *	This function is invoked just before the application exits. It frees
 *	all of the LoadedLibrary structures.
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
    LoadedLibrary *libraryPtr;

    /*
     * No synchronization here because there should just be one thread alive
     * at this point. Logically, libraryMutex should be grabbed at this point,
     * but the Mutexes get finalized before the call to this routine. The only
     * subsystem left alive at this point is the memory allocator.
     */

    while (firstLibraryPtr != NULL) {
	libraryPtr = firstLibraryPtr;
	firstLibraryPtr = libraryPtr->nextPtr;

#if defined(TCL_UNLOAD_DLLS) || defined(_WIN32)
	/*
	 * Some Unix dlls are poorly behaved - registering things like atexit
	 * calls that can't be unregistered. If you unload such dlls, you get
	 * a core on exit because it wants to call a function in the dll after
	 * it has been unloaded.
	 */

	if (libraryPtr->fileName[0] != '\0') {
	    Tcl_FSUnloadFile(NULL, libraryPtr->loadHandle);
	}
#endif

	Tcl_Free(libraryPtr->fileName);
	Tcl_Free(libraryPtr->prefix);
	Tcl_Free(libraryPtr);
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
