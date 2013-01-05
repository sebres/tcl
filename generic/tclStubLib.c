/*
 * tclStubLib.c --
 *
 *	Stub object that will be statically linked into extensions that want
 *	to access Tcl.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 1998 Paul Duffin.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * We need to ensure that we use the stub macros so that this file contains no
 * references to any of the stub functions. This will make it possible to
 * build an extension that references Tcl_InitStubs but doesn't end up
 * including the rest of the stub functions.
 */

#define USE_TCL_STUBS
#include "tclInt.h"

MODULE_SCOPE const TclStubs *tclStubsPtr;
MODULE_SCOPE const TclPlatStubs *tclPlatStubsPtr;
MODULE_SCOPE const TclIntStubs *tclIntStubsPtr;
MODULE_SCOPE const TclIntPlatStubs *tclIntPlatStubsPtr;

const TclStubs *tclStubsPtr = NULL;
const TclPlatStubs *tclPlatStubsPtr = NULL;
const TclIntStubs *tclIntStubsPtr = NULL;
const TclIntPlatStubs *tclIntPlatStubsPtr = NULL;

/*
 * Use our own ISDIGIT to avoid linking to libc on windows
 */

#define ISDIGIT(c) (((unsigned)((c)-'0')) <= 9)

/*
 *----------------------------------------------------------------------
 *
 * TclInitStubs --
 *
 *	Tries to initialise the stub table pointers and ensures that the
 *	correct version of Tcl is loaded.
 *
 * Results:
 *	The actual version of Tcl that satisfies the request, or NULL to
 *	indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */
MODULE_SCOPE const char *
TclInitStubs(
    Tcl_Interp *interp,
    const char *version,
    int exact,
    const char *tclversion,
    int magic)
{
    Interp *iPtr = (Interp *) interp;
    const char *actualVersion = tclversion;
    ClientData pkgData = NULL;
    const TclStubs *stubsPtr = iPtr->stubTable;

    /*
     * We can't optimize this check by caching tclStubsPtr because that
     * prevents apps from being able to load/unload Tcl dynamically multiple
     * times. [Bug 615304]
     */

    if (!stubsPtr || (stubsPtr->magic != magic)) {
	iPtr->legacyResult = "interpreter uses an incompatible stubs mechanism";
	iPtr->legacyFreeProc = 0; /* TCL_STATIC */
	return NULL;
    }

    if (iPtr->legacyResult) {
	actualVersion = stubsPtr->tcl_PkgRequireEx(interp, "Tcl", version, 0, &pkgData);
	if (actualVersion == NULL) {
	    return NULL;
	}
	if (exact) {
	    const char *p = version;
	    int count = 0;

	    while (*p) {
		count += !ISDIGIT(*p++);
	    }
	    if (count == 1) {
		const char *q = actualVersion;

		p = version;
		while (*p && (*p == *q)) {
		    p++; q++;
		}
		if (*p || ISDIGIT(*q)) {
		    /* Construct error message */
		    stubsPtr->tcl_PkgRequireEx(interp, "Tcl", version, 1, NULL);
		    return NULL;
		}
	    } else {
		actualVersion = stubsPtr->tcl_PkgRequireEx(interp, "Tcl", version, 1, NULL);
		if (actualVersion == NULL) {
		    return NULL;
		}
	    }
	}
    }
    tclStubsPtr = stubsPtr;
    if (tclStubsPtr->hooks) {
	tclPlatStubsPtr = tclStubsPtr->hooks->tclPlatStubs;
	tclIntStubsPtr = tclStubsPtr->hooks->tclIntStubs;
	tclIntPlatStubsPtr = tclStubsPtr->hooks->tclIntPlatStubs;
    } else {
	tclPlatStubsPtr = NULL;
	tclIntStubsPtr = NULL;
	tclIntPlatStubsPtr = NULL;
    }
    return actualVersion;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
