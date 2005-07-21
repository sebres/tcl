/* 
 * tclThread.c --
 *
 *	This file implements Platform independent thread operations. Most of
 *	the real work is done in the platform dependent files.
 *
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclThread.c,v 1.12 2005/07/21 14:38:51 dkf Exp $
 */

#include "tclInt.h"

/*
 * There are three classes of synchronization objects: mutexes, thread data
 * keys, and condition variables. The following are used to record the memory
 * used for these objects so they can be finalized.
 *
 * These statics are guarded by the mutex in the caller of
 * TclRememberThreadData, e.g., TclpThreadDataKeyInit
 */

typedef struct {
    int num;		/* Number of objects remembered */
    int max;		/* Max size of the array */
    char **list;	/* List of pointers */
} SyncObjRecord;

static SyncObjRecord keyRecord = {0, 0, NULL};
static SyncObjRecord mutexRecord = {0, 0, NULL};
static SyncObjRecord condRecord = {0, 0, NULL};

/*
 * Prototypes of functions used only in this file.
 */
 
static void		RememberSyncObject _ANSI_ARGS_((char *objPtr,
			    SyncObjRecord *recPtr));
static void		ForgetSyncObject _ANSI_ARGS_((char *objPtr,
			    SyncObjRecord *recPtr));

/* 
 * Several functions are #defined to nothing in tcl.h if TCL_THREADS is not
 * specified. Here we undo that so the functions are defined in the stubs
 * table.
 */

#ifndef TCL_THREADS
#undef Tcl_MutexLock
#undef Tcl_MutexUnlock
#undef Tcl_MutexFinalize
#undef Tcl_ConditionNotify
#undef Tcl_ConditionWait
#undef Tcl_ConditionFinalize
#endif


/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetThreadData --
 *
 *	This function allocates and initializes a chunk of thread local
 *	storage.
 *
 * Results:
 *	A thread-specific pointer to the data structure.
 *
 * Side effects:
 *	Will allocate memory the first time this thread calls for this chunk
 *	of storage.
 *
 *----------------------------------------------------------------------
 */

VOID *
Tcl_GetThreadData(keyPtr, size)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk */
    int size;			/* Size of storage block */
{
    VOID *result;
#ifdef TCL_THREADS

    /*
     * See if this is the first thread to init this key.
     */

    if (*keyPtr == NULL) {
#ifdef USE_THREAD_STORAGE
	TclThreadStorageDataKeyInit(keyPtr);
#else
	TclpThreadDataKeyInit(keyPtr);
#endif /* USE_THREAD_STORAGE */
    }

    /*
     * Initialize the key for this thread.
     */
#ifdef USE_THREAD_STORAGE
    result = TclThreadStorageDataKeyGet(keyPtr);
#else
    result = TclpThreadDataKeyGet(keyPtr);
#endif /* USE_THREAD_STORAGE */

    if (result == NULL) {
	result  = (VOID *)ckalloc((size_t)size);
	memset(result, 0, (size_t)size);
#ifdef USE_THREAD_STORAGE
	TclThreadStorageDataKeySet(keyPtr, result);
#else
	TclpThreadDataKeySet(keyPtr, result);
#endif /* USE_THREAD_STORAGE */
    }
#else /* TCL_THREADS */
    if (*keyPtr == NULL) {
	result = (VOID *)ckalloc((size_t)size);
	memset((char *)result, 0, (size_t)size);
	*keyPtr = (Tcl_ThreadDataKey)result;
	TclRememberDataKey(keyPtr);
    }
    result = *(VOID **)keyPtr;
#endif /* TCL_THREADS */
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadDataKeyGet --
 *
 *	This function returns a pointer to a block of thread local storage.
 *
 * Results:
 *	A thread-specific pointer to the data structure, or NULL if the memory
 *	has not been assigned to this key for this thread.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

VOID *
TclThreadDataKeyGet(keyPtr)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk, really
				 * (pthread_key_t **) */
{
#ifdef TCL_THREADS
#ifdef USE_THREAD_STORAGE
    return (VOID *)TclThreadStorageDataKeyGet(keyPtr);
#else
    return (VOID *)TclpThreadDataKeyGet(keyPtr);
#endif /* USE_THREAD_STORAGE */
#else /* TCL_THREADS */
    char *result = *(char **)keyPtr;
    return (VOID *)result;
#endif /* TCL_THREADS */
}


/*
 *----------------------------------------------------------------------
 *
 * TclThreadDataKeySet --
 *
 *	This function sets a thread local storage pointer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The assigned value will be returned by TclpThreadDataKeyGet.
 *
 *----------------------------------------------------------------------
 */

void
TclThreadDataKeySet(keyPtr, data)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk, really
				 * (pthread_key_t **) */
    VOID *data;			/* Thread local storage */
{
#ifdef TCL_THREADS

    if (*keyPtr == NULL) {
#ifdef USE_THREAD_STORAGE
	TclThreadStorageDataKeyInit(keyPtr);
#else
	TclpThreadDataKeyInit(keyPtr);
#endif /* USE_THREAD_STORAGE */
    }

#ifdef USE_THREAD_STORAGE
    TclThreadStorageDataKeySet(keyPtr, data);
#else
    TclpThreadDataKeySet(keyPtr, data);
#endif /* USE_THREAD_STORAGE */

#else /* TCL_THREADS */
    *keyPtr = (Tcl_ThreadDataKey) data;
#endif /* TCL_THREADS */
}

/*
 *----------------------------------------------------------------------
 *
 * RememberSyncObject
 *
 *	Keep a list of (mutexes/condition variable/data key) used during
 *	finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Add to the appropriate list.
 *
 *----------------------------------------------------------------------
 */

static void
RememberSyncObject(objPtr, recPtr)
    char *objPtr;		/* Pointer to sync object */
    SyncObjRecord *recPtr;	/* Record of sync objects */
{
    char **newList;
    int i, j;

    /*
     * Save the pointer to the allocated object so it can be finalized. Grow
     * the list of pointers if necessary, copying only non-NULL pointers to
     * the new list.
     */

    if (recPtr->num >= recPtr->max) {
	recPtr->max += 8;
	newList = (char **)ckalloc(recPtr->max * sizeof(char *));
	for (i=0,j=0 ; i<recPtr->num ; i++) {
	    if (recPtr->list[i] != NULL) {
		newList[j++] = recPtr->list[i];
	    }
	}
	if (recPtr->list != NULL) {
	    ckfree((char *)recPtr->list);
	}
	recPtr->list = newList;
	recPtr->num = j;
    }
    recPtr->list[recPtr->num] = objPtr;
    recPtr->num++;
}

/*
 *----------------------------------------------------------------------
 *
 * ForgetSyncObject
 *
 *	Remove a single object from the list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Remove from the appropriate list.
 *
 *----------------------------------------------------------------------
 */

static void
ForgetSyncObject(objPtr, recPtr)
    char *objPtr;		/* Pointer to sync object */
    SyncObjRecord *recPtr;	/* Record of sync objects */
{
    int i;

    for (i=0 ; i<recPtr->num ; i++) {
	if (objPtr == recPtr->list[i]) {
	    recPtr->list[i] = NULL;
	    return;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclRememberMutex
 *
 *	Keep a list of mutexes used during finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Add to the mutex list.
 *
 *----------------------------------------------------------------------
 */

void
TclRememberMutex(mutexPtr)
    Tcl_Mutex *mutexPtr;
{
    RememberSyncObject((char *)mutexPtr, &mutexRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MutexFinalize --
 *
 *	Finalize a single mutex and remove it from the list of remembered
 *	objects.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Remove the mutex from the list.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_MutexFinalize(mutexPtr)
    Tcl_Mutex *mutexPtr;
{
#ifdef TCL_THREADS
    TclpFinalizeMutex(mutexPtr);
#endif
    ForgetSyncObject((char *)mutexPtr, &mutexRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * TclRememberDataKey
 *
 *	Keep a list of thread data keys used during finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Add to the key list.
 *
 *----------------------------------------------------------------------
 */

void
TclRememberDataKey(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    RememberSyncObject((char *)keyPtr, &keyRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * TclRememberCondition
 *
 *	Keep a list of condition variables used during finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Add to the condition variable list.
 *
 *----------------------------------------------------------------------
 */

void
TclRememberCondition(condPtr)
    Tcl_Condition *condPtr;
{
    RememberSyncObject((char *)condPtr, &condRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConditionFinalize --
 *
 *	Finalize a single condition variable and remove it from the list of
 *	remembered objects.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Remove the condition variable from the list.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ConditionFinalize(condPtr)
    Tcl_Condition *condPtr;
{
#ifdef TCL_THREADS
    TclpFinalizeCondition(condPtr);
#endif
    ForgetSyncObject((char *)condPtr, &condRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeThreadData --
 *
 *	This function cleans up the thread-local storage. This is called once
 *	for each thread.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees up all thread local storage.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeThreadData()
{
    int i;

    TclpMasterLock();
    for (i=0 ; i<keyRecord.num ; i++) {
	Tcl_ThreadDataKey *keyPtr = (Tcl_ThreadDataKey *) keyRecord.list[i];

#ifdef TCL_THREADS
#ifdef USE_THREAD_STORAGE
	TclFinalizeThreadStorageData(keyPtr);
#else
	TclpFinalizeThreadData(keyPtr);
#endif /* USE_THREAD_STORAGE */
#else /* TCL_THREADS */
	if (*keyPtr != NULL) {
	    ckfree((char *)*keyPtr);
	    *keyPtr = NULL;
	}
#endif /* TCL_THREADS */
    }
    TclpMasterUnlock();
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeSynchronization --
 *
 *	This function cleans up all synchronization objects: mutexes,
 *	condition variables, and thread-local storage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees up the memory.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeSynchronization()
{
#ifdef TCL_THREADS
    Tcl_ThreadDataKey *keyPtr;
    Tcl_Mutex *mutexPtr;
    Tcl_Condition *condPtr;
    int i;

    TclpMasterLock();
    for (i=0 ; i<keyRecord.num ; i++) {
	keyPtr = (Tcl_ThreadDataKey *)keyRecord.list[i];
#ifdef USE_THREAD_STORAGE
	TclFinalizeThreadStorageDataKey(keyPtr);
#else
	TclpFinalizeThreadDataKey(keyPtr);
#endif /* USE_THREAD_STORAGE */
    }
    if (keyRecord.list != NULL) {
	ckfree((char *)keyRecord.list);
	keyRecord.list = NULL;
    }
    keyRecord.max = 0;
    keyRecord.num = 0;

    /*
     * Call platform specific thread storage master cleanup.
     */

#ifdef USE_THREAD_STORAGE
    TclFinalizeThreadStorage();
#endif /* USE_THREAD_STORAGE */

    for (i=0 ; i<mutexRecord.num ; i++) {
	mutexPtr = (Tcl_Mutex *)mutexRecord.list[i];
	if (mutexPtr != NULL) {
	    TclpFinalizeMutex(mutexPtr);
	}
    }
    if (mutexRecord.list != NULL) {
	ckfree((char *)mutexRecord.list);
	mutexRecord.list = NULL;
    }
    mutexRecord.max = 0;
    mutexRecord.num = 0;

    for (i=0 ; i<condRecord.num ; i++) {
	condPtr = (Tcl_Condition *)condRecord.list[i];
	if (condPtr != NULL) {
	    TclpFinalizeCondition(condPtr);
	}
    }
    if (condRecord.list != NULL) {
	ckfree((char *)condRecord.list);
	condRecord.list = NULL;
    }
    condRecord.max = 0;
    condRecord.num = 0;

    TclpMasterUnlock();
#else /* TCL_THREADS */
    if (keyRecord.list != NULL) {
	ckfree((char *)keyRecord.list);
	keyRecord.list = NULL;
    }
    keyRecord.max = 0;
    keyRecord.num = 0;
#endif /* TCL_THREADS */
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ExitThread --
 *
 *	This function is called to terminate the current thread. This should
 *	be used by extensions that create threads with additional interpreters
 *	in them.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All thread exit handlers are invoked, then the thread dies.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ExitThread(status)
    int status;
{
    Tcl_FinalizeThread();
#ifdef TCL_THREADS
    TclpThreadExit(status);
#endif
}

#ifndef TCL_THREADS

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConditionWait, et al. --
 *
 *	These noop functions are provided so the stub table does not have to
 *	be conditionalized for threads. The real implementations of these
 *	functions live in the platform specific files.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#undef Tcl_ConditionWait
void
Tcl_ConditionWait(condPtr, mutexPtr, timePtr)
    Tcl_Condition *condPtr;	/* Really (pthread_cond_t **) */
    Tcl_Mutex *mutexPtr;	/* Really (pthread_mutex_t **) */
    Tcl_Time *timePtr;		/* Timeout on waiting period */
{
}

#undef Tcl_ConditionNotify
void
Tcl_ConditionNotify(condPtr)
    Tcl_Condition *condPtr;
{
}

#undef Tcl_MutexLock
void
Tcl_MutexLock(mutexPtr)
    Tcl_Mutex *mutexPtr;
{
}

#undef Tcl_MutexUnlock
void
Tcl_MutexUnlock(mutexPtr)
    Tcl_Mutex *mutexPtr;
{
}
#endif /* !TCL_THREADS */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
