/* **********************************************
 * NRE internals   
 * **********************************************
 */

#define NRE_STACK_SIZE        100

/*
 * This is the main data struct for representing NR commands. It was
 * originally designed to fit in sizeof(Tcl_Obj) in order to exploit the
 * fastest memory allocator available. The current version completely changed
 * the memory management approach (stack vs linked list), but the struct was
 * kept as it proved to be a "good" fit.
 */

typedef struct NRE_callback {
    Tcl_NRPostProc *procPtr;
    ClientData data[4];
} NRE_callback;

typedef struct NRE_stack {
    struct NRE_callback items[NRE_STACK_SIZE];
    struct NRE_stack *next;
} NRE_stack;

/*
 * Inline versions of Tcl_NRAddCallback and friends
 */

#define TOP_CB(iPtr) (((Interp *)(iPtr))->execEnvPtr->callbackPtr)

#define TclNRAddCallback(interp,postProcPtr,data0,data1,data2,data3)	\
    do {								\
	NRE_callback *cbPtr;						\
	ALLOC_CB(interp, cbPtr);					\
	INIT_CB(cbPtr, postProcPtr,data0,data1,data2,data3);		\
    } while (0)

#define INIT_CB(cbPtr, postProcPtr,data0,data1,data2,data3)		\
    do {								\
	cbPtr->procPtr = (postProcPtr);					\
	cbPtr->data[0] = (ClientData)(data0);				\
	cbPtr->data[1] = (ClientData)(data1);				\
	cbPtr->data[2] = (ClientData)(data2);				\
	cbPtr->data[3] = (ClientData)(data3);				\
    } while (0)

#define POP_CB(interp, cbPtr)			\
    (cbPtr) = TOP_CB(interp)--

#define ALLOC_CB(interp, cbPtr)					\
    do {							\
	ExecEnv *eePtr = ((Interp *) interp)->execEnvPtr;	\
	NRE_stack *this = eePtr->NRStack;			\
								\
	if (eePtr->callbackPtr &&					\
		(eePtr->callbackPtr < &this->items[NRE_STACK_SIZE-1])) { \
	    (cbPtr) = ++eePtr->callbackPtr;				\
	} else {							\
	    (cbPtr) = TclNewCallback(interp);				\
	}								\
    } while (0)

#define FREE_CB(interp, cbPtr)

#define NEXT_CB(ptr) TclNextCallback(ptr)

MODULE_SCOPE NRE_callback *TclNewCallback(Tcl_Interp *interp);
MODULE_SCOPE NRE_callback *TclPopCallback(Tcl_Interp *interp);
MODULE_SCOPE NRE_callback *TclNextCallback(NRE_callback *ptr);
