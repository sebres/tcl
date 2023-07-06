/*
 * tclAbstractList.h --
 *
 *	The AbstractList Obj Type -- a psuedo List
 *
 * Copyright © 2022 by Brian Griffin. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLABSTRACTLIST
#define _TCLABSTRACTLIST

#include "tclInt.h"

static inline const char*
Tcl_AbstractListTypeName(
    Tcl_Obj *objPtr) /* Should be of type AbstractList */
{
    Tcl_AbstractListType *typePtr;
    typePtr = Tcl_AbstractListGetType(objPtr);
    if (typePtr && typePtr->typeName) {
	return typePtr->typeName;
    } else {
	return "abstractlist";
    }
}

Tcl_Obj *   Tcl_AbstractListObjNew(Tcl_Interp *interp, const Tcl_AbstractListType *vTablePtr);
Tcl_WideInt Tcl_AbstractListObjLength(Tcl_Obj *abstractListPtr);
int	    Tcl_AbstractListObjIndex(Tcl_Interp *interp, Tcl_Obj *abstractListPtr,
		Tcl_Size index, Tcl_Obj **elemObj);
int	    Tcl_AbstractListObjRange(Tcl_Interp *interp, Tcl_Obj *abstractListPtr,
		Tcl_Size fromIdx, Tcl_Size toIdx, Tcl_Obj **newObjPtr);
int	    Tcl_AbstractListObjReverse(Tcl_Interp *interp, Tcl_Obj *abstractListPtr,
		Tcl_Obj **newObjPtr);
int	    Tcl_AbstractListObjGetElements(Tcl_Interp *interp, Tcl_Obj *objPtr, Tcl_Size *objcPtr,
		Tcl_Obj ***objvPtr);
Tcl_Obj *   Tcl_AbstractListObjCopy(Tcl_Interp *interp, Tcl_Obj *listPtr);
void	*   Tcl_AbstractListGetConcreteRep(Tcl_Obj *objPtr);
Tcl_Obj *   Tcl_AbstractListSetElement(Tcl_Interp *interp, Tcl_Obj *listPtr,
		Tcl_Size indexCount, Tcl_Obj *const indexArray[], Tcl_Obj *valueObj);
int	    Tcl_AbstractListObjReplace(Tcl_Interp *interp, Tcl_Obj *listObj,
		Tcl_Size first, Tcl_Size numToDelete, Tcl_Size numToInsert,
		Tcl_Obj *const insertObjs[]);

#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */