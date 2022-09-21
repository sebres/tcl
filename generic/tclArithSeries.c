/*
 * tclArithSeries.c --
 *
 *     This file contains the ArithSeries concrete abstract list
 *     implementation. It implements the inner workings of the lseq command.
 *
 * Copyright © 2022 Brian S. Griffin.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclArithSeries.h"
#include <assert.h>

/* -------------------------- ArithSeries object ---------------------------- */


#define ArithSeriesRepPtr(arithSeriesObjPtr) \
    (ArithSeries *) ((arithSeriesObjPtr)->internalRep.twoPtrValue.ptr1)

#define ArithSeriesIndexM(arithSeriesRepPtr, index) \
    ((arithSeriesRepPtr)->isDouble ?					\
     (((ArithSeriesDbl*)(arithSeriesRepPtr))->start+((index) * ((ArithSeriesDbl*)(arithSeriesRepPtr))->step)) \
     :									\
     ((arithSeriesRepPtr)->start+((index) * arithSeriesRepPtr->step)))

#define ArithSeriesGetInternalRep(objPtr, arithRepPtr)		\
    do {								\
	const Tcl_ObjInternalRep *irPtr;				\
	irPtr = TclFetchInternalRep((objPtr), &tclArithSeriesType);	\
	(arithRepPtr) = irPtr ? (ArithSeries *)irPtr->twoPtrValue.ptr1 : NULL;	\
    } while (0)


/*
 * Prototypes for procedures defined later in this file:
 */

static void		DupArithSeriesInternalRep (Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void		FreeArithSeriesInternalRep (Tcl_Obj *listPtr);
static int		SetArithSeriesFromAny (Tcl_Interp *interp, Tcl_Obj *objPtr);
static void		UpdateStringOfArithSeries (Tcl_Obj *listPtr);

/*
 * The structure below defines the arithmetic series Tcl object type by
 * means of procedures that can be invoked by generic object code.
 *
 * The arithmetic series object is a special case of Tcl list representing
 * an interval of an arithmetic series in constant space.
 *
 * The arithmetic series is internally represented with three integers,
 * *start*, *end*, and *step*, Where the length is calculated with
 * the following algorithm:
 *
 * if RANGE == 0 THEN
 *   ERROR
 * if RANGE > 0
 *   LEN is (((END-START)-1)/STEP) + 1
 * else if RANGE < 0
 *   LEN is (((END-START)-1)/STEP) - 1
 *
 * And where the equivalent's list I-th element is calculated
 * as:
 *
 * LIST[i] = START+(STEP*i)
 *
 * Zero elements ranges, like in the case of START=10 END=10 STEP=1
 * are valid and will be equivalent to the empty list.
 */

const Tcl_ObjType tclArithSeriesType = {
    "arithseries",			/* name */
    FreeArithSeriesInternalRep,		/* freeIntRepProc */
    DupArithSeriesInternalRep,		/* dupIntRepProc */
    UpdateStringOfArithSeries,		/* updateStringProc */
    SetArithSeriesFromAny		/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * ArithSeriesLen --
 *
 * 	Compute the length of the equivalent list where
 * 	every element is generated starting from *start*,
 * 	and adding *step* to generate every successive element
 * 	that's < *end* for positive steps, or > *end* for negative
 * 	steps.
 *
 * Results:
 *
 * 	The length of the list generated by the given range,
 * 	that may be zero.
 * 	The function returns -1 if the list is of length infinite.
 *
 * Side effects:
 *
 * 	None.
 *
 *----------------------------------------------------------------------
 */
static Tcl_WideInt
ArithSeriesLen(Tcl_WideInt start, Tcl_WideInt end, Tcl_WideInt step)
{
    Tcl_WideInt len;

    if (step == 0) return 0;
    len = (step ? (1 + (((end-start))/step)) : 0);
    return (len < 0) ? -1 : len;
}

/*
 *----------------------------------------------------------------------
 *
 * TclNewArithSeriesInt --
 *
 *	Creates a new ArithSeries object. The returned object has
 *	refcount = 0.
 *
 * Results:
 *
 * 	A Tcl_Obj pointer to the created ArithSeries object.
 * 	A NULL pointer of the range is invalid.
 *
 * Side Effects:
 *
 * 	None.
 *----------------------------------------------------------------------
 */
Tcl_Obj *
TclNewArithSeriesInt(Tcl_WideInt start, Tcl_WideInt end, Tcl_WideInt step, Tcl_WideInt len)
{
    Tcl_WideInt length = (len>=0 ? len : ArithSeriesLen(start, end, step));
    Tcl_Obj *arithSeriesPtr;
    ArithSeries *arithSeriesRepPtr;

    TclNewObj(arithSeriesPtr);

    if (length <= 0) {
	return arithSeriesPtr;
    }

    arithSeriesRepPtr = (ArithSeries*) ckalloc(sizeof (ArithSeries));
    arithSeriesRepPtr->isDouble = 0;
    arithSeriesRepPtr->start = start;
    arithSeriesRepPtr->end = end;
    arithSeriesRepPtr->step = step;
    arithSeriesRepPtr->len = length;
    arithSeriesRepPtr->elements = NULL;
    arithSeriesPtr->internalRep.twoPtrValue.ptr1 = arithSeriesRepPtr;
    arithSeriesPtr->internalRep.twoPtrValue.ptr2 = NULL;
    arithSeriesPtr->typePtr = &tclArithSeriesType;
    if (length > 0)
    	Tcl_InvalidateStringRep(arithSeriesPtr);

    return arithSeriesPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclNewArithSeriesDbl --
 *
 *	Creates a new ArithSeries object with doubles. The returned object has
 *	refcount = 0.
 *
 * Results:
 *
 * 	A Tcl_Obj pointer to the created ArithSeries object.
 * 	A NULL pointer of the range is invalid.
 *
 * Side Effects:
 *
 * 	None.
 *----------------------------------------------------------------------
 */
Tcl_Obj *
TclNewArithSeriesDbl(double start, double end, double step, Tcl_WideInt len)
{
    Tcl_WideInt length = (len>=0 ? len : ArithSeriesLen(start, end, step));
    Tcl_Obj *arithSeriesPtr;
    ArithSeriesDbl *arithSeriesRepPtr;

    TclNewObj(arithSeriesPtr);

    if (length <= 0) {
	return arithSeriesPtr;
    }

    arithSeriesRepPtr = (ArithSeriesDbl*) ckalloc(sizeof (ArithSeriesDbl));
    arithSeriesRepPtr->isDouble = 1;
    arithSeriesRepPtr->start = start;
    arithSeriesRepPtr->end = end;
    arithSeriesRepPtr->step = step;
    arithSeriesRepPtr->len = length;
    arithSeriesRepPtr->elements = NULL;
    arithSeriesPtr->internalRep.twoPtrValue.ptr1 = arithSeriesRepPtr;
    arithSeriesPtr->internalRep.twoPtrValue.ptr2 = NULL;
    arithSeriesPtr->typePtr = &tclArithSeriesType;
    if (length > 0)
    	Tcl_InvalidateStringRep(arithSeriesPtr);

    return arithSeriesPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * assignNumber --
 *
 *	Create the appropriate Tcl_Obj value for the given numeric values.
 *      Used locally only for decoding [lseq] numeric arguments.
 *	refcount = 0.
 *
 * Results:
 *
 * 	A Tcl_Obj pointer.
 *      No assignment on error.
 *
 * Side Effects:
 *
 * 	None.
 *----------------------------------------------------------------------
 */
static void
assignNumber(int useDoubles, Tcl_WideInt *intNumberPtr, double *dblNumberPtr, Tcl_Obj *numberObj)
{
    union {
	double d;
	Tcl_WideInt i;
    } *number;
    int tcl_number_type;

    if (Tcl_GetNumberFromObj(NULL, numberObj, (ClientData*)&number, &tcl_number_type) != TCL_OK) {
	return;
    }
    if (useDoubles) {
	if (tcl_number_type == TCL_NUMBER_DOUBLE) {
	    *dblNumberPtr = number->d;
	} else {
	    *dblNumberPtr = (double)number->i;
	}
    } else {
	if (tcl_number_type == TCL_NUMBER_INT) {
	    *intNumberPtr = number->i;
	} else {
	    *intNumberPtr = (Tcl_WideInt)number->d;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclNewArithSeriesObj --
 *
 *	Creates a new ArithSeries object. Some arguments may be NULL and will
 *	be computed based on the other given arguments.
 *      refcount = 0.
 *
 * Results:
 *
 * 	A Tcl_Obj pointer to the created ArithSeries object.
 * 	An empty Tcl_Obj if the range is invalid.
 *
 * Side Effects:
 *
 * 	None.
 *----------------------------------------------------------------------
 */
Tcl_Obj *
TclNewArithSeriesObj(int useDoubles, Tcl_Obj *startObj, Tcl_Obj *endObj, Tcl_Obj *stepObj, Tcl_Obj *lenObj)
{
    double dstart, dend, dstep;
    Tcl_WideInt start, end, step, len;

    if (startObj) {
	assignNumber(useDoubles, &start, &dstart, startObj);
    } else {
	start = 0;
	dstart = start;
    }
    if (stepObj) {
	assignNumber(useDoubles, &step, &dstep, stepObj);
	if (useDoubles) {
	    step = dstep;
	} else {
	    dstep = step;
	}
	if (dstep == 0) {
	    return Tcl_NewObj();
	}
    }
    if (endObj) {
	assignNumber(useDoubles, &end, &dend, endObj);
    }
    if (lenObj) {
	Tcl_GetWideIntFromObj(NULL, lenObj, &len);
    }

    if (startObj && endObj) {
	if (!stepObj) {
	    if (useDoubles) {
		dstep = (dstart < dend) ? 1.0 : -1.0;
		step = dstep;
	    } else {
		step = (start < end) ? 1 : -1;
		dstep = step;
	    }
	}
	assert(dstep!=0);
	if (!lenObj) {
	    if (useDoubles) {
		len = (dend - dstart + dstep)/dstep;
	    } else {
		len = (end - start + step)/step;
	    }
	}
    }

    if (!endObj) {
	if (useDoubles) {
	    dend = dstart + (dstep * (len-1));
	    end = dend;
	} else {
	    end = start + (step * (len-1));
	    dend = end;
	}
    }

    if (useDoubles) {
	return TclNewArithSeriesDbl(dstart, dend, dstep, len);
    } else {
	return TclNewArithSeriesInt(start, end, step, len);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclArithSeriesObjStep --
 *
 *	Return a Tcl_Obj with the step value from the give ArithSeries Obj.
 *	refcount = 0.
 *
 * Results:
 *
 * 	A Tcl_Obj pointer to the created ArithSeries object.
 * 	A NULL pointer of the range is invalid.
 *
 * Side Effects:
 *
 * 	None.
 *----------------------------------------------------------------------
 */
/*
 * TclArithSeriesObjStep --
 */
int
TclArithSeriesObjStep(
    Tcl_Obj *arithSeriesPtr,
    Tcl_Obj **stepObj)
{
    ArithSeries *arithSeriesRepPtr;

    if (arithSeriesPtr->typePtr != &tclArithSeriesType) {
        Tcl_Panic("TclArithSeriesObjIndex called with a not ArithSeries Obj.");
    }
    arithSeriesRepPtr = ArithSeriesRepPtr(arithSeriesPtr);
    if (arithSeriesRepPtr->isDouble) {
	*stepObj = Tcl_NewDoubleObj(((ArithSeriesDbl*)(arithSeriesRepPtr))->step);
    } else {
	*stepObj = Tcl_NewWideIntObj(arithSeriesRepPtr->step);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * TclArithSeriesObjIndex --
 *
 *	Returns the element with the specified index in the list
 *	represented by the specified Arithmetic Sequence object.
 *	If the index is out of range, TCL_ERROR is returned,
 *	otherwise TCL_OK is returned and the integer value of the
 *	element is stored in *element.
 *
 * Results:
 *
 * 	TCL_OK on success, TCL_ERROR on index out of range.
 *
 * Side Effects:
 *
 * 	On success, the integer pointed by *element is modified.
 *
 *----------------------------------------------------------------------
 */

int
TclArithSeriesObjIndex(Tcl_Obj *arithSeriesPtr, Tcl_WideInt index, Tcl_Obj **elementObj)
{
    ArithSeries *arithSeriesRepPtr;

    if (arithSeriesPtr->typePtr != &tclArithSeriesType) {
	Tcl_Panic("TclArithSeriesObjIndex called with a not ArithSeries Obj.");
    }
    arithSeriesRepPtr = ArithSeriesRepPtr(arithSeriesPtr);
    if (index < 0 || index >= arithSeriesRepPtr->len) {
	return TCL_ERROR;
    }
    /* List[i] = Start + (Step * index) */
    if (arithSeriesRepPtr->isDouble) {
	*elementObj = Tcl_NewDoubleObj(ArithSeriesIndexM(arithSeriesRepPtr, index));
    } else {
	*elementObj = Tcl_NewWideIntObj(ArithSeriesIndexM(arithSeriesRepPtr, index));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclArithSeriesObjLength
 *
 *	Returns the length of the arithmetic series.
 *
 * Results:
 *
 * 	The length of the series as Tcl_WideInt.
 *
 * Side Effects:
 *
 * 	None.
 *
 *----------------------------------------------------------------------
 */
Tcl_WideInt TclArithSeriesObjLength(Tcl_Obj *arithSeriesPtr)
{
    ArithSeries *arithSeriesRepPtr = (ArithSeries*)
	    arithSeriesPtr->internalRep.twoPtrValue.ptr1;
    return arithSeriesRepPtr->len;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeArithSeriesInternalRep --
 *
 *	Deallocate the storage associated with an arithseries object's
 *	internal representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees arithSeriesPtr's ArithSeries* internal representation and
 *	sets listPtr's	internalRep.twoPtrValue.ptr1 to NULL.
 *
 *----------------------------------------------------------------------
 */

static void
FreeArithSeriesInternalRep(Tcl_Obj *arithSeriesPtr)
{
    ArithSeries *arithSeriesRepPtr =
	    (ArithSeries *) arithSeriesPtr->internalRep.twoPtrValue.ptr1;
    if (arithSeriesRepPtr->elements) {
	Tcl_WideInt i;
	Tcl_Obj**elmts = arithSeriesRepPtr->elements;
	for(i=0; i<arithSeriesRepPtr->len; i++) {
	    if (elmts[i]) {
		Tcl_DecrRefCount(elmts[i]);
	    }
	}
	ckfree((char *) arithSeriesRepPtr->elements);
    }
    ckfree((char *) arithSeriesRepPtr);
    arithSeriesPtr->internalRep.twoPtrValue.ptr1 = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DupArithSeriesInternalRep --
 *
 *	Initialize the internal representation of a arithseries Tcl_Obj to a
 *	copy of the internal representation of an existing arithseries object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	We set "copyPtr"s internal rep to a pointer to a
 *	newly allocated ArithSeries structure.
 *----------------------------------------------------------------------
 */

static void
DupArithSeriesInternalRep(
    Tcl_Obj *srcPtr,		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr)		/* Object with internal rep to set. */
{
    ArithSeries *srcArithSeriesRepPtr =
	    (ArithSeries *) srcPtr->internalRep.twoPtrValue.ptr1;
    ArithSeries *copyArithSeriesRepPtr;

    /*
     * Allocate a new ArithSeries structure. */

    copyArithSeriesRepPtr = (ArithSeries*) ckalloc(sizeof(ArithSeries));
    *copyArithSeriesRepPtr = *srcArithSeriesRepPtr;
    copyArithSeriesRepPtr->elements = NULL;
    copyPtr->internalRep.twoPtrValue.ptr1 = copyArithSeriesRepPtr;
    copyPtr->internalRep.twoPtrValue.ptr2 = NULL;
    copyPtr->typePtr = &tclArithSeriesType;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfArithSeries --
 *
 *	Update the string representation for an arithseries object.
 *	Note: This procedure does not invalidate an existing old string rep
 *	so storage will be lost if this has not already been done.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the list-to-string conversion. This string will be empty if the
 *	list has no elements. The list internal representation
 *	should not be NULL and we assume it is not NULL.
 *
 * Notes:
 * 	At the cost of overallocation it's possible to estimate
 * 	the length of the string representation and make this procedure
 * 	much faster. Because the programmer shouldn't expect the
 * 	string conversion of a big arithmetic sequence to be fast
 * 	this version takes more care of space than time.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfArithSeries(Tcl_Obj *arithSeriesPtr)
{
    ArithSeries *arithSeriesRepPtr =
	    (ArithSeries*) arithSeriesPtr->internalRep.twoPtrValue.ptr1;
    char *elem, *p;
    Tcl_Obj *elemObj;
    Tcl_WideInt i;
    Tcl_WideInt length = 0;
    int slen;

    /*
     * Pass 1: estimate space.
     */
    for (i = 0; i < arithSeriesRepPtr->len; i++) {
	TclArithSeriesObjIndex(arithSeriesPtr, i, &elemObj);
	elem = TclGetStringFromObj(elemObj, &slen);
	Tcl_DecrRefCount(elemObj);
	slen += 1; /* + 1 is for the space or the nul-term */
	length += slen;
    }

    /*
     * Pass 2: generate the string repr.
     */

    p = Tcl_InitStringRep(arithSeriesPtr, NULL, length);
    for (i = 0; i < arithSeriesRepPtr->len; i++) {
	TclArithSeriesObjIndex(arithSeriesPtr, i, &elemObj);
	elem = TclGetStringFromObj(elemObj, &slen);
	strcpy(p, elem);
	p[slen] = ' ';
	p += slen+1;
	Tcl_DecrRefCount(elemObj);
    }
    if (length > 0) arithSeriesPtr->bytes[length-1] = '\0';
    arithSeriesPtr->length = length-1;
}

/*
 *----------------------------------------------------------------------
 *
 * SetArithSeriesFromAny --
 *
 * 	The Arithmetic Series object is just an way to optimize
 * 	Lists space complexity, so no one should try to convert
 * 	a string to an Arithmetic Series object.
 *
 * 	This function is here just to populate the Type structure.
 *
 * Results:
 *
 * 	The result is always TCL_ERROR. But see Side Effects.
 *
 * Side effects:
 *
 * 	Tcl Panic if called.
 *
 *----------------------------------------------------------------------
 */

static int
SetArithSeriesFromAny(
    TCL_UNUSED(Tcl_Interp *),		/* Used for error reporting if not NULL. */
    TCL_UNUSED(Tcl_Obj *))		/* The object to convert. */
{
    Tcl_Panic("SetArithSeriesFromAny: should never be called");
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclArithSeriesObjCopy --
 *
 *	Makes a "pure arithSeries" copy of an ArithSeries value. This provides for the C
 *	level a counterpart of the [lrange $list 0 end] command, while using
 *	internals details to be as efficient as possible.
 *
 * Results:
 *
 *	Normally returns a pointer to a new Tcl_Obj, that contains the same
 *	arithSeries value as *arithSeriesPtr does. The returned Tcl_Obj has a
 *	refCount of zero. If *arithSeriesPtr does not hold an arithSeries,
 *	NULL is returned, and if interp is non-NULL, an error message is
 *	recorded there.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclArithSeriesObjCopy(
    Tcl_Interp *interp,		/* Used to report errors if not NULL. */
    Tcl_Obj *arithSeriesPtr)	/* List object for which an element array is
				 * to be returned. */
{
    Tcl_Obj *copyPtr;
    ArithSeries *arithSeriesRepPtr;

    ArithSeriesGetInternalRep(arithSeriesPtr, arithSeriesRepPtr);
    if (NULL == arithSeriesRepPtr) {
	if (SetArithSeriesFromAny(interp, arithSeriesPtr) != TCL_OK) {
	    /* We know this is going to panic, but it's the message we want */
	    return NULL;
	}
    }

    TclNewObj(copyPtr);
    TclInvalidateStringRep(copyPtr);
    DupArithSeriesInternalRep(arithSeriesPtr, copyPtr);
    return copyPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclArithSeriesObjRange --
 *
 *	Makes a slice of an ArithSeries value.
 *      *arithSeriesPtr must be known to be a valid list.
 *
 * Results:
 *	Returns a pointer to the sliced series.
 *      This may be a new object or the same object if not shared.
 *
 * Side effects:
 *	?The possible conversion of the object referenced by listPtr?
 *	?to a list object.?
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclArithSeriesObjRange(
    Tcl_Obj *arithSeriesPtr,	/* List object to take a range from. */
    int fromIdx,		/* Index of first element to include. */
    int toIdx)			/* Index of last element to include. */
{
    ArithSeries *arithSeriesRepPtr;
    Tcl_Obj *startObj, *endObj, *stepObj;

    ArithSeriesGetInternalRep(arithSeriesPtr, arithSeriesRepPtr);

    if (fromIdx < 0) {
	fromIdx = 0;
    }
    if (fromIdx > toIdx) {
	Tcl_Obj *obj;
	TclNewObj(obj);
	return obj;
    }

    TclArithSeriesObjIndex(arithSeriesPtr, fromIdx, &startObj);
    Tcl_IncrRefCount(startObj);
    TclArithSeriesObjIndex(arithSeriesPtr, toIdx, &endObj);
    Tcl_IncrRefCount(endObj);
    TclArithSeriesObjStep(arithSeriesPtr, &stepObj);
    Tcl_IncrRefCount(stepObj);

    if (Tcl_IsShared(arithSeriesPtr) ||
	    ((arithSeriesPtr->refCount > 1))) {
	Tcl_Obj *newSlicePtr = TclNewArithSeriesObj(arithSeriesRepPtr->isDouble,
	           startObj, endObj, stepObj, NULL);
	Tcl_DecrRefCount(startObj);
	Tcl_DecrRefCount(endObj);
	Tcl_DecrRefCount(stepObj);
	return newSlicePtr;
    }

    /*
     * In-place is possible.
     */

    /*
     * Even if nothing below causes any changes, we still want the
     * string-canonizing effect of [lrange 0 end].
     */

    TclInvalidateStringRep(arithSeriesPtr);

    if (arithSeriesRepPtr->isDouble) {
	ArithSeriesDbl *arithSeriesDblRepPtr = (ArithSeriesDbl*)arithSeriesPtr;
	double start, end, step;
	Tcl_GetDoubleFromObj(NULL, startObj, &start);
	Tcl_GetDoubleFromObj(NULL, endObj, &end);
	Tcl_GetDoubleFromObj(NULL, stepObj, &step);
	arithSeriesDblRepPtr->start = start;
	arithSeriesDblRepPtr->end = end;
	arithSeriesDblRepPtr->step = step;
	arithSeriesDblRepPtr->len = (end-start+step)/step;
	arithSeriesDblRepPtr->elements = NULL;

    } else {
	Tcl_WideInt start, end, step;
	Tcl_GetWideIntFromObj(NULL, startObj, &start);
	Tcl_GetWideIntFromObj(NULL, endObj, &end);
	Tcl_GetWideIntFromObj(NULL, stepObj, &step);
	arithSeriesRepPtr->start = start;
	arithSeriesRepPtr->end = end;
	arithSeriesRepPtr->step = step;
	arithSeriesRepPtr->len = (end-start+step)/step;
	arithSeriesRepPtr->elements = NULL;
    }

    Tcl_DecrRefCount(startObj);
    Tcl_DecrRefCount(endObj);
    Tcl_DecrRefCount(stepObj);

    return arithSeriesPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclArithSeriesGetElements --
 *
 *	This function returns an (objc,objv) array of the elements in a list
 *	object.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case *objcPtr is set to
 *	the count of list elements and *objvPtr is set to a pointer to an
 *	array of (*objcPtr) pointers to each list element. If listPtr does not
 *	refer to an Abstract List object and the object can not be converted
 *	to one, TCL_ERROR is returned and an error message will be left in the
 *	interpreter's result if interp is not NULL.
 *
 *	The objects referenced by the returned array should be treated as
 *	readonly and their ref counts are _not_ incremented; the caller must
 *	do that if it holds on to a reference. Furthermore, the pointer and
 *	length returned by this function may change as soon as any function is
 *	called on the list object; be careful about retaining the pointer in a
 *	local data structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclArithSeriesGetElements(
    Tcl_Interp *interp,		/* Used to report errors if not NULL. */
    Tcl_Obj *objPtr,		/* AbstractList object for which an element
				 * array is to be returned. */
    int *objcPtr,		/* Where to store the count of objects
				 * referenced by objv. */
    Tcl_Obj ***objvPtr)		/* Where to store the pointer to an array of
				 * pointers to the list's objects. */
{
    if (TclHasInternalRep(objPtr,&tclArithSeriesType)) {
	ArithSeries *arithSeriesRepPtr;
	Tcl_Obj **objv;
	int i, objc;

	ArithSeriesGetInternalRep(objPtr, arithSeriesRepPtr);
	objc = arithSeriesRepPtr->len;
	if (objc > 0) {
	    if (arithSeriesRepPtr->elements) {
		/* If this exists, it has already been populated */
		objv = arithSeriesRepPtr->elements;
	    } else {
		/* Construct the elements array */
		objv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj*) * objc);
		if (objv == NULL) {
		    if (interp) {
			Tcl_SetObjResult(
			    interp,
			    Tcl_NewStringObj("max length of a Tcl list exceeded", -1));
			Tcl_SetErrorCode(interp, "TCL", "MEMORY", NULL);
		    }
		    return TCL_ERROR;
		}
		arithSeriesRepPtr->elements = objv;
		for (i = 0; i < objc; i++) {
		    if (TclArithSeriesObjIndex(objPtr, i, &objv[i]) != TCL_OK) {
			if (interp) {
			    Tcl_SetObjResult(
				interp,
				Tcl_NewStringObj("indexing error", -1));
			    Tcl_SetErrorCode(interp, "TCL", "MEMORY", NULL);
			}
			return TCL_ERROR;
		    }
		    Tcl_IncrRefCount(objv[i]);
		}
	    }
	} else {
	    objv = NULL;
	}
	*objvPtr = objv;
	*objcPtr = objc;
    } else {
	if (interp != NULL) {
	    Tcl_SetObjResult(
		interp,
		Tcl_ObjPrintf("value is not an arithseries"));
	    Tcl_SetErrorCode(interp, "TCL", "VALUE", "UNKNOWN", NULL);
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclArithSeriesObjReverse --
 *
 *	Reverse the order of the ArithSeries value.
 *      *arithSeriesPtr must be known to be a valid list.
 *
 * Results:
 *	Returns a pointer to the reordered series.
 *      This may be a new object or the same object if not shared.
 *
 * Side effects:
 *	?The possible conversion of the object referenced by listPtr?
 *	?to a list object.?
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclArithSeriesObjReverse(
    Tcl_Obj *arithSeriesPtr)	/* List object to reverse. */
{
    ArithSeries *arithSeriesRepPtr;
    Tcl_Obj *startObj, *endObj, *stepObj;
    Tcl_Obj *resultObj;
    Tcl_WideInt start, end, step, len;
    double dstart, dend, dstep;
    int isDouble;

    ArithSeriesGetInternalRep(arithSeriesPtr, arithSeriesRepPtr);

    isDouble = arithSeriesRepPtr->isDouble;
    len = arithSeriesRepPtr->len;

    TclArithSeriesObjIndex(arithSeriesPtr, (len-1), &startObj);
    TclArithSeriesObjIndex(arithSeriesPtr, 0, &endObj);
    TclArithSeriesObjStep(arithSeriesPtr, &stepObj);

    if (isDouble) {
	Tcl_GetDoubleFromObj(NULL, startObj, &dstart);
	Tcl_GetDoubleFromObj(NULL, endObj, &dend);
	Tcl_GetDoubleFromObj(NULL, stepObj, &dstep);
	dstep = -dstep;
	TclSetDoubleObj(stepObj, dstep);
    } else {
	Tcl_GetWideIntFromObj(NULL, startObj, &start);
	Tcl_GetWideIntFromObj(NULL, endObj, &end);
	Tcl_GetWideIntFromObj(NULL, stepObj, &step);
	step = -step;
	TclSetIntObj(stepObj, step);
    }

    if (Tcl_IsShared(arithSeriesPtr) ||
	    ((arithSeriesPtr->refCount > 1))) {
	Tcl_Obj *lenObj = Tcl_NewWideIntObj(len);
	resultObj = TclNewArithSeriesObj(isDouble,
		        startObj, endObj, stepObj, lenObj);
	Tcl_DecrRefCount(lenObj);
    } else {

	/*
	 * In-place is possible.
	 */

	TclInvalidateStringRep(arithSeriesPtr);

	if (isDouble) {
	    ArithSeriesDbl *arithSeriesDblRepPtr =
		(ArithSeriesDbl*)arithSeriesRepPtr;
	    arithSeriesDblRepPtr->start = dstart;
	    arithSeriesDblRepPtr->end = dend;
	    arithSeriesDblRepPtr->step = dstep;
	} else {
	    arithSeriesRepPtr->start = start;
	    arithSeriesRepPtr->end = end;
	    arithSeriesRepPtr->step = step;
	}
	if (arithSeriesRepPtr->elements) {
	    Tcl_WideInt i;
	    for (i=0; i<len; i++) {
		Tcl_DecrRefCount(arithSeriesRepPtr->elements[i]);
	    }
	    ckfree((char*)arithSeriesRepPtr->elements);
	}
	arithSeriesRepPtr->elements = NULL;

	resultObj = arithSeriesPtr;
    }

    Tcl_DecrRefCount(startObj);
    Tcl_DecrRefCount(endObj);
    Tcl_DecrRefCount(stepObj);

    return resultObj;
}
