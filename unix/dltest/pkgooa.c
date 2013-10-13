/*
 * pkgooa.c --
 *
 *	This file contains a simple Tcl package "pkgooa" that is intended for
 *	testing the Tcl dynamic loading facilities.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#undef STATIC_BUILD
#include "tclOO.h"
#include <string.h>

/*
 * Prototypes for procedures defined later in this file:
 */

static int    Pkgooa_StubsOKObjCmd(ClientData clientData,
		Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);


/*
 *----------------------------------------------------------------------
 *
 * Pkgooa_StubsOKObjCmd --
 *
 *	This procedure is invoked to process the "pkgooa_stubsok" Tcl command.
 *	It gives 1 if stubs are used correctly, 0 if stubs are not OK.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
Pkgooa_StubsOKObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(
	    Tcl_CopyObjectInstance == tclOOStubsPtr->tcl_CopyObjectInstance));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Pkgooa_Init --
 *
 *	This is a package initialization procedure, which is called by Tcl
 *	when this package is to be added to an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Object copyObjectInstance(Tcl_Interp *interp,
		Tcl_Object source, const char *name, const char *nameSpace)
{
	Tcl_Object result;
	result = Tcl_CopyObjectInstance(interp, source, name, nameSpace);
	if (result == NULL) {
        Tcl_AppendResult(interp, "ERROR: copy failed.");
	}
	return result;
}

static TclOOStubs stubsCopy = {
    TCL_STUB_MAGIC,
    NULL,
    copyObjectInstance
    /* more entries here, but those are not
     * needed for this test-case. */
};

DLLEXPORT int
Pkgooa_Init(
    Tcl_Interp *interp)		/* Interpreter in which the package is to be
				 * made available. */
{
    int code;

    if (Tcl_InitStubs(interp, "9.0", 0) == NULL) {
	return TCL_ERROR;
    }
    if (tclStubsPtr == NULL) {
	Tcl_AppendResult(interp, "Tcl stubs are not inialized, "
		"did you compile using -DUSE_TCL_STUBS? ");
	return TCL_ERROR;
    }
    if (Tcl_OOInitStubs(interp) == NULL) {
	return TCL_ERROR;
    }
    if (tclOOStubsPtr == NULL) {
	Tcl_AppendResult(interp, "TclOO stubs are not inialized");
	return TCL_ERROR;
    }

    /* Test case for Bug [f51efe99a7].
     *
     * Let tclOOStubsPtr point to an alternate stub table
     * (with only a single function, that's enough for
     * this test). This way, the function "pkgooa_stubsok"
     * can check whether the TclOO function calls really
     * use the stub table, or only pretend to.
     *
     * On platforms without backlinking (Windows, Cygwin,
     * AIX), this code doesn't even compile without using
     * stubs, but on UNIX ELF systems, the problem is
     * less visible.
     */

    tclOOStubsPtr = &stubsCopy;

    code = Tcl_PkgProvide(interp, "Pkgooa", "1.0");
    if (code != TCL_OK) {
	return code;
    }
    Tcl_CreateObjCommand(interp, "pkgooa_stubsok", Pkgooa_StubsOKObjCmd, NULL, NULL);
    return TCL_OK;
}
