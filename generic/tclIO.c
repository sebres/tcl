/* 
 * tclIO.c --
 *
 *	This file provides the generic portions (those that are the same on
 *	all platforms and for all channel types) of Tcl's IO facilities.
 *
 * Copyright (c) 1998 Scriptics Corporation
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclIO.c,v 1.20.2.3.2.4 2000/07/14 04:16:12 hobbs Exp $
 */

#include "tclInt.h"
#include "tclPort.h"


/*
 * Forward declarations of internal procedures.
 * First the driver procedures of the transformation.
 */

static int		TransformBlockModeProc _ANSI_ARGS_ ((
				ClientData instanceData, int mode));
static int		TransformCloseProc _ANSI_ARGS_ ((
				ClientData instanceData, Tcl_Interp* interp));
static int		TransformInputProc _ANSI_ARGS_ ((
				ClientData instanceData,
				char* buf, int toRead, int* errorCodePtr));
static int		TransformOutputProc _ANSI_ARGS_ ((
				ClientData instanceData,
				char*  buf, int toWrite, int* errorCodePtr));
static int		TransformSeekProc _ANSI_ARGS_ ((
				ClientData instanceData, long offset,
				int mode, int* errorCodePtr));
static int		TransformSetOptionProc _ANSI_ARGS_((
				ClientData instanceData, Tcl_Interp *interp,
				char *optionName, char *value));
static int		TransformGetOptionProc _ANSI_ARGS_((
				ClientData instanceData, Tcl_Interp *interp,
				char *optionName, Tcl_DString *dsPtr));
static void		TransformWatchProc _ANSI_ARGS_ ((
				ClientData instanceData, int mask));
static int		TransformGetFileHandleProc _ANSI_ARGS_ ((
				ClientData instanceData, int direction,
				ClientData* handlePtr));
static int		TransformNotifyProc _ANSI_ARGS_ ((
				ClientData instanceData, int mask));

/*
 * Forward declarations of internal procedures.
 * Secondly the procedures for handling and generating fileeevents.
 */

static void		TransformChannelHandlerTimer _ANSI_ARGS_ ((
				ClientData clientData));

/*
 * Forward declarations of internal procedures.
 * Third, helper procedures encapsulating essential tasks.
 */

typedef struct TransformChannelData TransformChannelData;

static int		ExecuteCallback _ANSI_ARGS_ ((
				TransformChannelData* ctrl, Tcl_Interp* interp,
				unsigned char* op, unsigned char* buf,
				int bufLen, int transmit, int preserve));

/*
 * Action codes to give to 'ExecuteCallback' (argument 'transmit')
 * confering to the procedure what to do with the result of the script
 * it calls.
 */

#define TRANSMIT_DONT  (0) /* No transfer to do */
#define TRANSMIT_DOWN  (1) /* Transfer to the underlying channel */
#define TRANSMIT_SELF  (2) /* Transfer into our channel. */
#define TRANSMIT_IBUF  (3) /* Transfer to internal input buffer */
#define TRANSMIT_NUM   (4) /* Transfer number to 'maxRead' */

/*
 * Codes for 'preserve' of 'ExecuteCallback'
 */

#define P_PRESERVE    (1)
#define P_NO_PRESERVE (0)

/*
 * Strings for the action codes delivered to the script implementing
 * a transformation. Argument 'op' of 'ExecuteCallback'.
 */

#define A_CREATE_WRITE	(UCHARP ("create/write"))
#define A_DELETE_WRITE	(UCHARP ("delete/write"))
#define A_FLUSH_WRITE	(UCHARP ("flush/write"))
#define A_WRITE		(UCHARP ("write"))

#define A_CREATE_READ	(UCHARP ("create/read"))
#define A_DELETE_READ	(UCHARP ("delete/read"))
#define A_FLUSH_READ	(UCHARP ("flush/read"))
#define A_READ		(UCHARP ("read"))

#define A_QUERY_MAXREAD (UCHARP ("query/maxRead"))
#define A_CLEAR_READ	(UCHARP ("clear/read"))

/*
 * Management of a simple buffer.
 */

typedef struct ResultBuffer ResultBuffer;

static void		ResultClear  _ANSI_ARGS_ ((ResultBuffer* r));
static void		ResultInit   _ANSI_ARGS_ ((ResultBuffer* r));
static int		ResultLength _ANSI_ARGS_ ((ResultBuffer* r));
static int		ResultCopy   _ANSI_ARGS_ ((ResultBuffer* r,
				unsigned char* buf, int toRead));
static void		ResultAdd    _ANSI_ARGS_ ((ResultBuffer* r,
				unsigned char* buf, int toWrite));

/*
 * This structure describes the channel type structure for tcl based
 * transformations.
 */

static Tcl_ChannelType transformChannelType = {
    "transform",			/* Type name. */
    TCL_CHANNEL_VERSION_2,
    TransformCloseProc,			/* Close proc. */
    TransformInputProc,			/* Input proc. */
    TransformOutputProc,		/* Output proc. */
    TransformSeekProc,			/* Seek proc. */
    TransformSetOptionProc,		/* Set option proc. */
    TransformGetOptionProc,		/* Get option proc. */
    TransformWatchProc,			/* Initialize notifier. */
    TransformGetFileHandleProc,		/* Get OS handles out of channel. */
    NULL,				/* close2proc */
    TransformBlockModeProc,		/* Set blocking/nonblocking mode.*/
    NULL,				/* Flush proc. */
    TransformNotifyProc,                /* Handling of events bubbling up */
};

/*
 * Possible values for 'flags' field in control structure, see below.
 */

#define CHANNEL_ASYNC		(1<<0) /* non-blocking mode */

/*
 * Definition of the structure containing the information about the
 * internal input buffer.
 */

struct ResultBuffer {
    unsigned char* buf;       /* Reference to the buffer area */
    int            allocated; /* Allocated size of the buffer area */
    int            used;      /* Number of bytes in the buffer, <= allocated */
};

/*
 * Additional bytes to allocate during buffer expansion
 */

#define INCREMENT (512)

/*
 * Number of milliseconds to wait before firing an event to flush
 * out information waiting in buffers (fileevent support).
 */

#define DELAY (5)

/*
 * Convenience macro to make some casts easier to use.
 */

#define UCHARP(x) ((unsigned char*) (x))
#define NO_INTERP ((Tcl_Interp*) NULL)

/*
 * Definition of a structure used by all transformations generated here to
 * maintain their local state.
 */

struct TransformChannelData {

    /*
     * General section. Data to integrate the transformation into the channel
     * system.
     */

    Tcl_Channel self;     /* Our own Channel handle */
    int readIsFlushed;    /* Flag to note wether in.flushProc was called or not
			   */
    int flags;            /* Currently CHANNEL_ASYNC or zero */
    int watchMask;        /* Current watch/event/interest mask */
    int mode;             /* mode of parent channel, OR'ed combination of
			   * TCL_READABLE, TCL_WRITABLE */
    Tcl_TimerToken timer; /* Timer for automatic flushing of information
			   * sitting in an internal buffer. Required for full
			   * fileevent support */
    /*
     * Transformation specific data.
     */

    int maxRead;            /* Maximum allowed number of bytes to read, as
			     * given to us by the tcl script implementing the
			     * transformation. */
    Tcl_Interp*    interp;  /* Reference to the interpreter which created the
			     * transformation. Used to execute the code
			     * below. */
    Tcl_Obj*       command; /* Tcl code to execute for a buffer */
    ResultBuffer   result;  /* Internal buffer used to store the result of a
			     * transformation of incoming data. Additionally
			     * serves as buffer of all data not yet consumed by
			     * the reader. */
};
/*
 * END OF IOGT STUFF
 */

/*
 * Make sure that both EAGAIN and EWOULDBLOCK are defined. This does not
 * compile on systems where neither is defined. We want both defined so
 * that we can test safely for both. In the code we still have to test for
 * both because there may be systems on which both are defined and have
 * different values.
 */

#if ((!defined(EWOULDBLOCK)) && (defined(EAGAIN)))
#   define EWOULDBLOCK EAGAIN
#endif
#if ((!defined(EAGAIN)) && (defined(EWOULDBLOCK)))
#   define EAGAIN EWOULDBLOCK
#endif
#if ((!defined(EAGAIN)) && (!defined(EWOULDBLOCK)))
error one of EWOULDBLOCK or EAGAIN must be defined
#endif

/*
 * The following structure encapsulates the state for a background channel
 * copy.  Note that the data buffer for the copy will be appended to this
 * structure.
 */

typedef struct CopyState {
    struct Channel *readPtr;	/* Pointer to input channel. */
    struct Channel *writePtr;	/* Pointer to output channel. */
    int readFlags;		/* Original read channel flags. */
    int writeFlags;		/* Original write channel flags. */
    int toRead;			/* Number of bytes to copy, or -1. */
    int total;			/* Total bytes transferred (written). */
    Tcl_Interp *interp;		/* Interp that started the copy. */
    Tcl_Obj *cmdPtr;		/* Command to be invoked at completion. */
    int bufSize;		/* Size of appended buffer. */
    char buffer[1];		/* Copy buffer, this must be the last
				 * field. */
} CopyState;

/*
 * struct ChannelBuffer:
 *
 * Buffers data being sent to or from a channel.
 */

typedef struct ChannelBuffer {
    int nextAdded;		/* The next position into which a character
                                 * will be put in the buffer. */
    int nextRemoved;		/* Position of next byte to be removed
                                 * from the buffer. */
    int bufLength;		/* How big is the buffer? */
    struct ChannelBuffer *nextPtr;
    				/* Next buffer in chain. */
    char buf[4];		/* Placeholder for real buffer. The real
                                 * buffer occuppies this space + bufSize-4
                                 * bytes. This must be the last field in
                                 * the structure. */
} ChannelBuffer;

#define CHANNELBUFFER_HEADER_SIZE	(sizeof(ChannelBuffer) - 4)

/*
 * How much extra space to allocate in buffer to hold bytes from previous
 * buffer (when converting to UTF-8) or to hold bytes that will go to
 * next buffer (when converting from UTF-8).
 */
 
#define BUFFER_PADDING	    16
 
/*
 * The following defines the *default* buffer size for channels.
 */

#define CHANNELBUFFER_DEFAULT_SIZE	(1024 * 4)

/*
 * Structure to record a close callback. One such record exists for
 * each close callback registered for a channel.
 */

typedef struct CloseCallback {
    Tcl_CloseProc *proc;		/* The procedure to call. */
    ClientData clientData;		/* Arbitrary one-word data to pass
					 * to the callback. */
    struct CloseCallback *nextPtr;	/* For chaining close callbacks. */
} CloseCallback;

/*
 * The following structure describes the information saved from a call to
 * "fileevent". This is used later when the event being waited for to
 * invoke the saved script in the interpreter designed in this record.
 */

typedef struct EventScriptRecord {
    struct Channel *chanPtr;	/* The channel for which this script is
				 * registered. This is used only when an
				 * error occurs during evaluation of the
				 * script, to delete the handler. */
    Tcl_Obj *scriptPtr;		/* Script to invoke. */
    Tcl_Interp *interp;		/* In what interpreter to invoke script? */
    int mask;			/* Events must overlap current mask for the
				 * stored script to be invoked. */
    struct EventScriptRecord *nextPtr;
				/* Next in chain of records. */
} EventScriptRecord;

/*
 * struct Channel:
 *
 * One of these structures is allocated for each open channel. It contains data
 * specific to the channel but which belongs to the generic part of the Tcl
 * channel mechanism, and it points at an instance specific (and type
 * specific) * instance data, and at a channel type structure.
 */

typedef struct Channel {
    struct ChannelState *state; /* Split out state information */

    ClientData instanceData;	/* Instance-specific data provided by
				 * creator of channel. */
    Tcl_ChannelType *typePtr;	/* Pointer to channel type structure. */

    struct Channel *downChanPtr;/* Refers to channel this one was stacked
				 * upon.  This reference is NULL for normal
				 * channels.  See Tcl_StackChannel. */
    struct Channel *upChanPtr;	/* Refers to the channel above stacked this
				 * one. NULL for the top most channel. */
} Channel;

/*
 * struct ChannelState:
 *
 * One of these structures is allocated for each open channel. It contains data
 * specific to the channel but which belongs to the generic part of the Tcl
 * channel mechanism, and it points at an instance specific (and type
 * specific) * instance data, and at a channel type structure.
 */

typedef struct ChannelState {
    char *channelName;		/* The name of the channel instance in Tcl
				 * commands. Storage is owned by the generic IO
				 * code, is dynamically allocated. */
    int	flags;			/* ORed combination of the flags defined
				 * below. */
    Tcl_Encoding encoding;	/* Encoding to apply when reading or writing
				 * data on this channel.  NULL means no
				 * encoding is applied to data. */
    Tcl_EncodingState inputEncodingState;
				/* Current encoding state, used when converting
				 * input data bytes to UTF-8. */
    int inputEncodingFlags;	/* Encoding flags to pass to conversion
				 * routine when converting input data bytes to
				 * UTF-8.  May be TCL_ENCODING_START before
				 * converting first byte and TCL_ENCODING_END
				 * when EOF is seen. */
    Tcl_EncodingState outputEncodingState;
				/* Current encoding state, used when converting
				 * UTF-8 to output data bytes. */
    int outputEncodingFlags;	/* Encoding flags to pass to conversion
				 * routine when converting UTF-8 to output
				 * data bytes.  May be TCL_ENCODING_START
				 * before converting first byte and
				 * TCL_ENCODING_END when EOF is seen. */
    Tcl_EolTranslation inputTranslation;
				/* What translation to apply for end of line
				 * sequences on input? */    
    Tcl_EolTranslation outputTranslation;
				/* What translation to use for generating
				 * end of line sequences in output? */
    int inEofChar;		/* If nonzero, use this as a signal of EOF
				 * on input. */
    int outEofChar;             /* If nonzero, append this to the channel
				 * when it is closed if it is open for
				 * writing. */
    int unreportedError;	/* Non-zero if an error report was deferred
				 * because it happened in the background. The
				 * value is the POSIX error code. */
    int refCount;		/* How many interpreters hold references to
				 * this IO channel? */

    CloseCallback *closeCbPtr;	/* Callbacks registered to be called when the
				 * channel is closed. */
    char *outputStage;		/* Temporary staging buffer used when
				 * translating EOL before converting from
				 * UTF-8 to external form. */
    ChannelBuffer *curOutPtr;	/* Current output buffer being filled. */
    ChannelBuffer *outQueueHead;/* Points at first buffer in output queue. */
    ChannelBuffer *outQueueTail;/* Points at last buffer in output queue. */

    ChannelBuffer *saveInBufPtr;/* Buffer saved for input queue - eliminates
				 * need to allocate a new buffer for "gets"
				 * that crosses buffer boundaries. */
    ChannelBuffer *inQueueHead;	/* Points at first buffer in input queue. */
    ChannelBuffer *inQueueTail;	/* Points at last buffer in input queue. */

    struct ChannelHandler *chPtr;/* List of channel handlers registered
				  * for this channel. */
    int interestMask;		/* Mask of all events this channel has
				 * handlers for. */
    EventScriptRecord *scriptRecordPtr;
				/* Chain of all scripts registered for
				 * event handlers ("fileevent") on this
				 * channel. */

    int bufSize;		/* What size buffers to allocate? */
    Tcl_TimerToken timer;	/* Handle to wakeup timer for this channel. */
    CopyState *csPtr;		/* State of background copy, or NULL. */
    Channel *topChanPtr;	/* Refers to topmost channel in a stack.
				 * Never NULL. */
    Channel *bottomChanPtr;	/* Refers to bottommost channel in a stack.
				 * This channel can be relied on to live as
				 * long as the channel state. Never NULL. */
    struct ChannelState *nextCSPtr;
				/* Next in list of channels currently open. */
} ChannelState;
    
/*
 * Values for the flags field in Channel. Any ORed combination of the
 * following flags can be stored in the field. These flags record various
 * options and state bits about the channel. In addition to the flags below,
 * the channel can also have TCL_READABLE (1<<1) and TCL_WRITABLE (1<<2) set.
 */

#define CHANNEL_NONBLOCKING	(1<<3)	/* Channel is currently in
					 * nonblocking mode. */
#define CHANNEL_LINEBUFFERED	(1<<4)	/* Output to the channel must be
					 * flushed after every newline. */
#define CHANNEL_UNBUFFERED	(1<<5)	/* Output to the channel must always
					 * be flushed immediately. */
#define BUFFER_READY		(1<<6)	/* Current output buffer (the
					 * curOutPtr field in the
                                         * channel structure) should be
                                         * output as soon as possible even
                                         * though it may not be full. */
#define BG_FLUSH_SCHEDULED	(1<<7)	/* A background flush of the
					 * queued output buffers has been
                                         * scheduled. */
#define CHANNEL_CLOSED		(1<<8)	/* Channel has been closed. No
					 * further Tcl-level IO on the
                                         * channel is allowed. */
#define CHANNEL_EOF		(1<<9)	/* EOF occurred on this channel.
					 * This bit is cleared before every
                                         * input operation. */
#define CHANNEL_STICKY_EOF	(1<<10)	/* EOF occurred on this channel because
					 * we saw the input eofChar. This bit
                                         * prevents clearing of the EOF bit
                                         * before every input operation. */
#define CHANNEL_BLOCKED		(1<<11)	/* EWOULDBLOCK or EAGAIN occurred
					 * on this channel. This bit is
					 * cleared before every input or
					 * output operation. */
#define INPUT_SAW_CR		(1<<12)	/* Channel is in CRLF eol input
					 * translation mode and the last
                                         * byte seen was a "\r". */
#define INPUT_NEED_NL		(1<<15)	/* Saw a '\r' at end of last buffer,
					 * and there should be a '\n' at
					 * beginning of next buffer. */
#define CHANNEL_DEAD		(1<<13)	/* The channel has been closed by
					 * the exit handler (on exit) but
                                         * not deallocated. When any IO
                                         * operation sees this flag on a
                                         * channel, it does not call driver
                                         * level functions to avoid referring
                                         * to deallocated data. */
#define CHANNEL_NEED_MORE_DATA	(1<<14)	/* The last input operation failed
					 * because there was not enough data
					 * to complete the operation.  This
					 * flag is set when gets fails to
					 * get a complete line or when read
					 * fails to get a complete character.
					 * When set, file events will not be
					 * delivered for buffered data until
					 * the state of the channel changes. */
#define CHANNEL_DISALLOW_RAW	(1<<16)	/* When set, causes the Raw API to
					 * delegate to the standard APIs
					 * instead */

/*
 * For each channel handler registered in a call to Tcl_CreateChannelHandler,
 * there is one record of the following type. All of records for a specific
 * channel are chained together in a singly linked list which is stored in
 * the channel structure.
 */

typedef struct ChannelHandler {
    Channel *chanPtr;		/* The channel structure for this channel. */
    int mask;			/* Mask of desired events. */
    Tcl_ChannelProc *proc;	/* Procedure to call in the type of
                                 * Tcl_CreateChannelHandler. */
    ClientData clientData;	/* Argument to pass to procedure. */
    struct ChannelHandler *nextPtr;
    				/* Next one in list of registered handlers. */
} ChannelHandler;

/*
 * This structure keeps track of the current ChannelHandler being invoked in
 * the current invocation of ChannelHandlerEventProc. There is a potential
 * problem if a ChannelHandler is deleted while it is the current one, since
 * ChannelHandlerEventProc needs to look at the nextPtr field. To handle this
 * problem, structures of the type below indicate the next handler to be
 * processed for any (recursively nested) dispatches in progress. The
 * nextHandlerPtr field is updated if the handler being pointed to is deleted.
 * The nextPtr field is used to chain together all recursive invocations, so
 * that Tcl_DeleteChannelHandler can find all the recursively nested
 * invocations of ChannelHandlerEventProc and compare the handler being
 * deleted against the NEXT handler to be invoked in that invocation; when it
 * finds such a situation, Tcl_DeleteChannelHandler updates the nextHandlerPtr
 * field of the structure to the next handler.
 */

typedef struct NextChannelHandler {
    ChannelHandler *nextHandlerPtr;	/* The next handler to be invoked in
                                         * this invocation. */
    struct NextChannelHandler *nestedHandlerPtr;
    /* Next nested invocation of
     * ChannelHandlerEventProc. */
} NextChannelHandler;


/*
 * The following structure describes the event that is added to the Tcl
 * event queue by the channel handler check procedure.
 */

typedef struct ChannelHandlerEvent {
    Tcl_Event header;		/* Standard header for all events. */
    Channel *chanPtr;		/* The channel that is ready. */
    int readyMask;		/* Events that have occurred. */
} ChannelHandlerEvent;

/*
 * The following structure is used by Tcl_GetsObj() to encapsulates the
 * state for a "gets" operation.
 */
 
typedef struct GetsState {
    Tcl_Obj *objPtr;		/* The object to which UTF-8 characters
				 * will be appended. */
    char **dstPtr;		/* Pointer into objPtr's string rep where
				 * next character should be stored. */
    Tcl_Encoding encoding;	/* The encoding to use to convert raw bytes
				 * to UTF-8.  */
    ChannelBuffer *bufPtr;	/* The current buffer of raw bytes being
				 * emptied. */
    Tcl_EncodingState state;	/* The encoding state just before the last
				 * external to UTF-8 conversion in
				 * FilterInputBytes(). */
    int rawRead;		/* The number of bytes removed from bufPtr
				 * in the last call to FilterInputBytes(). */
    int bytesWrote;		/* The number of bytes of UTF-8 data
				 * appended to objPtr during the last call to
				 * FilterInputBytes(). */
    int charsWrote;		/* The corresponding number of UTF-8
				 * characters appended to objPtr during the
				 * last call to FilterInputBytes(). */
    int totalChars;		/* The total number of UTF-8 characters
				 * appended to objPtr so far, just before the
				 * last call to FilterInputBytes(). */
} GetsState;

/*
 * All static variables used in this file are collected into a single
 * instance of the following structure.  For multi-threaded implementations,
 * there is one instance of this structure for each thread.
 *
 * Notice that different structures with the same name appear in other
 * files.  The structure defined below is used in this file only.
 */

typedef struct ThreadSpecificData {

    /*
     * This variable holds the list of nested ChannelHandlerEventProc 
     * invocations.
     */
    NextChannelHandler *nestedHandlerPtr;

    /*
     * List of all channels currently open, indexed by ChannelState,
     * as only one ChannelState exists per set of stacked channels.
     */
    ChannelState *firstCSPtr;
#ifdef oldcode
    /*
     * Has a channel exit handler been created yet?
     */
    int channelExitHandlerCreated;

    /*
     * Has the channel event source been created and registered with the
     * notifier?
     */
    int channelEventSourceCreated;
#endif
    /*
     * Static variables to hold channels for stdin, stdout and stderr.
     */
    Tcl_Channel stdinChannel;
    int stdinInitialized;
    Tcl_Channel stdoutChannel;
    int stdoutInitialized;
    Tcl_Channel stderrChannel;
    int stderrInitialized;

} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;


/*
 * Static functions in this file:
 */

static ChannelBuffer *	AllocChannelBuffer _ANSI_ARGS_((int length));
static void		ChannelEventScriptInvoker _ANSI_ARGS_((
				ClientData clientData, int flags));
static void		ChannelTimerProc _ANSI_ARGS_((
				ClientData clientData));
static int		CheckChannelErrors _ANSI_ARGS_((ChannelState *statePtr,
				int direction));
static int		CheckFlush _ANSI_ARGS_((Channel *chanPtr,
				ChannelBuffer *bufPtr, int newlineFlag));
static int		CheckForDeadChannel _ANSI_ARGS_((Tcl_Interp *interp,
				ChannelState *statePtr));
static void		CheckForStdChannelsBeingClosed _ANSI_ARGS_((
				Tcl_Channel chan));
static void		CleanupChannelHandlers _ANSI_ARGS_((
				Tcl_Interp *interp, Channel *chanPtr));
static int		CloseChannel _ANSI_ARGS_((Tcl_Interp *interp,
				Channel *chanPtr, int errorCode));
static void		CommonGetsCleanup _ANSI_ARGS_((Channel *chanPtr,
				Tcl_Encoding encoding));
static int		CopyAndTranslateBuffer _ANSI_ARGS_((
				ChannelState *statePtr, char *result,
				int space));
static int		CopyData _ANSI_ARGS_((CopyState *csPtr, int mask));
static void		CopyEventProc _ANSI_ARGS_((ClientData clientData,
				int mask));
static void		CreateScriptRecord _ANSI_ARGS_((
				Tcl_Interp *interp, Channel *chanPtr,
				int mask, Tcl_Obj *scriptPtr));
static void		DeleteChannelTable _ANSI_ARGS_((
				ClientData clientData, Tcl_Interp *interp));
static void		DeleteScriptRecord _ANSI_ARGS_((Tcl_Interp *interp,
				Channel *chanPtr, int mask));
static void		DiscardInputQueued _ANSI_ARGS_((ChannelState *statePtr,
				int discardSavedBuffers));
static void		DiscardOutputQueued _ANSI_ARGS_((
				ChannelState *chanPtr));
static int		DoRead _ANSI_ARGS_((Channel *chanPtr, char *srcPtr,
				int slen));
static int		DoWrite _ANSI_ARGS_((Channel *chanPtr, char *src,
				int srcLen));
static int		FilterInputBytes _ANSI_ARGS_((Channel *chanPtr,
				GetsState *statePtr));
static int		FlushChannel _ANSI_ARGS_((Tcl_Interp *interp,
				Channel *chanPtr, int calledFromAsyncFlush));
static Tcl_HashTable *	GetChannelTable _ANSI_ARGS_((Tcl_Interp *interp));
static int		GetInput _ANSI_ARGS_((Channel *chanPtr));
static void		PeekAhead _ANSI_ARGS_((Channel *chanPtr,
				char **dstEndPtr, GetsState *gsPtr));
static int		ReadBytes _ANSI_ARGS_((ChannelState *statePtr,
				Tcl_Obj *objPtr, int charsLeft,
				int *offsetPtr));
static int		ReadChars _ANSI_ARGS_((ChannelState *statePtr,
				Tcl_Obj *objPtr, int charsLeft, int *offsetPtr,
				int *factorPtr));
static void		RecycleBuffer _ANSI_ARGS_((ChannelState *statePtr,
				ChannelBuffer *bufPtr, int mustDiscard));
static int		StackSetBlockMode _ANSI_ARGS_((Channel *chanPtr,
				int mode));
static int		SetBlockMode _ANSI_ARGS_((Tcl_Interp *interp,
				Channel *chanPtr, int mode));
static void		StopCopy _ANSI_ARGS_((CopyState *csPtr));
static int		TranslateInputEOL _ANSI_ARGS_((ChannelState *statePtr,
				char *dst, CONST char *src, int *dstLenPtr,
				int *srcLenPtr));
static int		TranslateOutputEOL _ANSI_ARGS_((ChannelState *statePtr,
				char *dst, CONST char *src, int *dstLenPtr,
				int *srcLenPtr));
static void		UpdateInterest _ANSI_ARGS_((Channel *chanPtr));
static int		WriteBytes _ANSI_ARGS_((Channel *chanPtr,
				CONST char *src, int srcLen));
static int		WriteChars _ANSI_ARGS_((Channel *chanPtr,
				CONST char *src, int srcLen));


/*
 *---------------------------------------------------------------------------
 *
 * TclInitIOSubsystem --
 *
 *	Initialize all resources used by this subsystem on a per-process
 *	basis.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the memory subsystems.
 *
 *---------------------------------------------------------------------------
 */

void
TclInitIOSubsystem()
{
    /*
     * By fetching thread local storage we take care of
     * allocating it for each thread.
     */
    (void) TCL_TSD_INIT(&dataKey);
}   

/*
 *-------------------------------------------------------------------------
 *
 * TclFinalizeIOSubsystem --
 *
 *	Releases all resources used by this subsystem on a per-process 
 *	basis.  Closes all extant channels that have not already been 
 *	closed because they were not owned by any interp.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on encoding and memory subsystems.
 *
 *-------------------------------------------------------------------------
 */

	/* ARGSUSED */
void
TclFinalizeIOSubsystem()
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    Channel *chanPtr;			/* Iterates over open channels. */
    ChannelState *nextCSPtr;		/* Iterates over open channels. */
    ChannelState *statePtr;		/* state of channel stack */

    for (statePtr = tsdPtr->firstCSPtr; statePtr != (ChannelState *) NULL;
	 statePtr = nextCSPtr) {
	chanPtr		= statePtr->topChanPtr;
        nextCSPtr	= statePtr->nextCSPtr;

        /*
         * Set the channel back into blocking mode to ensure that we wait
         * for all data to flush out.
         */
        
        (void) Tcl_SetChannelOption(NULL, (Tcl_Channel) chanPtr,
                "-blocking", "on");

        if ((chanPtr == (Channel *) tsdPtr->stdinChannel) ||
                (chanPtr == (Channel *) tsdPtr->stdoutChannel) ||
                (chanPtr == (Channel *) tsdPtr->stderrChannel)) {

            /*
             * Decrement the refcount which was earlier artificially bumped
             * up to keep the channel from being closed.
             */

            statePtr->refCount--;
        }

        if (statePtr->refCount <= 0) {

	    /*
             * Close it only if the refcount indicates that the channel is not
             * referenced from any interpreter. If it is, that interpreter will
             * close the channel when it gets destroyed.
             */

            (void) Tcl_Close((Tcl_Interp *) NULL, (Tcl_Channel) chanPtr);

        } else {

            /*
             * The refcount is greater than zero, so flush the channel.
             */

            Tcl_Flush((Tcl_Channel) chanPtr);

            /*
             * Call the device driver to actually close the underlying
             * device for this channel.
             */
            
	    if (chanPtr->typePtr->closeProc != TCL_CLOSE2PROC) {
		(chanPtr->typePtr->closeProc)(chanPtr->instanceData,
			(Tcl_Interp *) NULL);
	    } else {
		(chanPtr->typePtr->close2Proc)(chanPtr->instanceData,
			(Tcl_Interp *) NULL, 0);
	    }

            /*
             * Finally, we clean up the fields in the channel data structure
             * since all of them have been deleted already. We mark the
             * channel with CHANNEL_DEAD to prevent any further IO operations
             * on it.
             */

            chanPtr->instanceData = (ClientData) NULL;
            statePtr->flags |= CHANNEL_DEAD;
        }
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetStdChannel --
 *
 *	This function is used to change the channels that are used
 *	for stdin/stdout/stderr in new interpreters.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetStdChannel(channel, type)
    Tcl_Channel channel;
    int type;			/* One of TCL_STDIN, TCL_STDOUT, TCL_STDERR. */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    switch (type) {
	case TCL_STDIN:
	    tsdPtr->stdinInitialized = 1;
	    tsdPtr->stdinChannel = channel;
	    break;
	case TCL_STDOUT:
	    tsdPtr->stdoutInitialized = 1;
	    tsdPtr->stdoutChannel = channel;
	    break;
	case TCL_STDERR:
	    tsdPtr->stderrInitialized = 1;
	    tsdPtr->stderrChannel = channel;
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetStdChannel --
 *
 *	Returns the specified standard channel.
 *
 * Results:
 *	Returns the specified standard channel, or NULL.
 *
 * Side effects:
 *	May cause the creation of a standard channel and the underlying
 *	file.
 *
 *----------------------------------------------------------------------
 */
Tcl_Channel
Tcl_GetStdChannel(type)
    int type;			/* One of TCL_STDIN, TCL_STDOUT, TCL_STDERR. */
{
    Tcl_Channel channel = NULL;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    /*
     * If the channels were not created yet, create them now and
     * store them in the static variables. 
     */

    switch (type) {
	case TCL_STDIN:
	    if (!tsdPtr->stdinInitialized) {
		tsdPtr->stdinChannel = TclpGetDefaultStdChannel(TCL_STDIN);
		tsdPtr->stdinInitialized = 1;

		/*
                 * Artificially bump the refcount to ensure that the channel
                 * is only closed on exit.
                 *
                 * NOTE: Must only do this if stdinChannel is not NULL. It
                 * can be NULL in situations where Tcl is unable to connect
                 * to the standard input.
                 */

                if (tsdPtr->stdinChannel != (Tcl_Channel) NULL) {
                    (void) Tcl_RegisterChannel((Tcl_Interp *) NULL,
                            tsdPtr->stdinChannel);
                }
	    }
	    channel = tsdPtr->stdinChannel;
	    break;
	case TCL_STDOUT:
	    if (!tsdPtr->stdoutInitialized) {
		tsdPtr->stdoutChannel = TclpGetDefaultStdChannel(TCL_STDOUT);
		tsdPtr->stdoutInitialized = 1;
                if (tsdPtr->stdoutChannel != (Tcl_Channel) NULL) {
                    (void) Tcl_RegisterChannel((Tcl_Interp *) NULL,
                            tsdPtr->stdoutChannel);
                }
	    }
	    channel = tsdPtr->stdoutChannel;
	    break;
	case TCL_STDERR:
	    if (!tsdPtr->stderrInitialized) {
		tsdPtr->stderrChannel = TclpGetDefaultStdChannel(TCL_STDERR);
		tsdPtr->stderrInitialized = 1;
                if (tsdPtr->stderrChannel != (Tcl_Channel) NULL) {
                    (void) Tcl_RegisterChannel((Tcl_Interp *) NULL,
                            tsdPtr->stderrChannel);
                }
	    }
	    channel = tsdPtr->stderrChannel;
	    break;
    }
    return channel;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateCloseHandler
 *
 *	Creates a close callback which will be called when the channel is
 *	closed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Causes the callback to be called in the future when the channel
 *	will be closed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CreateCloseHandler(chan, proc, clientData)
    Tcl_Channel chan;		/* The channel for which to create the
                                 * close callback. */
    Tcl_CloseProc *proc;	/* The callback routine to call when the
                                 * channel will be closed. */
    ClientData clientData;	/* Arbitrary data to pass to the
                                 * close callback. */
{
    ChannelState *statePtr;
    CloseCallback *cbPtr;

    statePtr = ((Channel *) chan)->state;

    cbPtr = (CloseCallback *) ckalloc((unsigned) sizeof(CloseCallback));
    cbPtr->proc = proc;
    cbPtr->clientData = clientData;

    cbPtr->nextPtr = statePtr->closeCbPtr;
    statePtr->closeCbPtr = cbPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteCloseHandler --
 *
 *	Removes a callback that would have been called on closing
 *	the channel. If there is no matching callback then this
 *	function has no effect.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The callback will not be called in the future when the channel
 *	is eventually closed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteCloseHandler(chan, proc, clientData)
    Tcl_Channel chan;		/* The channel for which to cancel the
                                 * close callback. */
    Tcl_CloseProc *proc;	/* The procedure for the callback to
                                 * remove. */
    ClientData clientData;	/* The callback data for the callback
                                 * to remove. */
{
    ChannelState *statePtr;
    CloseCallback *cbPtr, *cbPrevPtr;

    statePtr = ((Channel *) chan)->state;
    for (cbPtr = statePtr->closeCbPtr, cbPrevPtr = (CloseCallback *) NULL;
	 cbPtr != (CloseCallback *) NULL;
	 cbPtr = cbPtr->nextPtr) {
        if ((cbPtr->proc == proc) && (cbPtr->clientData == clientData)) {
            if (cbPrevPtr == (CloseCallback *) NULL) {
                statePtr->closeCbPtr = cbPtr->nextPtr;
            }
            ckfree((char *) cbPtr);
            break;
        } else {
            cbPrevPtr = cbPtr;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetChannelTable --
 *
 *	Gets and potentially initializes the channel table for an
 *	interpreter. If it is initializing the table it also inserts
 *	channels for stdin, stdout and stderr if the interpreter is
 *	trusted.
 *
 * Results:
 *	A pointer to the hash table created, for use by the caller.
 *
 * Side effects:
 *	Initializes the channel table for an interpreter. May create
 *	channels for stdin, stdout and stderr.
 *
 *----------------------------------------------------------------------
 */

static Tcl_HashTable *
GetChannelTable(interp)
    Tcl_Interp *interp;
{
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_Channel stdinChan, stdoutChan, stderrChan;

    hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
    if (hTblPtr == (Tcl_HashTable *) NULL) {
        hTblPtr = (Tcl_HashTable *) ckalloc((unsigned) sizeof(Tcl_HashTable));
        Tcl_InitHashTable(hTblPtr, TCL_STRING_KEYS);

        (void) Tcl_SetAssocData(interp, "tclIO",
                (Tcl_InterpDeleteProc *) DeleteChannelTable,
                (ClientData) hTblPtr);

        /*
         * If the interpreter is trusted (not "safe"), insert channels
         * for stdin, stdout and stderr (possibly creating them in the
         * process).
         */

        if (Tcl_IsSafe(interp) == 0) {
            stdinChan = Tcl_GetStdChannel(TCL_STDIN);
            if (stdinChan != NULL) {
                Tcl_RegisterChannel(interp, stdinChan);
            }
            stdoutChan = Tcl_GetStdChannel(TCL_STDOUT);
            if (stdoutChan != NULL) {
                Tcl_RegisterChannel(interp, stdoutChan);
            }
            stderrChan = Tcl_GetStdChannel(TCL_STDERR);
            if (stderrChan != NULL) {
                Tcl_RegisterChannel(interp, stderrChan);
            }
        }

    }
    return hTblPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteChannelTable --
 *
 *	Deletes the channel table for an interpreter, closing any open
 *	channels whose refcount reaches zero. This procedure is invoked
 *	when an interpreter is deleted, via the AssocData cleanup
 *	mechanism.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the hash table of channels. May close channels. May flush
 *	output on closed channels. Removes any channeEvent handlers that were
 *	registered in this interpreter.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteChannelTable(clientData, interp)
    ClientData clientData;	/* The per-interpreter data structure. */
    Tcl_Interp *interp;		/* The interpreter being deleted. */
{
    Tcl_HashTable *hTblPtr;	/* The hash table. */
    Tcl_HashSearch hSearch;	/* Search variable. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    Channel *chanPtr;		/* Channel being deleted. */
    ChannelState *statePtr;	/* State of Channel being deleted. */
    EventScriptRecord *sPtr, *prevPtr, *nextPtr;
    				/* Variables to loop over all channel events
                                 * registered, to delete the ones that refer
                                 * to the interpreter being deleted. */

    /*
     * Delete all the registered channels - this will close channels whose
     * refcount reaches zero.
     */
    
    hTblPtr = (Tcl_HashTable *) clientData;
    for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
	 hPtr != (Tcl_HashEntry *) NULL;
	 hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch)) {

        chanPtr = (Channel *) Tcl_GetHashValue(hPtr);
	statePtr = chanPtr->state;

        /*
         * Remove any fileevents registered in this interpreter.
         */
        
        for (sPtr = statePtr->scriptRecordPtr,
                 prevPtr = (EventScriptRecord *) NULL;
	     sPtr != (EventScriptRecord *) NULL;
	     sPtr = nextPtr) {
            nextPtr = sPtr->nextPtr;
            if (sPtr->interp == interp) {
                if (prevPtr == (EventScriptRecord *) NULL) {
                    statePtr->scriptRecordPtr = nextPtr;
                } else {
                    prevPtr->nextPtr = nextPtr;
                }

                Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                        ChannelEventScriptInvoker, (ClientData) sPtr);

		Tcl_DecrRefCount(sPtr->scriptPtr);
                ckfree((char *) sPtr);
            } else {
                prevPtr = sPtr;
            }
        }

        /*
         * Cannot call Tcl_UnregisterChannel because that procedure calls
         * Tcl_GetAssocData to get the channel table, which might already
         * be inaccessible from the interpreter structure. Instead, we
         * emulate the behavior of Tcl_UnregisterChannel directly here.
         */

        Tcl_DeleteHashEntry(hPtr);
        statePtr->refCount--;
        if (statePtr->refCount <= 0) {
            if (!(statePtr->flags & BG_FLUSH_SCHEDULED)) {
                (void) Tcl_Close(interp, (Tcl_Channel) chanPtr);
            }
        }
    }
    Tcl_DeleteHashTable(hTblPtr);
    ckfree((char *) hTblPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CheckForStdChannelsBeingClosed --
 *
 *	Perform special handling for standard channels being closed. When
 *	given a standard channel, if the refcount is now 1, it means that
 *	the last reference to the standard channel is being explicitly
 *	closed. Now bump the refcount artificially down to 0, to ensure the
 *	normal handling of channels being closed will occur. Also reset the
 *	static pointer to the channel to NULL, to avoid dangling references.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Manipulates the refcount on standard channels. May smash the global
 *	static pointer to a standard channel.
 *
 *----------------------------------------------------------------------
 */

static void
CheckForStdChannelsBeingClosed(chan)
    Tcl_Channel chan;
{
    ChannelState *statePtr = ((Channel *) chan)->state;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if ((chan == tsdPtr->stdinChannel) && (tsdPtr->stdinInitialized)) {
        if (statePtr->refCount < 2) {
            statePtr->refCount = 0;
            tsdPtr->stdinChannel = NULL;
            return;
        }
    } else if ((chan == tsdPtr->stdoutChannel)
	    && (tsdPtr->stdoutInitialized)) {
        if (statePtr->refCount < 2) {
            statePtr->refCount = 0;
            tsdPtr->stdoutChannel = NULL;
            return;
        }
    } else if ((chan == tsdPtr->stderrChannel)
	    && (tsdPtr->stderrInitialized)) {
        if (statePtr->refCount < 2) {
            statePtr->refCount = 0;
            tsdPtr->stderrChannel = NULL;
            return;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegisterChannel --
 *
 *	Adds an already-open channel to the channel table of an interpreter.
 *	If the interpreter passed as argument is NULL, it only increments
 *	the channel refCount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May increment the reference count of a channel.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_RegisterChannel(interp, chan)
    Tcl_Interp *interp;		/* Interpreter in which to add the channel. */
    Tcl_Channel chan;		/* The channel to add to this interpreter
                                 * channel table. */
{
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    int new;			/* Is the hash entry new or does it exist? */
    Channel *chanPtr;		/* The actual channel. */
    ChannelState *statePtr;	/* State of the actual channel. */

    /*
     * Always (un)register bottom-most channel in the stack.  This makes
     * management of the channel list easier because no manipulation is
     * necessary during (un)stack operation.
     */
    chanPtr = ((Channel *) chan)->state->bottomChanPtr;
    statePtr = chanPtr->state;

    if (statePtr->channelName == (char *) NULL) {
        panic("Tcl_RegisterChannel: channel without name");
    }
    if (interp != (Tcl_Interp *) NULL) {
        hTblPtr = GetChannelTable(interp);
        hPtr = Tcl_CreateHashEntry(hTblPtr, statePtr->channelName, &new);
        if (new == 0) {
            if (chan == (Tcl_Channel) Tcl_GetHashValue(hPtr)) {
                return;
            }

	    panic("Tcl_RegisterChannel: duplicate channel names");
        }
        Tcl_SetHashValue(hPtr, (ClientData) chanPtr);
    }
    statePtr->refCount++;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnregisterChannel --
 *
 *	Deletes the hash entry for a channel associated with an interpreter.
 *	If the interpreter given as argument is NULL, it only decrements the
 *	reference count.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes the hash entry for a channel associated with an interpreter.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UnregisterChannel(interp, chan)
    Tcl_Interp *interp;		/* Interpreter in which channel is defined. */
    Tcl_Channel chan;		/* Channel to delete. */
{
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    Channel *chanPtr;		/* The real IO channel. */
    ChannelState *statePtr;	/* State of the real channel. */

    /*
     * Always (un)register bottom-most channel in the stack.  This makes
     * management of the channel list easier because no manipulation is
     * necessary during (un)stack operation.
     */
    chanPtr = ((Channel *) chan)->state->bottomChanPtr;
    statePtr = chanPtr->state;

    if (interp != (Tcl_Interp *) NULL) {
        hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
        if (hTblPtr == (Tcl_HashTable *) NULL) {
            return TCL_OK;
        }
        hPtr = Tcl_FindHashEntry(hTblPtr, statePtr->channelName);
        if (hPtr == (Tcl_HashEntry *) NULL) {
            return TCL_OK;
        }
        if ((Channel *) Tcl_GetHashValue(hPtr) != chanPtr) {
            return TCL_OK;
        }
        Tcl_DeleteHashEntry(hPtr);

        /*
         * Remove channel handlers that refer to this interpreter, so that they
         * will not be present if the actual close is delayed and more events
         * happen on the channel. This may occur if the channel is shared
         * between several interpreters, or if the channel has async
         * flushing active.
         */
    
        CleanupChannelHandlers(interp, chanPtr);
    }

    statePtr->refCount--;
    
    /*
     * Perform special handling for standard channels being closed. If the
     * refCount is now 1 it means that the last reference to the standard
     * channel is being explicitly closed, so bump the refCount down
     * artificially to 0. This will ensure that the channel is actually
     * closed, below. Also set the static pointer to NULL for the channel.
     */

    CheckForStdChannelsBeingClosed(chan);

    /*
     * If the refCount reached zero, close the actual channel.
     */

    if (statePtr->refCount <= 0) {

        /*
         * Ensure that if there is another buffer, it gets flushed
         * whether or not we are doing a background flush.
         */

        if ((statePtr->curOutPtr != NULL) &&
                (statePtr->curOutPtr->nextAdded >
                        statePtr->curOutPtr->nextRemoved)) {
            statePtr->flags |= BUFFER_READY;
        }
        statePtr->flags |= CHANNEL_CLOSED;
        if (!(statePtr->flags & BG_FLUSH_SCHEDULED)) {
            if (Tcl_Close(interp, chan) != TCL_OK) {
                return TCL_ERROR;
            }
        }
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_GetChannel --
 *
 *	Finds an existing Tcl_Channel structure by name in a given
 *	interpreter. This function is public because it is used by
 *	channel-type-specific functions.
 *
 * Results:
 *	A Tcl_Channel or NULL on failure. If failed, interp's result
 *	object contains an error message.  *modePtr is filled with the
 *	modes in which the channel was opened.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Channel
Tcl_GetChannel(interp, chanName, modePtr)
    Tcl_Interp *interp;		/* Interpreter in which to find or create
                                 * the channel. */
    char *chanName;		/* The name of the channel. */
    int *modePtr;		/* Where to store the mode in which the
                                 * channel was opened? Will contain an ORed
                                 * combination of TCL_READABLE and
                                 * TCL_WRITABLE, if non-NULL. */
{
    Channel *chanPtr;		/* The actual channel. */
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    char *name;			/* Translated name. */

    /*
     * Substitute "stdin", etc.  Note that even though we immediately
     * find the channel using Tcl_GetStdChannel, we still need to look
     * it up in the specified interpreter to ensure that it is present
     * in the channel table.  Otherwise, safe interpreters would always
     * have access to the standard channels.
     */

    name = chanName;
    if ((chanName[0] == 's') && (chanName[1] == 't')) {
	chanPtr = NULL;
	if (strcmp(chanName, "stdin") == 0) {
	    chanPtr = (Channel *) Tcl_GetStdChannel(TCL_STDIN);
	} else if (strcmp(chanName, "stdout") == 0) {
	    chanPtr = (Channel *) Tcl_GetStdChannel(TCL_STDOUT);
	} else if (strcmp(chanName, "stderr") == 0) {
	    chanPtr = (Channel *) Tcl_GetStdChannel(TCL_STDERR);
	}
	if (chanPtr != NULL) {
	    name = chanPtr->state->channelName;
	}
    }

    hTblPtr = GetChannelTable(interp);
    hPtr = Tcl_FindHashEntry(hTblPtr, name);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        Tcl_AppendResult(interp, "can not find channel named \"",
                chanName, "\"", (char *) NULL);
        return NULL;
    }

    /*
     * Always return bottom-most channel in the stack.  This one lives
     * the longest - other channels may go away unnoticed.
     * The other APIs compensate where necessary to retrieve the
     * topmost channel again.
     */
    chanPtr = (Channel *) Tcl_GetHashValue(hPtr);
    chanPtr = chanPtr->state->bottomChanPtr;
    if (modePtr != NULL) {
        *modePtr = (chanPtr->state->flags & (TCL_READABLE|TCL_WRITABLE));
    }
    
    return (Tcl_Channel) chanPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateChannel --
 *
 *	Creates a new entry in the hash table for a Tcl_Channel
 *	record.
 *
 * Results:
 *	Returns the new Tcl_Channel.
 *
 * Side effects:
 *	Creates a new Tcl_Channel instance and inserts it into the
 *	hash table.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_CreateChannel(typePtr, chanName, instanceData, mask)
    Tcl_ChannelType *typePtr;	/* The channel type record. */
    char *chanName;		/* Name of channel to record. */
    ClientData instanceData;	/* Instance specific data. */
    int mask;			/* TCL_READABLE & TCL_WRITABLE to indicate
                                 * if the channel is readable, writable. */
{
    Channel *chanPtr;		/* The channel structure newly created. */
    ChannelState *statePtr;	/* The stack-level independent state info
				 * for the channel. */
    CONST char *name;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    chanPtr  = (Channel *) ckalloc((unsigned) sizeof(Channel));
    statePtr = (ChannelState *) ckalloc((unsigned) sizeof(ChannelState));
    chanPtr->state = statePtr;

    chanPtr->instanceData	= instanceData;
    chanPtr->typePtr		= typePtr;

    /*
     * Set all the bits that are part of the stack-independent state
     * information for the channel.
     */

    if (chanName != (char *) NULL) {
        statePtr->channelName = ckalloc((unsigned) (strlen(chanName) + 1));
        strcpy(statePtr->channelName, chanName);
    } else {
        panic("Tcl_CreateChannel: NULL channel name");
    }

    statePtr->flags		= mask;

    /*
     * Set the channel to system default encoding.
     */

    statePtr->encoding = NULL;
    name = Tcl_GetEncodingName(NULL);
    if (strcmp(name, "binary") != 0) {
    	statePtr->encoding = Tcl_GetEncoding(NULL, name);
    }
    statePtr->inputEncodingState	= NULL;
    statePtr->inputEncodingFlags	= TCL_ENCODING_START;
    statePtr->outputEncodingState	= NULL;
    statePtr->outputEncodingFlags	= TCL_ENCODING_START;

    /*
     * Set the channel up initially in AUTO input translation mode to
     * accept "\n", "\r" and "\r\n". Output translation mode is set to
     * a platform specific default value. The eofChar is set to 0 for both
     * input and output, so that Tcl does not look for an in-file EOF
     * indicator (e.g. ^Z) and does not append an EOF indicator to files.
     */

    statePtr->inputTranslation	= TCL_TRANSLATE_AUTO;
    statePtr->outputTranslation	= TCL_PLATFORM_TRANSLATION;
    statePtr->inEofChar		= 0;
    statePtr->outEofChar	= 0;

    statePtr->unreportedError	= 0;
    statePtr->refCount		= 0;
    statePtr->closeCbPtr	= (CloseCallback *) NULL;
    statePtr->curOutPtr		= (ChannelBuffer *) NULL;
    statePtr->outQueueHead	= (ChannelBuffer *) NULL;
    statePtr->outQueueTail	= (ChannelBuffer *) NULL;
    statePtr->saveInBufPtr	= (ChannelBuffer *) NULL;
    statePtr->inQueueHead	= (ChannelBuffer *) NULL;
    statePtr->inQueueTail	= (ChannelBuffer *) NULL;
    statePtr->chPtr		= (ChannelHandler *) NULL;
    statePtr->interestMask	= 0;
    statePtr->scriptRecordPtr	= (EventScriptRecord *) NULL;
    statePtr->bufSize		= CHANNELBUFFER_DEFAULT_SIZE;
    statePtr->timer		= NULL;
    statePtr->csPtr		= NULL;

    statePtr->outputStage	= NULL;
    if ((statePtr->encoding != NULL) && (statePtr->flags & TCL_WRITABLE)) {
	statePtr->outputStage = (char *)
	    ckalloc((unsigned) (statePtr->bufSize + 2));
    }

    /*
     * As we are creating the channel, it is obviously the top for now
     */
    statePtr->topChanPtr	= chanPtr;
    statePtr->bottomChanPtr	= chanPtr;
    chanPtr->downChanPtr	= (Channel *) NULL;
    chanPtr->upChanPtr		= (Channel *) NULL;

    /*
     * Link the channel into the list of all channels; create an on-exit
     * handler if there is not one already, to close off all the channels
     * in the list on exit.
     */

    statePtr->nextCSPtr  = tsdPtr->firstCSPtr;
    tsdPtr->firstCSPtr   = statePtr;

    /*
     * Install this channel in the first empty standard channel slot, if
     * the channel was previously closed explicitly.
     */

    if ((tsdPtr->stdinChannel == NULL) &&
	    (tsdPtr->stdinInitialized == 1)) {
	Tcl_SetStdChannel((Tcl_Channel) chanPtr, TCL_STDIN);
        Tcl_RegisterChannel((Tcl_Interp *) NULL, (Tcl_Channel) chanPtr);
    } else if ((tsdPtr->stdoutChannel == NULL) &&
	    (tsdPtr->stdoutInitialized == 1)) {
	Tcl_SetStdChannel((Tcl_Channel) chanPtr, TCL_STDOUT);
        Tcl_RegisterChannel((Tcl_Interp *) NULL, (Tcl_Channel) chanPtr);
    } else if ((tsdPtr->stderrChannel == NULL) &&
	    (tsdPtr->stderrInitialized == 1)) {
	Tcl_SetStdChannel((Tcl_Channel) chanPtr, TCL_STDERR);
        Tcl_RegisterChannel((Tcl_Interp *) NULL, (Tcl_Channel) chanPtr);
    } 
    return (Tcl_Channel) chanPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_StackChannel --
 *
 *	Replaces an entry in the hash table for a Tcl_Channel
 *	record. The replacement is a new channel with same name,
 *	it supercedes the replaced channel. Input and output of
 *	the superceded channel is now going through the newly
 *	created channel and allows the arbitrary filtering/manipulation
 *	of the dataflow.
 *
 *	Andreas Kupries <a.kupries@westend.com>, 12/13/1998
 *	"Trf-Patch for filtering channels"
 *
 * Results:
 *	Returns the new Tcl_Channel, which actually contains the
 *      saved information about prevChan.
 *
 * Side effects:
 *    A new channel structure is allocated and linked below
 *    the existing channel.  The channel operations and client
 *    data of the existing channel are copied down to the newly
 *    created channel, and the current channel has its operations
 *    replaced by the new typePtr.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_StackChannel(interp, typePtr, instanceData, mask, prevChan)
    Tcl_Interp	    *interp;	   /* The interpreter we are working in */
    Tcl_ChannelType *typePtr;	   /* The channel type record for the new
				    * channel. */
    ClientData	     instanceData; /* Instance specific data for the new
				    * channel. */
    int		     mask;	   /* TCL_READABLE & TCL_WRITABLE to indicate
				    * if the channel is readable, writable. */
    Tcl_Channel	     prevChan;	   /* The channel structure to replace */
{
    ThreadSpecificData	*tsdPtr = TCL_TSD_INIT(&dataKey);
    Channel		*chanPtr, *prevChanPtr;
    ChannelState	*statePtr;

    /*
     * Find the given channel in the list of all channels.
     * If we don't find it, then it was never registered correctly.
     *
     * This operation should occur at the top of a channel stack.
     */

    statePtr    = (ChannelState *) tsdPtr->firstCSPtr;
    prevChanPtr = ((Channel *) prevChan)->state->topChanPtr;

    while (statePtr->topChanPtr != prevChanPtr) {
	statePtr = statePtr->nextCSPtr;
    }

    if (statePtr == NULL) {
	Tcl_AppendResult(interp, "couldn't find state for channel \"",
		Tcl_GetChannelName(prevChan), "\"", (char *) NULL);
        return (Tcl_Channel) NULL;
    }

    /*
     * Here we check if the given "mask" matches the "flags"
     * of the already existing channel.
     *
     *	  | - | R | W | RW |
     *	--+---+---+---+----+	<=>  0 != (chan->mask & prevChan->mask)
     *	- |   |   |   |    |
     *	R |   | + |   | +  |	The superceding channel is allowed to
     *	W |   |   | + | +  |	restrict the capabilities of the
     *	RW|   | + | + | +  |	superceded one !
     *	--+---+---+---+----+
     */

    if ((mask & (statePtr->flags & (TCL_READABLE | TCL_WRITABLE))) == 0) {
	Tcl_AppendResult(interp,
		"reading and writing both disallowed for channel \"",
		Tcl_GetChannelName(prevChan), "\"", (char *) NULL);
        return (Tcl_Channel) NULL;
    }

    /*
     * Flush the buffers. This ensures that any data still in them
     * at this time is not handled by the new transformation. Restrict
     * this to writable channels. Take care to hide a possible bg-copy
     * in progress from Tcl_Flush and the CheckForChannelErrors inside.
     */

    if ((mask & TCL_WRITABLE) != 0) {
        CopyState *csPtr;

        csPtr           = statePtr->csPtr;
	statePtr->csPtr = (CopyState*) NULL;

	if (Tcl_Flush((Tcl_Channel) prevChanPtr) != TCL_OK) {
	    statePtr->csPtr = csPtr;
	    Tcl_AppendResult(interp, "could not flush channel \"",
		    Tcl_GetChannelName(prevChan), "\"", (char *) NULL);
	    return (Tcl_Channel) NULL;
	}

	statePtr->csPtr = csPtr;
    }

    chanPtr = (Channel *) ckalloc((unsigned) sizeof(Channel));

    /*
     * Save some of the current state into the new structure,
     * reinitialize the parts which will stay with the transformation.
     *
     * Remarks:
     */

    chanPtr->state		= statePtr;
    chanPtr->instanceData	= instanceData;
    chanPtr->typePtr		= typePtr;
    chanPtr->downChanPtr	= prevChanPtr;
    chanPtr->upChanPtr		= (Channel *) NULL;

    /*
     * Place new block at the head of a possibly existing list of previously
     * stacked channels.
     */

    prevChanPtr->upChanPtr	= chanPtr;
    statePtr->topChanPtr	= chanPtr;

    return (Tcl_Channel) chanPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnstackChannel --
 *
 *	Unstacks an entry in the hash table for a Tcl_Channel
 *	record. This is the reverse to 'Tcl_StackChannel'.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	If TCL_ERROR is returned, the posix error code will be set
 *	with Tcl_SetErrno.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UnstackChannel (interp, chan)
    Tcl_Interp *interp; /* The interpreter we are working in */
    Tcl_Channel chan;   /* The channel to unstack */
{
    Channel      *chanPtr  = (Channel *) chan;
    ChannelState *statePtr = chanPtr->state;
    int result = 0;

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    if (chanPtr->downChanPtr != (Channel *) NULL) {
        /*
	 * Instead of manipulating the per-thread / per-interp list/hashtable
	 * of registered channels we wind down the state of the transformation,
	 * and then restore the state of underlying channel into the old
	 * structure.
	 */
	Channel *downChanPtr = chanPtr->downChanPtr;

	/*
	 * Flush the buffers. This ensures that any data still in them
	 * at this time _is_ handled by the transformation we are unstacking
	 * right now. Restrict this to writable channels. Take care to hide
	 * a possible bg-copy in progress from Tcl_Flush and the
	 * CheckForChannelErrors inside.
	 */

	if (statePtr->flags & TCL_WRITABLE) {
	    CopyState*    csPtr;

	    csPtr           = statePtr->csPtr;
	    statePtr->csPtr = (CopyState*) NULL;

	    if (Tcl_Flush((Tcl_Channel) chanPtr) != TCL_OK) {
	        statePtr->csPtr = csPtr;
		Tcl_AppendResult(interp, "could not flush channel \"",
			Tcl_GetChannelName((Tcl_Channel) chanPtr), "\"",
			(char *) NULL);
		return TCL_ERROR;
	    }

	    statePtr->csPtr = csPtr;
	}

	statePtr->topChanPtr	= downChanPtr;
	downChanPtr->upChanPtr	= (Channel *) NULL;

	/*
	 * Leave this link intact for closeproc
	 *  chanPtr->downChanPtr	= (Channel *) NULL;
	 */

	/*
	 * Close and free the channel driver state.
	 */

	if (chanPtr->typePtr->closeProc != TCL_CLOSE2PROC) {
	    result = (chanPtr->typePtr->closeProc)(chanPtr->instanceData,
		    interp);
	} else {
	    result = (chanPtr->typePtr->close2Proc)(chanPtr->instanceData,
		    interp, 0);
	}

	chanPtr->typePtr	= NULL;
	/*
	 * AK: Tcl_NotifyChannel may hold a reference to this block of memory
	 */
	Tcl_EventuallyFree((ClientData) chanPtr, TCL_DYNAMIC);
	UpdateInterest(downChanPtr);

	if (result != 0) {
	    Tcl_SetErrno(result);
	    return TCL_ERROR;
	}
    } else {
        /*
	 * This channel does not cover another one.
	 * Simply do a close, if necessary.
	 */

        if (statePtr->refCount <= 0) {
            if (Tcl_Close(interp, chan) != TCL_OK) {
                return TCL_ERROR;
            }
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetStackedChannel --
 *
 *	Determines whether the specified channel is stacked upon another.
 *
 * Results:
 *	NULL if the channel is not stacked upon another one, or a reference
 *	to the channel it is stacked upon. This reference can be used in
 *	queries, but modification is not allowed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_GetStackedChannel(chan)
    Tcl_Channel chan;
{
    Channel *chanPtr = (Channel *) chan;	/* The actual channel. */

    return (Tcl_Channel) chanPtr->downChanPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetTopChannel --
 *
 *	Returns the top channel of a channel stack.
 *
 * Results:
 *	NULL if the channel is not stacked upon another one, or a reference
 *	to the channel it is stacked upon. This reference can be used in
 *	queries, but modification is not allowed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_GetTopChannel(chan)
    Tcl_Channel chan;
{
    Channel *chanPtr = (Channel *) chan;	/* The actual channel. */

    return (Tcl_Channel) chanPtr->state->topChanPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelInstanceData --
 *
 *	Returns the client data associated with a channel.
 *
 * Results:
 *	The client data.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_GetChannelInstanceData(chan)
    Tcl_Channel chan;		/* Channel for which to return client data. */
{
    Channel *chanPtr = (Channel *) chan;	/* The actual channel. */

    return chanPtr->instanceData;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelType --
 *
 *	Given a channel structure, returns the channel type structure.
 *
 * Results:
 *	Returns a pointer to the channel type structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_ChannelType *
Tcl_GetChannelType(chan)
    Tcl_Channel chan;		/* The channel to return type for. */
{
    Channel *chanPtr = (Channel *) chan;	/* The actual channel. */

    return chanPtr->typePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelMode --
 *
 *	Computes a mask indicating whether the channel is open for
 *	reading and writing.
 *
 * Results:
 *	An OR-ed combination of TCL_READABLE and TCL_WRITABLE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetChannelMode(chan)
    Tcl_Channel chan;		/* The channel for which the mode is
                                 * being computed. */
{
    ChannelState *statePtr = ((Channel *) chan)->state;
					/* State of actual channel. */

    return (statePtr->flags & (TCL_READABLE | TCL_WRITABLE));
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelName --
 *
 *	Returns the string identifying the channel name.
 *
 * Results:
 *	The string containing the channel name. This memory is
 *	owned by the generic layer and should not be modified by
 *	the caller.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetChannelName(chan)
    Tcl_Channel chan;		/* The channel for which to return the name. */
{
    ChannelState *statePtr;	/* State of actual channel. */

    statePtr = ((Channel *) chan)->state;
    return statePtr->channelName;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelHandle --
 *
 *	Returns an OS handle associated with a channel.
 *
 * Results:
 *	Returns TCL_OK and places the handle in handlePtr, or returns
 *	TCL_ERROR on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetChannelHandle(chan, direction, handlePtr)
    Tcl_Channel chan;		/* The channel to get file from. */
    int direction;		/* TCL_WRITABLE or TCL_READABLE. */
    ClientData *handlePtr;	/* Where to store handle */
{
    Channel *chanPtr;		/* The actual channel. */
    ClientData handle;
    int result;

    chanPtr = ((Channel *) chan)->state->bottomChanPtr;
    result = (chanPtr->typePtr->getHandleProc)(chanPtr->instanceData,
	    direction, &handle);
    if (handlePtr) {
	*handlePtr = handle;
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * AllocChannelBuffer --
 *
 *	A channel buffer has BUFFER_PADDING bytes extra at beginning to
 *	hold any bytes of a native-encoding character that got split by
 *	the end of the previous buffer and need to be moved to the
 *	beginning of the next buffer to make a contiguous string so it
 *	can be converted to UTF-8.
 *
 *	A channel buffer has BUFFER_PADDING bytes extra at the end to
 *	hold any bytes of a native-encoding character (generated from a
 *	UTF-8 character) that overflow past the end of the buffer and
 *	need to be moved to the next buffer.
 *
 * Results:
 *	A newly allocated channel buffer.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static ChannelBuffer *
AllocChannelBuffer(length)
    int length;			/* Desired length of channel buffer. */
{
    ChannelBuffer *bufPtr;
    int n;

    n = length + CHANNELBUFFER_HEADER_SIZE + BUFFER_PADDING + BUFFER_PADDING;
    bufPtr = (ChannelBuffer *) ckalloc((unsigned) n);
    bufPtr->nextAdded	= BUFFER_PADDING;
    bufPtr->nextRemoved	= BUFFER_PADDING;
    bufPtr->bufLength	= length + BUFFER_PADDING;
    bufPtr->nextPtr	= (ChannelBuffer *) NULL;
    return bufPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RecycleBuffer --
 *
 *	Helper function to recycle input and output buffers. Ensures
 *	that two input buffers are saved (one in the input queue and
 *	another in the saveInBufPtr field) and that curOutPtr is set
 *	to a buffer. Only if these conditions are met is the buffer
 *	freed to the OS.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May free a buffer to the OS.
 *
 *----------------------------------------------------------------------
 */

static void
RecycleBuffer(statePtr, bufPtr, mustDiscard)
    ChannelState *statePtr;	/* ChannelState in which to recycle buffers. */
    ChannelBuffer *bufPtr;	/* The buffer to recycle. */
    int mustDiscard;		/* If nonzero, free the buffer to the
                                 * OS, always. */
{
    /*
     * Do we have to free the buffer to the OS?
     */

    if (mustDiscard) {
        ckfree((char *) bufPtr);
        return;
    }

    /*
     * Only save buffers for the input queue if the channel is readable.
     */
    
    if (statePtr->flags & TCL_READABLE) {
        if (statePtr->inQueueHead == (ChannelBuffer *) NULL) {
            statePtr->inQueueHead = bufPtr;
            statePtr->inQueueTail = bufPtr;
            goto keepit;
        }
        if (statePtr->saveInBufPtr == (ChannelBuffer *) NULL) {
            statePtr->saveInBufPtr = bufPtr;
            goto keepit;
        }
    }

    /*
     * Only save buffers for the output queue if the channel is writable.
     */

    if (statePtr->flags & TCL_WRITABLE) {
        if (statePtr->curOutPtr == (ChannelBuffer *) NULL) {
            statePtr->curOutPtr = bufPtr;
            goto keepit;
        }
    }

    /*
     * If we reached this code we return the buffer to the OS.
     */

    ckfree((char *) bufPtr);
    return;

    keepit:
    bufPtr->nextRemoved = BUFFER_PADDING;
    bufPtr->nextAdded = BUFFER_PADDING;
    bufPtr->nextPtr = (ChannelBuffer *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DiscardOutputQueued --
 *
 *	Discards all output queued in the output queue of a channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Recycles buffers.
 *
 *----------------------------------------------------------------------
 */

static void
DiscardOutputQueued(statePtr)
    ChannelState *statePtr;	/* ChannelState for which to discard output. */
{
    ChannelBuffer *bufPtr;
    
    while (statePtr->outQueueHead != (ChannelBuffer *) NULL) {
        bufPtr = statePtr->outQueueHead;
        statePtr->outQueueHead = bufPtr->nextPtr;
        RecycleBuffer(statePtr, bufPtr, 0);
    }
    statePtr->outQueueHead = (ChannelBuffer *) NULL;
    statePtr->outQueueTail = (ChannelBuffer *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CheckForDeadChannel --
 *
 *	This function checks is a given channel is Dead.
 *      (A channel that has been closed but not yet deallocated.)
 *
 * Results:
 *	True (1) if channel is Dead, False (0) if channel is Ok
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static int
CheckForDeadChannel(interp, statePtr)
    Tcl_Interp *interp;		/* For error reporting (can be NULL) */
    ChannelState *statePtr;	/* The channel state to check. */
{
    if (statePtr->flags & CHANNEL_DEAD) {
        Tcl_SetErrno(EINVAL);
	if (interp) {
	    Tcl_AppendResult(interp,
		    "unable to access channel: invalid channel",
		    (char *) NULL);   
	}
	return 1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * FlushChannel --
 *
 *	This function flushes as much of the queued output as is possible
 *	now. If calledFromAsyncFlush is nonzero, it is being called in an
 *	event handler to flush channel output asynchronously.
 *
 * Results:
 *	0 if successful, else the error code that was returned by the
 *	channel type operation.
 *
 * Side effects:
 *	May produce output on a channel. May block indefinitely if the
 *	channel is synchronous. May schedule an async flush on the channel.
 *	May recycle memory for buffers in the output queue.
 *
 *----------------------------------------------------------------------
 */

static int
FlushChannel(interp, chanPtr, calledFromAsyncFlush)
    Tcl_Interp *interp;			/* For error reporting during close. */
    Channel *chanPtr;			/* The channel to flush on. */
    int calledFromAsyncFlush;		/* If nonzero then we are being
                                         * called from an asynchronous
                                         * flush callback. */
{
    ChannelState *statePtr = chanPtr->state;
					/* State of the channel stack. */
    ChannelBuffer *bufPtr;		/* Iterates over buffered output
                                         * queue. */
    int toWrite;			/* Amount of output data in current
                                         * buffer available to be written. */
    int written;			/* Amount of output data actually
                                         * written in current round. */
    int errorCode = 0;			/* Stores POSIX error codes from
                                         * channel driver operations. */
    int wroteSome = 0;			/* Set to one if any data was
					 * written to the driver. */

    /*
     * Prevent writing on a dead channel -- a channel that has been closed
     * but not yet deallocated. This can occur if the exit handler for the
     * channel deallocation runs before all channels are deregistered in
     * all interpreters.
     */
    
    if (CheckForDeadChannel(interp, statePtr)) return -1;
    
    /*
     * Loop over the queued buffers and attempt to flush as
     * much as possible of the queued output to the channel.
     */

    while (1) {

        /*
         * If the queue is empty and there is a ready current buffer, OR if
         * the current buffer is full, then move the current buffer to the
         * queue.
         */

        if (((statePtr->curOutPtr != (ChannelBuffer *) NULL) &&
                (statePtr->curOutPtr->nextAdded == statePtr->curOutPtr->bufLength))
                || ((statePtr->flags & BUFFER_READY) &&
                        (statePtr->outQueueHead == (ChannelBuffer *) NULL))) {
            statePtr->flags &= (~(BUFFER_READY));
            statePtr->curOutPtr->nextPtr = (ChannelBuffer *) NULL;
            if (statePtr->outQueueHead == (ChannelBuffer *) NULL) {
                statePtr->outQueueHead = statePtr->curOutPtr;
            } else {
                statePtr->outQueueTail->nextPtr = statePtr->curOutPtr;
            }
            statePtr->outQueueTail = statePtr->curOutPtr;
            statePtr->curOutPtr = (ChannelBuffer *) NULL;
        }
        bufPtr = statePtr->outQueueHead;

        /*
         * If we are not being called from an async flush and an async
         * flush is active, we just return without producing any output.
         */

        if ((!calledFromAsyncFlush) &&
                (statePtr->flags & BG_FLUSH_SCHEDULED)) {
            return 0;
        }

        /*
         * If the output queue is still empty, break out of the while loop.
         */

        if (bufPtr == (ChannelBuffer *) NULL) {
            break;	/* Out of the "while (1)". */
        }

        /*
         * Produce the output on the channel.
         */
        
        toWrite = bufPtr->nextAdded - bufPtr->nextRemoved;
        written = (chanPtr->typePtr->outputProc) (chanPtr->instanceData,
                (char *) bufPtr->buf + bufPtr->nextRemoved, toWrite,
		&errorCode);

	/*
         * If the write failed completely attempt to start the asynchronous
         * flush mechanism and break out of this loop - do not attempt to
         * write any more output at this time.
         */

        if (written < 0) {
            
            /*
             * If the last attempt to write was interrupted, simply retry.
             */
            
            if (errorCode == EINTR) {
                errorCode = 0;
                continue;
            }

            /*
             * If the channel is non-blocking and we would have blocked,
             * start a background flushing handler and break out of the loop.
             */

            if ((errorCode == EWOULDBLOCK) || (errorCode == EAGAIN)) {
		/*
		 * This used to check for CHANNEL_NONBLOCKING, and panic
		 * if the channel was blocking.  However, it appears
		 * that setting stdin to -blocking 0 has some effect on
		 * the stdout when it's a tty channel (dup'ed underneath)
		 */
		if (!(statePtr->flags & BG_FLUSH_SCHEDULED)) {
		    statePtr->flags |= BG_FLUSH_SCHEDULED;
		    UpdateInterest(chanPtr);
		}
		errorCode = 0;
		break;
            }

            /*
             * Decide whether to report the error upwards or defer it.
             */

            if (calledFromAsyncFlush) {
                if (statePtr->unreportedError == 0) {
                    statePtr->unreportedError = errorCode;
                }
            } else {
                Tcl_SetErrno(errorCode);
		if (interp != NULL) {
		    Tcl_SetResult(interp,
			    Tcl_PosixError(interp), TCL_VOLATILE);
		}
            }

            /*
             * When we get an error we throw away all the output
             * currently queued.
             */

            DiscardOutputQueued(statePtr);
            continue;
        } else {
	    wroteSome = 1;
	}

        bufPtr->nextRemoved += written;

        /*
         * If this buffer is now empty, recycle it.
         */

        if (bufPtr->nextRemoved == bufPtr->nextAdded) {
            statePtr->outQueueHead = bufPtr->nextPtr;
            if (statePtr->outQueueHead == (ChannelBuffer *) NULL) {
                statePtr->outQueueTail = (ChannelBuffer *) NULL;
            }
            RecycleBuffer(statePtr, bufPtr, 0);
        }
    }	/* Closes "while (1)". */

    /*
     * If we wrote some data while flushing in the background, we are done.
     * We can't finish the background flush until we run out of data and
     * the channel becomes writable again.  This ensures that all of the
     * pending data has been flushed at the system level.
     */

    if (statePtr->flags & BG_FLUSH_SCHEDULED) {
	if (wroteSome) {
	    return errorCode;
	} else if (statePtr->outQueueHead == (ChannelBuffer *) NULL) {
	    statePtr->flags &= (~(BG_FLUSH_SCHEDULED));
	    (chanPtr->typePtr->watchProc)(chanPtr->instanceData,
		    statePtr->interestMask);
	}
    }

    /*
     * If the channel is flagged as closed, delete it when the refCount
     * drops to zero, the output queue is empty and there is no output
     * in the current output buffer.
     */

    if ((statePtr->flags & CHANNEL_CLOSED) && (statePtr->refCount <= 0) &&
            (statePtr->outQueueHead == (ChannelBuffer *) NULL) &&
            ((statePtr->curOutPtr == (ChannelBuffer *) NULL) ||
                    (statePtr->curOutPtr->nextAdded ==
                            statePtr->curOutPtr->nextRemoved))) {
	return CloseChannel(interp, chanPtr, errorCode);
    }
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * CloseChannel --
 *
 *	Utility procedure to close a channel and free associated resources.
 *
 *	If the channel was stacked, then the it will copy the necessary
 *	elements of the NEXT channel into the TOP channel, in essence
 *	unstacking the channel.  The NEXT channel will then be freed.
 *
 *	If the channel was not stacked, then we will free all the bits
 *	for the TOP channel, including the data structure itself.
 *
 * Results:
 *	1 if the channel was stacked, 0 otherwise.
 *
 * Side effects:
 *	May close the actual channel; may free memory.
 *	May change the value of errno.
 *
 *----------------------------------------------------------------------
 */

static int
CloseChannel(interp, chanPtr, errorCode)
    Tcl_Interp *interp;			/* For error reporting. */
    Channel *chanPtr;			/* The channel to close. */
    int errorCode;			/* Status of operation so far. */
{
    int result = 0;			/* Of calling driver close
                                         * operation. */
    ChannelState *prevCSPtr;		/* Preceding channel state in list of
                                         * all states - used to splice a
                                         * channel out of the list on close. */
    ChannelState *statePtr;		/* state of the channel stack. */
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (chanPtr == NULL) {
        return result;
    }
    statePtr = chanPtr->state;

    /*
     * No more input can be consumed so discard any leftover input.
     */

    DiscardInputQueued(statePtr, 1);

    /*
     * Discard a leftover buffer in the current output buffer field.
     */

    if (statePtr->curOutPtr != (ChannelBuffer *) NULL) {
        ckfree((char *) statePtr->curOutPtr);
        statePtr->curOutPtr = (ChannelBuffer *) NULL;
    }
    
    /*
     * The caller guarantees that there are no more buffers
     * queued for output.
     */

    if (statePtr->outQueueHead != (ChannelBuffer *) NULL) {
        panic("TclFlush, closed channel: queued output left");
    }

    /*
     * If the EOF character is set in the channel, append that to the
     * output device.
     */

    if ((statePtr->outEofChar != 0) && (statePtr->flags & TCL_WRITABLE)) {
        int dummy;
        char c;

        c = (char) statePtr->outEofChar;
        (chanPtr->typePtr->outputProc) (chanPtr->instanceData, &c, 1, &dummy);
    }

    /*
     * Remove TCL_READABLE and TCL_WRITABLE from statePtr->flags, so
     * that close callbacks can not do input or output (assuming they
     * squirreled the channel away in their clientData). This also
     * prevents infinite loops if the callback calls any C API that
     * could call FlushChannel.
     */

    statePtr->flags &= (~(TCL_READABLE|TCL_WRITABLE));

    /*
     * Splice this channel out of the list of all channels.
     */

    if (tsdPtr->firstCSPtr && (statePtr == tsdPtr->firstCSPtr)) {
        tsdPtr->firstCSPtr = statePtr->nextCSPtr;
    } else {
        for (prevCSPtr = tsdPtr->firstCSPtr;
	     prevCSPtr && (prevCSPtr->nextCSPtr != statePtr);
	     prevCSPtr = prevCSPtr->nextCSPtr) {
            /* Empty loop body. */
        }
        if (prevCSPtr == (ChannelState *) NULL) {
            panic("FlushChannel: damaged channel list");
        }
        prevCSPtr->nextCSPtr = statePtr->nextCSPtr;
    }

    /*
     * Close and free the channel driver state.
     */

    if (chanPtr->typePtr->closeProc != TCL_CLOSE2PROC) {
	result = (chanPtr->typePtr->closeProc)(chanPtr->instanceData, interp);
    } else {
	result = (chanPtr->typePtr->close2Proc)(chanPtr->instanceData, interp,
		0);
    }

    /*
     * Some resources can be cleared only if the bottom channel
     * in a stack is closed. All the other channels in the stack
     * are not allowed to remove.
     */

    if (chanPtr == statePtr->bottomChanPtr) {
	if (statePtr->channelName != (char *) NULL) {
	    ckfree(statePtr->channelName);
	    statePtr->channelName = NULL;
	}

	Tcl_FreeEncoding(statePtr->encoding);
	if (statePtr->outputStage != NULL) {
	    ckfree((char *) statePtr->outputStage);
	    statePtr->outputStage = (char *) NULL;
	}
    }

    /*
     * If we are being called synchronously, report either
     * any latent error on the channel or the current error.
     */

    if (statePtr->unreportedError != 0) {
        errorCode = statePtr->unreportedError;
    }
    if (errorCode == 0) {
        errorCode = result;
        if (errorCode != 0) {
            Tcl_SetErrno(errorCode);
        }
    }

    /*
     * Cancel any outstanding timer.
     */

    Tcl_DeleteTimerHandler(statePtr->timer);

    /*
     * Mark the channel as deleted by clearing the type structure.
     */

    if (chanPtr->downChanPtr != (Channel *) NULL) {
#if 0
	int code = TCL_OK;

	while (chanPtr->downChanPtr != (Channel *) NULL) {
	    /*
	     * Unwind the state of the transformation, and then restore the
	     * state of (unstack) the underlying channel into the TOP channel
	     * structure.
	     */
	    code = Tcl_UnstackChannel(interp, (Tcl_Channel) chanPtr);
	    if (code == TCL_ERROR) {
		errorCode = Tcl_GetErrno();
		break;
	    }
	    chanPtr = chanPtr->downChanPtr;
	}
#else
	Channel *downChanPtr = chanPtr->downChanPtr;

	statePtr->nextCSPtr	= tsdPtr->firstCSPtr;
	tsdPtr->firstCSPtr	= statePtr;

	statePtr->topChanPtr	= downChanPtr;
	downChanPtr->upChanPtr	= (Channel *) NULL;
	chanPtr->typePtr	= NULL;

	Tcl_EventuallyFree((ClientData) chanPtr, TCL_DYNAMIC);
	return Tcl_Close(interp, (Tcl_Channel) downChanPtr);
#endif
    }

    /*
     * There is only the TOP Channel, so we free the remaining
     * pointers we have and then ourselves.
     */
    chanPtr->typePtr = NULL;

    Tcl_EventuallyFree((ClientData) chanPtr, TCL_DYNAMIC);

    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Close --
 *
 *	Closes a channel.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Closes the channel if this is the last reference.
 *
 * NOTE:
 *	Tcl_Close removes the channel as far as the user is concerned.
 *	However, it may continue to exist for a while longer if it has
 *	a background flush scheduled. The device itself is eventually
 *	closed and the channel record removed, in CloseChannel, above.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_Close(interp, chan)
    Tcl_Interp *interp;			/* Interpreter for errors. */
    Tcl_Channel chan;			/* The channel being closed. Must
                                         * not be referenced in any
                                         * interpreter. */
{
    ChannelHandler *chPtr, *chNext;	/* Iterate over channel handlers. */
    CloseCallback *cbPtr;		/* Iterate over close callbacks
                                         * for this channel. */
    EventScriptRecord *ePtr, *eNextPtr;	/* Iterate over eventscript records. */
    Channel *chanPtr;			/* The real IO channel. */
    ChannelState *statePtr;		/* State of real IO channel. */
    int result;				/* Of calling FlushChannel. */
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    NextChannelHandler *nhPtr;

    if (chan == (Tcl_Channel) NULL) {
        return TCL_OK;
    }

    /*
     * Perform special handling for standard channels being closed. If the
     * refCount is now 1 it means that the last reference to the standard
     * channel is being explicitly closed, so bump the refCount down
     * artificially to 0. This will ensure that the channel is actually
     * closed, below. Also set the static pointer to NULL for the channel.
     */

    CheckForStdChannelsBeingClosed(chan);

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr	= (Channel *) chan;
    statePtr	= chanPtr->state;
    chanPtr	= statePtr->topChanPtr;

    if (statePtr->refCount > 0) {
        panic("called Tcl_Close on channel with refCount > 0");
    }

    /*
     * Remove any references to channel handlers for this channel that
     * may be about to be invoked.
     */

    for (nhPtr = tsdPtr->nestedHandlerPtr;
	 nhPtr != (NextChannelHandler *) NULL;
	 nhPtr = nhPtr->nestedHandlerPtr) {
        if (nhPtr->nextHandlerPtr &&
		(nhPtr->nextHandlerPtr->chanPtr == chanPtr)) {
	    nhPtr->nextHandlerPtr = NULL;
        }
    }

    /*
     * Remove all the channel handler records attached to the channel
     * itself.
     */

    for (chPtr = statePtr->chPtr;
	 chPtr != (ChannelHandler *) NULL;
	 chPtr = chNext) {
        chNext = chPtr->nextPtr;
        ckfree((char *) chPtr);
    }
    statePtr->chPtr = (ChannelHandler *) NULL;

    /*
     * Cancel any pending copy operation.
     */

    StopCopy(statePtr->csPtr);

    /*
     * Must set the interest mask now to 0, otherwise infinite loops
     * will occur if Tcl_DoOneEvent is called before the channel is
     * finally deleted in FlushChannel. This can happen if the channel
     * has a background flush active.
     */
        
    statePtr->interestMask = 0;
    
    /*
     * Remove any EventScript records for this channel.
     */

    for (ePtr = statePtr->scriptRecordPtr;
	 ePtr != (EventScriptRecord *) NULL;
	 ePtr = eNextPtr) {
        eNextPtr = ePtr->nextPtr;
	Tcl_DecrRefCount(ePtr->scriptPtr);
        ckfree((char *) ePtr);
    }
    statePtr->scriptRecordPtr = (EventScriptRecord *) NULL;
        
    /*
     * Invoke the registered close callbacks and delete their records.
     */

    while (statePtr->closeCbPtr != (CloseCallback *) NULL) {
        cbPtr = statePtr->closeCbPtr;
        statePtr->closeCbPtr = cbPtr->nextPtr;
        (cbPtr->proc) (cbPtr->clientData);
        ckfree((char *) cbPtr);
    }

    /*
     * Ensure that the last output buffer will be flushed.
     */
    
    if ((statePtr->curOutPtr != (ChannelBuffer *) NULL) &&
	    (statePtr->curOutPtr->nextAdded > statePtr->curOutPtr->nextRemoved)) {
        statePtr->flags |= BUFFER_READY;
    }

    /*
     * If this channel supports it, close the read side, since we don't need it
     * anymore and this will help avoid deadlocks on some channel types.
     */

    if (chanPtr->typePtr->closeProc == TCL_CLOSE2PROC) {
	result = (chanPtr->typePtr->close2Proc)(chanPtr->instanceData, interp,
		TCL_CLOSE_READ);
    } else {
	result = 0;
    }

    /*
     * The call to FlushChannel will flush any queued output and invoke
     * the close function of the channel driver, or it will set up the
     * channel to be flushed and closed asynchronously.
     */

    statePtr->flags |= CHANNEL_CLOSED;
    if ((FlushChannel(interp, chanPtr, 0) != 0) || (result != 0)) {
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Write --
 *
 *	Puts a sequence of bytes into an output buffer, may queue the
 *	buffer for output if it gets full, and also remembers whether the
 *	current buffer is ready e.g. if it contains a newline and we are in
 *	line buffering mode.
 *
 * Results:
 *	The number of bytes written or -1 in case of error. If -1,
 *	Tcl_GetErrno will return the error code.
 *
 * Side effects:
 *	May buffer up output and may cause output to be produced on the
 *	channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Write(chan, src, srcLen)
    Tcl_Channel chan;			/* The channel to buffer output for. */
    char *src;				/* Data to queue in output buffer. */
    int srcLen;				/* Length of data in bytes, or < 0 for
					 * strlen(). */
{
    /*
     * Always use the topmost channel of the stack
     */
    Channel *chanPtr;
    ChannelState *statePtr;	/* state info for channel */

    statePtr = ((Channel *) chan)->state;
    chanPtr  = statePtr->topChanPtr;

    if (CheckChannelErrors(statePtr, TCL_WRITABLE) != 0) {
	return -1;
    }
    if (srcLen < 0) {
        srcLen = strlen(src);
    }
    return DoWrite(chanPtr, src, srcLen);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WriteRaw --
 *
 *	Puts a sequence of bytes into an output buffer, may queue the
 *	buffer for output if it gets full, and also remembers whether the
 *	current buffer is ready e.g. if it contains a newline and we are in
 *	line buffering mode.
 *
 * Results:
 *	The number of bytes written or -1 in case of error. If -1,
 *	Tcl_GetErrno will return the error code.
 *
 * Side effects:
 *	May buffer up output and may cause output to be produced on the
 *	channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WriteRaw(chan, src, srcLen)
    Tcl_Channel chan;			/* The channel to buffer output for. */
    char *src;				/* Data to queue in output buffer. */
    int srcLen;				/* Length of data in bytes, or < 0 for
					 * strlen(). */
{
    Channel *chanPtr = ((Channel *) chan);
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */

    CopyState*    csPtr;
    int           errorCode, written;

    /* FIX
     * The check below does too much because it will reject a call to this
     * function with a channel which is part of an 'fcopy'. But we have to
     * allow this here or else the chaining in the transformation drivers
     * will fail with 'file busy' error instead of retrieving and
     * transforming the data to copy.
     *
     * We let the check procedure now believe that there is no fcopy in
     * progress. A better solution than this might be an additional flag
     * argument to switch off specific checks.
     */

    csPtr           = statePtr->csPtr;
    statePtr->csPtr = (CopyState*) NULL;

    if (CheckChannelErrors(statePtr, TCL_WRITABLE) != 0) {
	statePtr->csPtr = csPtr;
	return -1;
    }
    if (srcLen < 0) {
        srcLen = strlen(src);
    }
    /*return DoWrite(chanPtr, src, srcLen);*/

    /*
     * Go immediately to the driver, do all the error handling by ourselves.
     * The code was stolen from 'FlushChannel'.
     */

    written = (chanPtr->typePtr->outputProc) (chanPtr->instanceData,
	    src, srcLen, &errorCode);

    if (written < 0) {
	Tcl_SetErrno(errorCode);
    }

    return written;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_WriteChars --
 *
 *	Takes a sequence of UTF-8 characters and converts them for output
 *	using the channel's current encoding, may queue the buffer for
 *	output if it gets full, and also remembers whether the current
 *	buffer is ready e.g. if it contains a newline and we are in
 *	line buffering mode.
 *
 * Results:
 *	The number of bytes written or -1 in case of error. If -1,
 *	Tcl_GetErrno will return the error code.
 *
 * Side effects:
 *	May buffer up output and may cause output to be produced on the
 *	channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WriteChars(chan, src, len)
    Tcl_Channel chan;		/* The channel to buffer output for. */
    CONST char *src;		/* UTF-8 characters to queue in output buffer. */
    int len;			/* Length of string in bytes, or < 0 for 
				 * strlen(). */
{
    /*
     * Always use the topmost channel of the stack
     */
    Channel *chanPtr;
    ChannelState *statePtr;	/* state info for channel */

    statePtr = ((Channel *) chan)->state;
    chanPtr  = statePtr->topChanPtr;

    if (CheckChannelErrors(statePtr, TCL_WRITABLE) != 0) {
	return -1;
    }
    if (len < 0) {
        len = strlen(src);
    }
    if (statePtr->encoding == NULL) {
	/*
	 * Inefficient way to convert UTF-8 to byte-array, but the  
	 * code parallels the way it is done for objects.
	 */

	Tcl_Obj *objPtr;
	int result;

	objPtr = Tcl_NewStringObj(src, len);
	src = (char *) Tcl_GetByteArrayFromObj(objPtr, &len);
	result = WriteBytes(chanPtr, src, len);
	Tcl_DecrRefCount(objPtr);
	return result;
    }
    return WriteChars(chanPtr, src, len);
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_WriteObj --
 *
 *	Takes the Tcl object and queues its contents for output.  If the 
 *	encoding of the channel is NULL, takes the byte-array representation 
 *	of the object and queues those bytes for output.  Otherwise, takes 
 *	the characters in the UTF-8 (string) representation of the object 
 *	and converts them for output using the channel's current encoding.  
 *	May flush internal buffers to output if one becomes full or is ready 
 *	for some other reason, e.g. if it contains a newline and the channel 
 *	is in line buffering mode.
 *
 * Results:
 *	The number of bytes written or -1 in case of error. If -1, 
 *	Tcl_GetErrno() will return the error code.
 *
 * Side effects:
 *	May buffer up output and may cause output to be produced on the
 *	channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WriteObj(chan, objPtr)
    Tcl_Channel chan;		/* The channel to buffer output for. */
    Tcl_Obj *objPtr;		/* The object to write. */
{
    /*
     * Always use the topmost channel of the stack
     */
    Channel *chanPtr;
    ChannelState *statePtr;	/* state info for channel */
    char *src;
    int srcLen;

    statePtr = ((Channel *) chan)->state;
    chanPtr  = statePtr->topChanPtr;

    if (CheckChannelErrors(statePtr, TCL_WRITABLE) != 0) {
	return -1;
    }
    if (statePtr->encoding == NULL) {
	src = (char *) Tcl_GetByteArrayFromObj(objPtr, &srcLen);
	return WriteBytes(chanPtr, src, srcLen);
    } else {
	src = Tcl_GetStringFromObj(objPtr, &srcLen);
	return WriteChars(chanPtr, src, srcLen);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * WriteBytes --
 *
 *	Write a sequence of bytes into an output buffer, may queue the
 *	buffer for output if it gets full, and also remembers whether the
 *	current buffer is ready e.g. if it contains a newline and we are in
 *	line buffering mode.
 *
 * Results:
 *	The number of bytes written or -1 in case of error. If -1,
 *	Tcl_GetErrno will return the error code.
 *
 * Side effects:
 *	May buffer up output and may cause output to be produced on the
 *	channel.
 *
 *----------------------------------------------------------------------
 */

static int
WriteBytes(chanPtr, src, srcLen)
    Channel *chanPtr;		/* The channel to buffer output for. */
    CONST char *src;		/* Bytes to write. */
    int srcLen;			/* Number of bytes to write. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *bufPtr;
    char *dst;
    int dstLen, dstMax, sawLF, savedLF, total, toWrite;
    
    total = 0;
    sawLF = 0;
    savedLF = 0;

    /*
     * Loop over all bytes in src, storing them in output buffer with
     * proper EOL translation.
     */

    while (srcLen + savedLF > 0) {
	bufPtr = statePtr->curOutPtr;
	if (bufPtr == NULL) {
	    bufPtr = AllocChannelBuffer(statePtr->bufSize);
	    statePtr->curOutPtr	= bufPtr;
	}
	dst = bufPtr->buf + bufPtr->nextAdded;
	dstMax = bufPtr->bufLength - bufPtr->nextAdded;
	dstLen = dstMax;

	toWrite = dstLen;
	if (toWrite > srcLen) {
	    toWrite = srcLen;
	}

	if (savedLF) {
	    /*
	     * A '\n' was left over from last call to TranslateOutputEOL()
	     * and we need to store it in this buffer.  If the channel is
	     * line-based, we will need to flush it.
	     */

	    *dst++ = '\n';
	    dstLen--;
	    sawLF++;
	}
	sawLF += TranslateOutputEOL(statePtr, dst, src, &dstLen, &toWrite);
	dstLen += savedLF;
	savedLF = 0;

	if (dstLen > dstMax) {
	    savedLF = 1;
	    dstLen = dstMax;
	}
	bufPtr->nextAdded += dstLen;
	if (CheckFlush(chanPtr, bufPtr, sawLF) != 0) {
	    return -1;
	}
	total += dstLen;
	src += toWrite;
	srcLen -= toWrite;
	sawLF = 0;
    }
    return total;
}

/*
 *----------------------------------------------------------------------
 *
 * WriteChars --
 *
 *	Convert UTF-8 bytes to the channel's external encoding and
 *	write the produced bytes into an output buffer, may queue the 
 *	buffer for output if it gets full, and also remembers whether the
 *	current buffer is ready e.g. if it contains a newline and we are in
 *	line buffering mode.
 *
 * Results:
 *	The number of bytes written or -1 in case of error. If -1,
 *	Tcl_GetErrno will return the error code.
 *
 * Side effects:
 *	May buffer up output and may cause output to be produced on the
 *	channel.
 *
 *----------------------------------------------------------------------
 */

static int
WriteChars(chanPtr, src, srcLen)
    Channel *chanPtr;		/* The channel to buffer output for. */
    CONST char *src;		/* UTF-8 string to write. */
    int srcLen;			/* Length of UTF-8 string in bytes. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *bufPtr;
    char *dst, *stage;
    int saved, savedLF, sawLF, total, toWrite, flags;
    int dstWrote, dstLen, stageLen, stageMax, stageRead;
    Tcl_Encoding encoding;
    char safe[BUFFER_PADDING];
    
    total = 0;
    sawLF = 0;
    savedLF = 0;
    saved = 0;
    encoding = statePtr->encoding;

    /*
     * Loop over all UTF-8 characters in src, storing them in staging buffer
     * with proper EOL translation.
     */

    while (srcLen + savedLF > 0) {
	stage = statePtr->outputStage;
	stageMax = statePtr->bufSize;
	stageLen = stageMax;

	toWrite = stageLen;
	if (toWrite > srcLen) {
	    toWrite = srcLen;
	}

	if (savedLF) {
	    /*
	     * A '\n' was left over from last call to TranslateOutputEOL()
	     * and we need to store it in the staging buffer.  If the
	     * channel is line-based, we will need to flush the output
	     * buffer (after translating the staging buffer).
	     */
	    
	    *stage++ = '\n';
	    stageLen--;
	    sawLF++;
	}
	sawLF += TranslateOutputEOL(statePtr, stage, src, &stageLen, &toWrite);

	stage -= savedLF;
	stageLen += savedLF;
	savedLF = 0;

	if (stageLen > stageMax) {
	    savedLF = 1;
	    stageLen = stageMax;
	}
	src += toWrite;
	srcLen -= toWrite;

	flags = statePtr->outputEncodingFlags;
	if (srcLen == 0) {
	    flags |= TCL_ENCODING_END;
	}

	/*
	 * Loop over all UTF-8 characters in staging buffer, converting them
	 * to external encoding, storing them in output buffer.
	 */

	while (stageLen + saved > 0) {
	    bufPtr = statePtr->curOutPtr;
	    if (bufPtr == NULL) {
		bufPtr = AllocChannelBuffer(statePtr->bufSize);
		statePtr->curOutPtr = bufPtr;
	    }
	    dst = bufPtr->buf + bufPtr->nextAdded;
	    dstLen = bufPtr->bufLength - bufPtr->nextAdded;

	    if (saved != 0) {
		/*
		 * Here's some translated bytes left over from the last
		 * buffer that we need to stick at the beginning of this
		 * buffer.
		 */
		 
		memcpy((VOID *) dst, (VOID *) safe, (size_t) saved);
		bufPtr->nextAdded += saved;
		dst += saved;
		dstLen -= saved;
		saved = 0;
	    }

	    Tcl_UtfToExternal(NULL, encoding, stage, stageLen, flags,
		    &statePtr->outputEncodingState, dst,
		    dstLen + BUFFER_PADDING, &stageRead, &dstWrote, NULL);
	    if (stageRead + dstWrote == 0) {
		/*
		 * We have an incomplete UTF-8 character at the end of the
		 * staging buffer.  It will get moved to the beginning of the
		 * staging buffer followed by more bytes from src.
		 */

		src -= stageLen;
		srcLen += stageLen;
		stageLen = 0;
		savedLF = 0;
		break;
	    }
	    bufPtr->nextAdded += dstWrote;
	    if (bufPtr->nextAdded > bufPtr->bufLength) {
		/*
		 * When translating from UTF-8 to external encoding, we
		 * allowed the translation to produce a character that
		 * crossed the end of the output buffer, so that we would
		 * get a completely full buffer before flushing it.  The
		 * extra bytes will be moved to the beginning of the next
		 * buffer.
		 */

		saved = bufPtr->nextAdded - bufPtr->bufLength;
		memcpy((VOID *) safe, (VOID *) (dst + dstLen), (size_t) saved);
		bufPtr->nextAdded = bufPtr->bufLength;
	    }
	    if (CheckFlush(chanPtr, bufPtr, sawLF) != 0) {
		return -1;
	    }

	    total += dstWrote;
	    stage += stageRead;
	    stageLen -= stageRead;
	    sawLF = 0;
	}
    }
    return total;
}

/*
 *---------------------------------------------------------------------------
 *
 * TranslateOutputEOL --
 *
 *	Helper function for WriteBytes() and WriteChars().  Converts the
 *	'\n' characters in the source buffer into the appropriate EOL
 *	form specified by the output translation mode.
 *
 *	EOL translation stops either when the source buffer is empty
 *	or the output buffer is full.
 *
 *	When converting to CRLF mode and there is only 1 byte left in
 *	the output buffer, this routine stores the '\r' in the last
 *	byte and then stores the '\n' in the byte just past the end of the 
 *	buffer.  The caller is responsible for passing in a buffer that
 *	is large enough to hold the extra byte.
 *
 * Results:
 *	The return value is 1 if a '\n' was translated from the source
 *	buffer, or 0 otherwise -- this can be used by the caller to
 *	decide to flush a line-based channel even though the channel
 *	buffer is not full.
 *
 *	*dstLenPtr is filled with how many bytes of the output buffer
 *	were used.  As mentioned above, this can be one more that
 *	the output buffer's specified length if a CRLF was stored.
 *
 *	*srcLenPtr is filled with how many bytes of the source buffer
 *	were consumed.  
 *
 * Side effects:
 *	It may be obvious, but bears mentioning that when converting
 *	in CRLF mode (which requires two bytes of storage in the output
 *	buffer), the number of bytes consumed from the source buffer
 *	will be less than the number of bytes stored in the output buffer.
 *
 *---------------------------------------------------------------------------
 */

static int
TranslateOutputEOL(statePtr, dst, src, dstLenPtr, srcLenPtr)
    ChannelState *statePtr;	/* Channel being read, for translation and
				 * buffering modes. */
    char *dst;			/* Output buffer filled with UTF-8 chars by
				 * applying appropriate EOL translation to
				 * source characters. */
    CONST char *src;		/* Source UTF-8 characters. */
    int *dstLenPtr;		/* On entry, the maximum length of output
				 * buffer in bytes.  On exit, the number of
				 * bytes actually used in output buffer. */
    int *srcLenPtr;		/* On entry, the length of source buffer.
				 * On exit, the number of bytes read from
				 * the source buffer. */
{
    char *dstEnd;
    int srcLen, newlineFound;
    
    newlineFound = 0;
    srcLen = *srcLenPtr;

    switch (statePtr->outputTranslation) {
	case TCL_TRANSLATE_LF: {
	    for (dstEnd = dst + srcLen; dst < dstEnd; ) {
		if (*src == '\n') {
		    newlineFound = 1;
		}
		*dst++ = *src++;
	    }
	    *dstLenPtr = srcLen;
	    break;
	}
	case TCL_TRANSLATE_CR: {
	    for (dstEnd = dst + srcLen; dst < dstEnd;) {
		if (*src == '\n') {
		    *dst++ = '\r';
		    newlineFound = 1;
		    src++;
		} else {
		    *dst++ = *src++;
		}
	    }
	    *dstLenPtr = srcLen;
	    break;
	}
	case TCL_TRANSLATE_CRLF: {
	    /*
	     * Since this causes the number of bytes to grow, we
	     * start off trying to put 'srcLen' bytes into the
	     * output buffer, but allow it to store more bytes, as
	     * long as there's still source bytes and room in the
	     * output buffer.
	     */

	    char *dstStart, *dstMax;
	    CONST char *srcStart;
	    
	    dstStart = dst;
	    dstMax = dst + *dstLenPtr;

	    srcStart = src;
	    
	    if (srcLen < *dstLenPtr) {
		dstEnd = dst + srcLen;
	    } else {
		dstEnd = dst + *dstLenPtr;
	    }
	    while (dst < dstEnd) {
		if (*src == '\n') {
		    if (dstEnd < dstMax) {
			dstEnd++;
		    }
		    *dst++ = '\r';
		    newlineFound = 1;
		}
		*dst++ = *src++;
	    }
	    *srcLenPtr = src - srcStart;
	    *dstLenPtr = dst - dstStart;
	    break;
	}
	default: {
	    break;
	}
    }
    return newlineFound;
}

/*
 *---------------------------------------------------------------------------
 *
 * CheckFlush --
 *
 *	Helper function for WriteBytes() and WriteChars().  If the
 *	channel buffer is ready to be flushed, flush it.
 *
 * Results:
 *	The return value is -1 if there was a problem flushing the
 *	channel buffer, or 0 otherwise.
 *
 * Side effects:
 *	The buffer will be recycled if it is flushed.
 *
 *---------------------------------------------------------------------------
 */

static int
CheckFlush(chanPtr, bufPtr, newlineFlag)
    Channel *chanPtr;		/* Channel being read, for buffering mode. */
    ChannelBuffer *bufPtr;	/* Channel buffer to possibly flush. */
    int newlineFlag;		/* Non-zero if a the channel buffer
				 * contains a newline. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    /*
     * The current buffer is ready for output:
     * 1. if it is full.
     * 2. if it contains a newline and this channel is line-buffered.
     * 3. if it contains any output and this channel is unbuffered.
     */

    if ((statePtr->flags & BUFFER_READY) == 0) {
	if (bufPtr->nextAdded == bufPtr->bufLength) {
	    statePtr->flags |= BUFFER_READY;
	} else if (statePtr->flags & CHANNEL_LINEBUFFERED) {
	    if (newlineFlag != 0) {
		statePtr->flags |= BUFFER_READY;
	    }
	} else if (statePtr->flags & CHANNEL_UNBUFFERED) {
	    statePtr->flags |= BUFFER_READY;
	}
    }
    if (statePtr->flags & BUFFER_READY) {
	if (FlushChannel(NULL, chanPtr, 0) != 0) {
	    return -1;
	}
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_Gets --
 *
 *	Reads a complete line of input from the channel into a Tcl_DString.
 *
 * Results:
 *	Length of line read (in characters) or -1 if error, EOF, or blocked.
 *	If -1, use Tcl_GetErrno() to retrieve the POSIX error code for the
 *	error or condition that occurred.
 *
 * Side effects:
 *	May flush output on the channel.  May cause input to be consumed
 *	from the channel.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_Gets(chan, lineRead)
    Tcl_Channel chan;		/* Channel from which to read. */
    Tcl_DString *lineRead;	/* The line read will be appended to this
				 * DString as UTF-8 characters.  The caller
				 * must have initialized it and is responsible
				 * for managing the storage. */
{
    Tcl_Obj *objPtr;
    int charsStored, length;
    char *string;

    objPtr = Tcl_NewObj();
    charsStored = Tcl_GetsObj(chan, objPtr);
    if (charsStored > 0) {
	string = Tcl_GetStringFromObj(objPtr, &length);
	Tcl_DStringAppend(lineRead, string, length);
    }
    Tcl_DecrRefCount(objPtr);
    return charsStored;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_GetsObj --
 *
 *	Accumulate input from the input channel until end-of-line or
 *	end-of-file has been seen.  Bytes read from the input channel
 *	are converted to UTF-8 using the encoding specified by the
 *	channel.
 *
 * Results:
 *	Number of characters accumulated in the object or -1 if error,
 *	blocked, or EOF.  If -1, use Tcl_GetErrno() to retrieve the
 *	POSIX error code for the error or condition that occurred.
 *
 * Side effects:
 *	Consumes input from the channel.
 *
 *	On reading EOF, leave channel pointing at EOF char.
 *	On reading EOL, leave channel pointing after EOL, but don't
 *	return EOL in dst buffer.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_GetsObj(chan, objPtr)
    Tcl_Channel chan;		/* Channel from which to read. */
    Tcl_Obj *objPtr;		/* The line read will be appended to this
				 * object as UTF-8 characters. */
{
    GetsState gs;
    Channel *chanPtr = (Channel *) chan;
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *bufPtr;
    int inEofChar, skip, copiedTotal;
    Tcl_Encoding encoding;
    char *dst, *dstEnd, *eol, *eof;
    Tcl_EncodingState oldState;
    int oldLength, oldFlags, oldRemoved;

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    if (CheckChannelErrors(statePtr, TCL_READABLE) != 0) {
	copiedTotal = -1;
	goto done;
    }

    bufPtr = statePtr->inQueueHead;
    encoding = statePtr->encoding;

    /*
     * Preserved so we can restore the channel's state in case we don't
     * find a newline in the available input.
     */

    Tcl_GetStringFromObj(objPtr, &oldLength);
    oldFlags = statePtr->inputEncodingFlags;
    oldState = statePtr->inputEncodingState;
    oldRemoved = BUFFER_PADDING;
    if (bufPtr != NULL) {
	oldRemoved = bufPtr->nextRemoved;
    }

    /*
     * If there is no encoding, use "iso8859-1" -- Tcl_GetsObj() doesn't
     * produce ByteArray objects.  To avoid circularity problems,
     * "iso8859-1" is builtin to Tcl.
     */

    if (encoding == NULL) {
	encoding = Tcl_GetEncoding(NULL, "iso8859-1");
    }

    /*
     * Object used by FilterInputBytes to keep track of how much data has
     * been consumed from the channel buffers.
     */

    gs.objPtr		= objPtr;
    gs.dstPtr		= &dst;
    gs.encoding		= encoding;
    gs.bufPtr		= bufPtr;
    gs.state		= oldState;
    gs.rawRead		= 0;
    gs.bytesWrote	= 0;
    gs.charsWrote	= 0;
    gs.totalChars	= 0;

    dst = objPtr->bytes + oldLength;
    dstEnd = dst;

    skip = 0;
    eof = NULL;
    inEofChar = statePtr->inEofChar;

    while (1) {
	if (dst >= dstEnd) {
	    if (FilterInputBytes(chanPtr, &gs) != 0) {
		goto restore;
	    }
	    dstEnd = dst + gs.bytesWrote;
	}
	
	/*
	 * Remember if EOF char is seen, then look for EOL anyhow, because
	 * the EOL might be before the EOF char.
	 */

	if (inEofChar != '\0') {
	    for (eol = dst; eol < dstEnd; eol++) {
		if (*eol == inEofChar) {
		    dstEnd = eol;
		    eof = eol;
		    break;
		}
	    }
	}

	/*
	 * On EOL, leave current file position pointing after the EOL, but
	 * don't store the EOL in the output string.
	 */

	eol = dst;
	switch (statePtr->inputTranslation) {
	    case TCL_TRANSLATE_LF: {
		for (eol = dst; eol < dstEnd; eol++) {
		    if (*eol == '\n') {
			skip = 1;
			goto goteol;
		    }
		}
		break;
	    }
	    case TCL_TRANSLATE_CR: {
		for (eol = dst; eol < dstEnd; eol++) {
		    if (*eol == '\r') {
			skip = 1;
			goto goteol;
		    }
		}
		break;
	    }
	    case TCL_TRANSLATE_CRLF: {
		for (eol = dst; eol < dstEnd; eol++) {
		    if (*eol == '\r') {
			eol++;
			if (eol >= dstEnd) {
			    int offset;
			    
			    offset = eol - objPtr->bytes;
			    dst = dstEnd;
			    if (FilterInputBytes(chanPtr, &gs) != 0) {
				goto restore;
			    }
			    dstEnd = dst + gs.bytesWrote;
			    eol = objPtr->bytes + offset;
			    if (eol >= dstEnd) {
				skip = 0;
				goto goteol;
			    }
			}
			if (*eol == '\n') {
			    eol--;
			    skip = 2;
			    goto goteol;
			}
		    }
		}
		break;
	    }
	    case TCL_TRANSLATE_AUTO: {
		skip = 1;
		if (statePtr->flags & INPUT_SAW_CR) {
		    statePtr->flags &= ~INPUT_SAW_CR;
		    if (*eol == '\n') {
			/*
			 * Skip the raw bytes that make up the '\n'.
			 */

			char tmp[1 + TCL_UTF_MAX];
			int rawRead;

			bufPtr = gs.bufPtr;
			Tcl_ExternalToUtf(NULL, gs.encoding,
				bufPtr->buf + bufPtr->nextRemoved,
				gs.rawRead, statePtr->inputEncodingFlags,
				&gs.state, tmp, 1 + TCL_UTF_MAX, &rawRead,
				NULL, NULL);
			bufPtr->nextRemoved += rawRead;
			gs.rawRead -= rawRead;
			gs.bytesWrote--;
			gs.charsWrote--;
			memmove(dst, dst + 1, (size_t) (dstEnd - dst));
			dstEnd--;
		    }
		}
		for (eol = dst; eol < dstEnd; eol++) {
		    if (*eol == '\r') {
			eol++;
			if (eol == dstEnd) {
			    /*
			     * If buffer ended on \r, peek ahead to see if a
			     * \n is available.
			     */

			    int offset;
			    
			    offset = eol - objPtr->bytes;
			    dst = dstEnd;
			    PeekAhead(chanPtr, &dstEnd, &gs);
			    eol = objPtr->bytes + offset;
			    if (eol >= dstEnd) {
				eol--;
				statePtr->flags |= INPUT_SAW_CR;
				goto goteol;
			    }
			}
			if (*eol == '\n') {
			    skip++;
			}
			eol--;
			goto goteol;
		    } else if (*eol == '\n') {
			goto goteol;
		    }
		}
	    }
	}
	if (eof != NULL) {
	    /*
	     * EOF character was seen.  On EOF, leave current file position
	     * pointing at the EOF character, but don't store the EOF
	     * character in the output string.
	     */

	    dstEnd = eof;
	    statePtr->flags |= (CHANNEL_EOF | CHANNEL_STICKY_EOF);
	    statePtr->inputEncodingFlags |= TCL_ENCODING_END;
	}
	if (statePtr->flags & CHANNEL_EOF) {
	    skip = 0;
	    eol = dstEnd;
	    if (eol == objPtr->bytes) {
		/*
		 * If we didn't produce any bytes before encountering EOF,
		 * caller needs to see -1.
		 */

		Tcl_SetObjLength(objPtr, 0);
		CommonGetsCleanup(chanPtr, encoding);
		copiedTotal = -1;
		goto done;
	    }
	    goto goteol;
	}
	dst = dstEnd;
    }

    /*
     * Found EOL or EOF, but the output buffer may now contain too many
     * UTF-8 characters.  We need to know how many raw bytes correspond to
     * the number of UTF-8 characters we want, plus how many raw bytes
     * correspond to the character(s) making up EOL (if any), so we can
     * remove the correct number of bytes from the channel buffer.
     */
     
    goteol:
    bufPtr = gs.bufPtr;
    statePtr->inputEncodingState = gs.state;
    Tcl_ExternalToUtf(NULL, gs.encoding, bufPtr->buf + bufPtr->nextRemoved,
	    gs.rawRead, statePtr->inputEncodingFlags,
	    &statePtr->inputEncodingState, dst, eol - dst + skip + TCL_UTF_MAX,
	    &gs.rawRead, NULL, &gs.charsWrote);
    bufPtr->nextRemoved += gs.rawRead;

    /*
     * Recycle all the emptied buffers.
     */

    Tcl_SetObjLength(objPtr, eol - objPtr->bytes);
    CommonGetsCleanup(chanPtr, encoding);
    statePtr->flags &= ~CHANNEL_BLOCKED;
    copiedTotal = gs.totalChars + gs.charsWrote - skip;
    goto done;

    /*
     * Couldn't get a complete line.  This only happens if we get a error
     * reading from the channel or we are non-blocking and there wasn't
     * an EOL or EOF in the data available.
     */

    restore:
    bufPtr = statePtr->inQueueHead;
    bufPtr->nextRemoved = oldRemoved;

    for (bufPtr = bufPtr->nextPtr; bufPtr != NULL; bufPtr = bufPtr->nextPtr) {
	bufPtr->nextRemoved = BUFFER_PADDING;
    }
    CommonGetsCleanup(chanPtr, encoding);

    statePtr->inputEncodingState = oldState;
    statePtr->inputEncodingFlags = oldFlags;
    Tcl_SetObjLength(objPtr, oldLength);

    /*
     * We didn't get a complete line so we need to indicate to UpdateInterest
     * that the gets blocked.  It will wait for more data instead of firing
     * a timer, avoiding a busy wait.  This is where we are assuming that the
     * next operation is a gets.  No more file events will be delivered on 
     * this channel until new data arrives or some operation is performed
     * on the channel (e.g. gets, read, fconfigure) that changes the blocking
     * state.  Note that this means a file event will not be delivered even
     * though a read would be able to consume the buffered data.
     */

    statePtr->flags |= CHANNEL_NEED_MORE_DATA;
    copiedTotal = -1;

    done:
    /*
     * Update the notifier state so we don't block while there is still
     * data in the buffers.
     */

    UpdateInterest(chanPtr);
    return copiedTotal;
}

/*
 *---------------------------------------------------------------------------
 *
 * FilterInputBytes --
 *
 *	Helper function for Tcl_GetsObj.  Produces UTF-8 characters from
 *	raw bytes read from the channel.  
 *
 *	Consumes available bytes from channel buffers.  When channel
 *	buffers are exhausted, reads more bytes from channel device into
 *	a new channel buffer.  It is the caller's responsibility to
 *	free the channel buffers that have been exhausted.
 *
 * Results:
 *	The return value is -1 if there was an error reading from the
 *	channel, 0 otherwise.
 *
 * Side effects:
 *	Status object keeps track of how much data from channel buffers
 *	has been consumed and where UTF-8 bytes should be stored.
 *
 *---------------------------------------------------------------------------
 */
 
static int
FilterInputBytes(chanPtr, gsPtr)
    Channel *chanPtr;		/* Channel to read. */
    GetsState *gsPtr;		/* Current state of gets operation. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *bufPtr;
    char *raw, *rawStart, *rawEnd;
    char *dst;
    int offset, toRead, dstNeeded, spaceLeft, result, rawLen, length;
    Tcl_Obj *objPtr;
#define ENCODING_LINESIZE   30	/* Lower bound on how many bytes to convert
				 * at a time.  Since we don't know a priori
				 * how many bytes of storage this many source
				 * bytes will use, we actually need at least
				 * ENCODING_LINESIZE * TCL_MAX_UTF bytes of
				 * room. */

    objPtr = gsPtr->objPtr;

    /*
     * Subtract the number of bytes that were removed from channel buffer
     * during last call.
     */

    bufPtr = gsPtr->bufPtr;
    if (bufPtr != NULL) {
	bufPtr->nextRemoved += gsPtr->rawRead;
	if (bufPtr->nextRemoved >= bufPtr->nextAdded) {
	    bufPtr = bufPtr->nextPtr;
	}
    }
    gsPtr->totalChars += gsPtr->charsWrote;

    if ((bufPtr == NULL) || (bufPtr->nextAdded == BUFFER_PADDING)) {
	/*
	 * All channel buffers were exhausted and the caller still hasn't
	 * seen EOL.  Need to read more bytes from the channel device.
	 * Side effect is to allocate another channel buffer.
	 */
	 
	read:
        if (statePtr->flags & CHANNEL_BLOCKED) {
            if (statePtr->flags & CHANNEL_NONBLOCKING) {
		gsPtr->charsWrote = 0;
		gsPtr->rawRead = 0;
		return -1;
	    }
            statePtr->flags &= ~CHANNEL_BLOCKED;
        }
	if (GetInput(chanPtr) != 0) {
	    gsPtr->charsWrote = 0;
	    gsPtr->rawRead = 0;
	    return -1;
	}
	bufPtr = statePtr->inQueueTail;
	gsPtr->bufPtr = bufPtr;
    }

    /*
     * Convert some of the bytes from the channel buffer to UTF-8.  Space in
     * objPtr's string rep is used to hold the UTF-8 characters.  Grow the
     * string rep if we need more space.
     */

    rawStart = bufPtr->buf + bufPtr->nextRemoved;
    raw = rawStart;
    rawEnd = bufPtr->buf + bufPtr->nextAdded;
    rawLen = rawEnd - rawStart;

    dst = *gsPtr->dstPtr;
    offset = dst - objPtr->bytes;
    toRead = ENCODING_LINESIZE;
    if (toRead > rawLen) {
	toRead = rawLen;
    }
    dstNeeded = toRead * TCL_UTF_MAX + 1;
    spaceLeft = objPtr->length - offset - TCL_UTF_MAX - 1;
    if (dstNeeded > spaceLeft) {
	length = offset * 2;
	if (offset < dstNeeded) {
	    length = offset + dstNeeded;
	}
	length += TCL_UTF_MAX + 1;
	Tcl_SetObjLength(objPtr, length);
	spaceLeft = length - offset;
	dst = objPtr->bytes + offset;
	*gsPtr->dstPtr = dst;
    }
    gsPtr->state = statePtr->inputEncodingState;
    result = Tcl_ExternalToUtf(NULL, gsPtr->encoding, raw, rawLen,
	    statePtr->inputEncodingFlags, &statePtr->inputEncodingState,
	    dst, spaceLeft, &gsPtr->rawRead, &gsPtr->bytesWrote,
	    &gsPtr->charsWrote); 
    if (result == TCL_CONVERT_MULTIBYTE) {
	/*
	 * The last few bytes in this channel buffer were the start of a
	 * multibyte sequence.  If this buffer was full, then move them to
	 * the next buffer so the bytes will be contiguous.  
	 */

	ChannelBuffer *nextPtr;
	int extra;
	
	nextPtr = bufPtr->nextPtr;
	if (bufPtr->nextAdded < bufPtr->bufLength) {
	    if (gsPtr->rawRead > 0) {
		/*
		 * Some raw bytes were converted to UTF-8.  Fall through,
		 * returning those UTF-8 characters because a EOL might be
		 * present in them.
		 */
	    } else if (statePtr->flags & CHANNEL_EOF) {
		/*
		 * There was a partial character followed by EOF on the
		 * device.  Fall through, returning that nothing was found.
		 */

		bufPtr->nextRemoved = bufPtr->nextAdded;
	    } else {
		/*
		 * There are no more cached raw bytes left.  See if we can
		 * get some more.
		 */

		goto read;
	    }
	} else {
	    if (nextPtr == NULL) {
		nextPtr = AllocChannelBuffer(statePtr->bufSize);
		bufPtr->nextPtr = nextPtr;
		statePtr->inQueueTail = nextPtr;
	    }
	    extra = rawLen - gsPtr->rawRead;
	    memcpy((VOID *) (nextPtr->buf + BUFFER_PADDING - extra),
		    (VOID *) (raw + gsPtr->rawRead), (size_t) extra);
	    nextPtr->nextRemoved -= extra;
	    bufPtr->nextAdded -= extra;
	}
    }

    gsPtr->bufPtr = bufPtr;
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * PeekAhead --
 *
 *	Helper function used by Tcl_GetsObj().  Called when we've seen a
 *	\r at the end of the UTF-8 string and want to look ahead one
 *	character to see if it is a \n.
 *
 * Results:
 *	*gsPtr->dstPtr is filled with a pointer to the start of the range of
 *	UTF-8 characters that were found by peeking and *dstEndPtr is filled
 *	with a pointer to the bytes just after the end of the range.
 *
 * Side effects:
 *	If no more raw bytes were available in one of the channel buffers,
 *	tries to perform a non-blocking read to get more bytes from the
 *	channel device.
 *
 *---------------------------------------------------------------------------
 */

static void
PeekAhead(chanPtr, dstEndPtr, gsPtr)
    Channel *chanPtr;		/* The channel to read. */
    char **dstEndPtr;		/* Filled with pointer to end of new range
				 * of UTF-8 characters. */
    GetsState *gsPtr;		/* Current state of gets operation. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *bufPtr;
    Tcl_DriverBlockModeProc *blockModeProc;
    int bytesLeft;

    bufPtr = gsPtr->bufPtr;

    /*
     * If there's any more raw input that's still buffered, we'll peek into
     * that.  Otherwise, only get more data from the channel driver if it
     * looks like there might actually be more data.  The assumption is that
     * if the channel buffer is filled right up to the end, then there
     * might be more data to read.
     */

    blockModeProc = NULL;
    if (bufPtr->nextPtr == NULL) {
	bytesLeft = bufPtr->nextAdded - (bufPtr->nextRemoved + gsPtr->rawRead);
	if (bytesLeft == 0) {
	    if (bufPtr->nextAdded < bufPtr->bufLength) {
		/*
		 * Don't peek ahead if last read was short read.
		 */
		 
		goto cleanup;
	    }
	    if ((statePtr->flags & CHANNEL_NONBLOCKING) == 0) {
		blockModeProc = Tcl_ChannelBlockModeProc(chanPtr->typePtr);
		if (blockModeProc == NULL) {
		    /*
		     * Don't peek ahead if cannot set non-blocking mode.
		     */

		    goto cleanup;
		}
		StackSetBlockMode(chanPtr, TCL_MODE_NONBLOCKING);
	    }
	}
    }
    if (FilterInputBytes(chanPtr, gsPtr) == 0) {
	*dstEndPtr = *gsPtr->dstPtr + gsPtr->bytesWrote;
    }
    if (blockModeProc != NULL) {
	StackSetBlockMode(chanPtr, TCL_MODE_BLOCKING);
    }
    return;

    cleanup:
    bufPtr->nextRemoved += gsPtr->rawRead;
    gsPtr->rawRead = 0;
    gsPtr->totalChars += gsPtr->charsWrote;
    gsPtr->bytesWrote = 0;
    gsPtr->charsWrote = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * CommonGetsCleanup --
 *
 *	Helper function for Tcl_GetsObj() to restore the channel after
 *	a "gets" operation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Encoding may be freed.
 *
 *---------------------------------------------------------------------------
 */
 
static void
CommonGetsCleanup(chanPtr, encoding)
    Channel *chanPtr;
    Tcl_Encoding encoding;
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *bufPtr, *nextPtr;
    
    bufPtr = statePtr->inQueueHead;
    for ( ; bufPtr != NULL; bufPtr = nextPtr) {
	nextPtr = bufPtr->nextPtr;
	if (bufPtr->nextRemoved < bufPtr->nextAdded) {
	    break;
	}
	RecycleBuffer(statePtr, bufPtr, 0);
    }
    statePtr->inQueueHead = bufPtr;
    if (bufPtr == NULL) {
	statePtr->inQueueTail = NULL;
    } else {
	/*
	 * If any multi-byte characters were split across channel buffer
	 * boundaries, the split-up bytes were moved to the next channel
	 * buffer by FilterInputBytes().  Move the bytes back to their
	 * original buffer because the caller could change the channel's
	 * encoding which could change the interpretation of whether those
	 * bytes really made up multi-byte characters after all.
	 */
	 
	nextPtr = bufPtr->nextPtr;
	for ( ; nextPtr != NULL; nextPtr = bufPtr->nextPtr) {
	    int extra;

	    extra = bufPtr->bufLength - bufPtr->nextAdded;
	    if (extra > 0) {
		memcpy((VOID *) (bufPtr->buf + bufPtr->nextAdded),
			(VOID *) (nextPtr->buf + BUFFER_PADDING - extra),
			(size_t) extra);
		bufPtr->nextAdded += extra;
		nextPtr->nextRemoved = BUFFER_PADDING;
	    }
	    bufPtr = nextPtr;
	}
    }
    if (statePtr->encoding == NULL) {
	Tcl_FreeEncoding(encoding);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Read --
 *
 *	Reads a given number of bytes from a channel.  EOL and EOF
 *	translation is done on the bytes being read, so the the number
 *	of bytes consumed from the channel may not be equal to the
 *	number of bytes stored in the destination buffer.
 *
 *	No encoding conversions are applied to the bytes being read.
 *
 * Results:
 *	The number of bytes read, or -1 on error. Use Tcl_GetErrno()
 *	to retrieve the error code for the error that occurred.
 *
 * Side effects:
 *	May cause input to be buffered.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Read(chan, dst, bytesToRead)
    Tcl_Channel chan;		/* The channel from which to read. */
    char *dst;			/* Where to store input read. */
    int bytesToRead;		/* Maximum number of bytes to read. */
{
    Channel *chanPtr = (Channel *) chan;		
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    if (CheckChannelErrors(statePtr, TCL_READABLE) != 0) {
	return -1;
    }

    return DoRead(chanPtr, dst, bytesToRead);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ReadRaw --
 *
 *	Reads a given number of bytes from a channel.  EOL and EOF
 *	translation is done on the bytes being read, so the the number
 *	of bytes consumed from the channel may not be equal to the
 *	number of bytes stored in the destination buffer.
 *
 *	No encoding conversions are applied to the bytes being read.
 *
 * Results:
 *	The number of bytes read, or -1 on error. Use Tcl_GetErrno()
 *	to retrieve the error code for the error that occurred.
 *
 * Side effects:
 *	May cause input to be buffered.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ReadRaw(chan, dst, bytesToRead)
    Tcl_Channel chan;		/* The channel from which to read. */
    char *dst;			/* Where to store input read. */
    int bytesToRead;		/* Maximum number of bytes to read. */
{
    Channel *chanPtr = (Channel *) chan;		
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */

    CopyState*    csPtr;
    int nread, result;

    /* FIX
     * The check below does too much because it will reject a call to this
     * function with a channel which is part of an 'fcopy'. But we have to
     * allow this here or else the chaining in the transformation drivers
     * will fail with 'file busy' error instead of retrieving and
     * transforming the data to copy.
     *
     * We let the check procedure now believe that there is no fcopy in
     * progress. A better solution than this might be an additional flag
     * argument to switch off specific checks.
     */

    csPtr           = statePtr->csPtr;
    statePtr->csPtr = (CopyState*) NULL;

    if (CheckChannelErrors(statePtr, TCL_READABLE) != 0) {
	statePtr->csPtr = csPtr;
	return -1;
    }

    statePtr->csPtr = csPtr;

    /*return DoRead(chanPtr, dst, bytesToRead);*/

    /*
     * Go immediately to the driver, do all the error handling by ourselves.
     * The code was stolen from 'GetInput' and slightly adapted (different
     * return value here).
     */

    nread = (*chanPtr->typePtr->inputProc)(chanPtr->instanceData,
	    dst, bytesToRead, &result);

    if (nread > 0) {
	/*
	 * If we get a short read, signal up that we may be BLOCKED. We
	 * should avoid calling the driver because on some platforms we
	 * will block in the low level reading code even though the
	 * channel is set into nonblocking mode.
	 */
            
	if (nread < bytesToRead) {
	    statePtr->flags |= CHANNEL_BLOCKED;
	}
    } else if (nread == 0) {
	statePtr->flags |= CHANNEL_EOF;
	statePtr->inputEncodingFlags |= TCL_ENCODING_END;
    } else if (nread < 0) {
	if ((result == EWOULDBLOCK) || (result == EAGAIN)) {
	    statePtr->flags |= CHANNEL_BLOCKED;
	    result = EAGAIN;
	}
	Tcl_SetErrno(result);
	return -1;
    } 

    return nread;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_ReadChars --
 *
 *	Reads from the channel until the requested number of characters
 *	have been seen, EOF is seen, or the channel would block.  EOL
 *	and EOF translation is done.  If reading binary data, the raw
 *	bytes are wrapped in a Tcl byte array object.  Otherwise, the raw
 *	bytes are converted to UTF-8 using the channel's current encoding
 *	and stored in a Tcl string object.
 *
 * Results:
 *	The number of characters read, or -1 on error. Use Tcl_GetErrno()
 *	to retrieve the error code for the error that occurred.
 *
 * Side effects:
 *	May cause input to be buffered.
 *
 *---------------------------------------------------------------------------
 */
 
int
Tcl_ReadChars(chan, objPtr, toRead, appendFlag)
    Tcl_Channel chan;		/* The channel to read. */
    Tcl_Obj *objPtr;		/* Input data is stored in this object. */
    int toRead;			/* Maximum number of characters to store,
				 * or -1 to read all available data (up to EOF
				 * or when channel blocks). */
    int appendFlag;		/* If non-zero, data read from the channel
				 * will be appended to the object.  Otherwise,
				 * the data will replace the existing contents
				 * of the object. */

{
    Channel *chanPtr = (Channel *) chan;
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *bufPtr;
    int offset, factor, copied, copiedNow, result;
    Tcl_Encoding encoding;
#define UTF_EXPANSION_FACTOR	1024
    
    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    if (CheckChannelErrors(statePtr, TCL_READABLE) != 0) {
	copied = -1;
	goto done;
    }

    encoding = statePtr->encoding;
    factor = UTF_EXPANSION_FACTOR;

    if (appendFlag == 0) {
	if (encoding == NULL) {
	    Tcl_SetByteArrayLength(objPtr, 0);
	} else {
	    Tcl_SetObjLength(objPtr, 0);
	}
	offset = 0;
    } else {
	if (encoding == NULL) {
	    Tcl_GetByteArrayFromObj(objPtr, &offset);
	} else {
	    Tcl_GetStringFromObj(objPtr, &offset);
	}
    }

    for (copied = 0; (unsigned) toRead > 0; ) {
	copiedNow = -1;
	if (statePtr->inQueueHead != NULL) {
	    if (encoding == NULL) {
		copiedNow = ReadBytes(statePtr, objPtr, toRead, &offset);
	    } else {
		copiedNow = ReadChars(statePtr, objPtr, toRead, &offset,
			&factor);
	    }

	    /*
	     * If the current buffer is empty recycle it.
	     */

	    bufPtr = statePtr->inQueueHead;
	    if (bufPtr->nextRemoved == bufPtr->nextAdded) {
		ChannelBuffer *nextPtr;

		nextPtr = bufPtr->nextPtr;
		RecycleBuffer(statePtr, bufPtr, 0);
		statePtr->inQueueHead = nextPtr;
		if (nextPtr == NULL) {
		    statePtr->inQueueTail = nextPtr;
		}
	    }
	}
	if (copiedNow < 0) {
	    if (statePtr->flags & CHANNEL_EOF) {
		break;
	    }
	    if (statePtr->flags & CHANNEL_BLOCKED) {
		if (statePtr->flags & CHANNEL_NONBLOCKING) {
		    break;
		}
		statePtr->flags &= ~CHANNEL_BLOCKED;
	    }
	    result = GetInput(chanPtr);
	    if (result != 0) {
		if (result == EAGAIN) {
		    break;
		}
		copied = -1;
		goto done;
	    }
	} else {
	    copied += copiedNow;
	    toRead -= copiedNow;
	}
    }
    statePtr->flags &= ~CHANNEL_BLOCKED;
    if (encoding == NULL) {
	Tcl_SetByteArrayLength(objPtr, offset);
    } else {
	Tcl_SetObjLength(objPtr, offset);
    }

    done:
    /*
     * Update the notifier state so we don't block while there is still
     * data in the buffers.
     */

    UpdateInterest(chanPtr);
    return copied;
}
/*
 *---------------------------------------------------------------------------
 *
 * ReadBytes --
 *
 *	Reads from the channel until the requested number of bytes have
 *	been seen, EOF is seen, or the channel would block.  Bytes from
 *	the channel are stored in objPtr as a ByteArray object.  EOL
 *	and EOF translation are done.
 *
 *	'bytesToRead' can safely be a very large number because
 *	space is only allocated to hold data read from the channel
 *	as needed.
 *
 * Results:
 *	The return value is the number of bytes appended to the object
 *	and *offsetPtr is filled with the total number of bytes in the
 *	object (greater than the return value if there were already bytes
 *	in the object).
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static int
ReadBytes(statePtr, objPtr, bytesToRead, offsetPtr)
    ChannelState *statePtr;	/* State of the channel to read. */
    int bytesToRead;		/* Maximum number of characters to store,
				 * or < 0 to get all available characters.
				 * Characters are obtained from the first
				 * buffer in the queue -- even if this number
				 * is larger than the number of characters
				 * available in the first buffer, only the
				 * characters from the first buffer are
				 * returned. */
    Tcl_Obj *objPtr;		/* Input data is appended to this ByteArray
				 * object.  Its length is how much space
				 * has been allocated to hold data, not how
				 * many bytes of data have been stored in the
				 * object. */
    int *offsetPtr;		/* On input, contains how many bytes of
				 * objPtr have been used to hold data.  On
				 * output, filled with how many bytes are now
				 * being used. */
{
    int toRead, srcLen, srcRead, dstWrote, offset, length;
    ChannelBuffer *bufPtr;
    char *src, *dst;

    offset = *offsetPtr;

    bufPtr = statePtr->inQueueHead; 
    src = bufPtr->buf + bufPtr->nextRemoved;
    srcLen = bufPtr->nextAdded - bufPtr->nextRemoved;

    toRead = bytesToRead;
    if ((unsigned) toRead > (unsigned) srcLen) {
	toRead = srcLen;
    }

    dst = (char *) Tcl_GetByteArrayFromObj(objPtr, &length);
    if (toRead > length - offset - 1) {
	/*
	 * Double the existing size of the object or make enough room to
	 * hold all the characters we may get from the source buffer,
	 * whichever is larger.
	 */

	length = offset * 2;
	if (offset < toRead) {
	    length = offset + toRead + 1;
	}
	dst = (char *) Tcl_SetByteArrayLength(objPtr, length);
    }
    dst += offset;

    if (statePtr->flags & INPUT_NEED_NL) {
	statePtr->flags &= ~INPUT_NEED_NL;
	if ((srcLen == 0) || (*src != '\n')) {
	    *dst = '\r';
	    *offsetPtr += 1;
	    return 1;
	}
	*dst++ = '\n';
	src++;
	srcLen--;
	toRead--;
    }

    srcRead = srcLen;
    dstWrote = toRead;
    if (TranslateInputEOL(statePtr, dst, src, &dstWrote, &srcRead) != 0) {
	if (dstWrote == 0) {
	    return -1;
	}
    }
    bufPtr->nextRemoved += srcRead;
    *offsetPtr += dstWrote;
    return dstWrote;
}

/*
 *---------------------------------------------------------------------------
 *
 * ReadChars --
 *
 *	Reads from the channel until the requested number of UTF-8
 *	characters have been seen, EOF is seen, or the channel would
 *	block.  Raw bytes from the channel are converted to UTF-8
 *	and stored in objPtr.  EOL and EOF translation is done.
 *
 *	'charsToRead' can safely be a very large number because
 *	space is only allocated to hold data read from the channel
 *	as needed.
 *
 * Results:
 *	The return value is the number of characters appended to
 *	the object, *offsetPtr is filled with the number of bytes that
 *	were appended, and *factorPtr is filled with the expansion
 *	factor used to guess how many bytes of UTF-8 to allocate to
 *	hold N source bytes.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static int
ReadChars(statePtr, objPtr, charsToRead, offsetPtr, factorPtr)
    ChannelState *statePtr;	/* State of channel to read. */
    int charsToRead;		/* Maximum number of characters to store,
				 * or -1 to get all available characters.
				 * Characters are obtained from the first
				 * buffer in the queue -- even if this number
				 * is larger than the number of characters
				 * available in the first buffer, only the
				 * characters from the first buffer are
				 * returned. */
    Tcl_Obj *objPtr;		/* Input data is appended to this object.
				 * objPtr->length is how much space has been
				 * allocated to hold data, not how many bytes
				 * of data have been stored in the object. */
    int *offsetPtr;		/* On input, contains how many bytes of
				 * objPtr have been used to hold data.  On
				 * output, filled with how many bytes are now
				 * being used. */
    int *factorPtr;		/* On input, contains a guess of how many
				 * bytes need to be allocated to hold the
				 * result of converting N source bytes to
				 * UTF-8.  On output, contains another guess
				 * based on the data seen so far. */
{
    int toRead, factor, offset, spaceLeft, length;
    int srcLen, srcRead, dstNeeded, dstRead, dstWrote, numChars;
    ChannelBuffer *bufPtr;
    char *src, *dst;
    Tcl_EncodingState oldState;

    factor = *factorPtr;
    offset = *offsetPtr;

    bufPtr = statePtr->inQueueHead; 
    src = bufPtr->buf + bufPtr->nextRemoved;
    srcLen = bufPtr->nextAdded - bufPtr->nextRemoved;

    toRead = charsToRead;
    if ((unsigned) toRead > (unsigned) srcLen) {
	toRead = srcLen;
    }

    /*
     * 'factor' is how much we guess that the bytes in the source buffer
     * will expand when converted to UTF-8 chars.  This guess comes from
     * analyzing how many characters were produced by the previous
     * pass.
     */

    dstNeeded = toRead * factor / UTF_EXPANSION_FACTOR;
    spaceLeft = objPtr->length - offset - TCL_UTF_MAX - 1;

    if (dstNeeded > spaceLeft) {
	/*
	 * Double the existing size of the object or make enough room to
	 * hold all the characters we want from the source buffer,
	 * whichever is larger.
	 */

	length = offset * 2;
	if (offset < dstNeeded) {
	    length = offset + dstNeeded;
	}
	spaceLeft = length - offset;
	length += TCL_UTF_MAX + 1;
	Tcl_SetObjLength(objPtr, length);
    }
    if (toRead == srcLen) {
	/*
	 * Want to convert the whole buffer in one pass.  If we have
	 * enough space, convert it using all available space in object
	 * rather than using the factor.
	 */

	dstNeeded = spaceLeft;
    }
    dst = objPtr->bytes + offset;

    oldState = statePtr->inputEncodingState;
    if (statePtr->flags & INPUT_NEED_NL) {
	/*
	 * We want a '\n' because the last character we saw was '\r'.
	 */

	statePtr->flags &= ~INPUT_NEED_NL;
	Tcl_ExternalToUtf(NULL, statePtr->encoding, src, srcLen,
		statePtr->inputEncodingFlags, &statePtr->inputEncodingState,
		dst, TCL_UTF_MAX + 1, &srcRead, &dstWrote, &numChars);
	if ((dstWrote > 0) && (*dst == '\n')) {
	    /*
	     * The next char was a '\n'.  Consume it and produce a '\n'.
	     */

	    bufPtr->nextRemoved += srcRead;
	} else {
	    /*
	     * The next char was not a '\n'.  Produce a '\r'.
	     */

	    *dst = '\r';
	}
	statePtr->inputEncodingFlags &= ~TCL_ENCODING_START;
	*offsetPtr += 1;
        return 1;
    }

    Tcl_ExternalToUtf(NULL, statePtr->encoding, src, srcLen,
	    statePtr->inputEncodingFlags, &statePtr->inputEncodingState, dst,
	    dstNeeded + TCL_UTF_MAX, &srcRead, &dstWrote, &numChars);
    if (srcRead == 0) {
	/*
	 * Not enough bytes in src buffer to make a complete char.  Copy
	 * the bytes to the next buffer to make a new contiguous string,
	 * then tell the caller to fill the buffer with more bytes.
	 */

	ChannelBuffer *nextPtr;
	
	nextPtr = bufPtr->nextPtr;
	if (nextPtr == NULL) {
	    /*
	     * There isn't enough data in the buffers to complete the next
	     * character, so we need to wait for more data before the next
	     * file event can be delivered.
	     */

	    statePtr->flags |= CHANNEL_NEED_MORE_DATA;
	    return -1;
	}
	nextPtr->nextRemoved -= srcLen;
	memcpy((VOID *) (nextPtr->buf + nextPtr->nextRemoved), (VOID *) src,
		(size_t) srcLen);
	RecycleBuffer(statePtr, bufPtr, 0);
	statePtr->inQueueHead = nextPtr;
	return ReadChars(statePtr, objPtr, charsToRead, offsetPtr, factorPtr);
    }

    dstRead = dstWrote;
    if (TranslateInputEOL(statePtr, dst, dst, &dstWrote, &dstRead) != 0) {
	/*
	 * Hit EOF char.  How many bytes of src correspond to where the
	 * EOF was located in dst?
	 */
	 
	if (dstWrote == 0) {
	    return -1;
	}
	statePtr->inputEncodingState = oldState;
	Tcl_ExternalToUtf(NULL, statePtr->encoding, src, srcLen,
		statePtr->inputEncodingFlags, &statePtr->inputEncodingState,
		dst, dstRead + TCL_UTF_MAX, &srcRead, &dstWrote, &numChars);
	TranslateInputEOL(statePtr, dst, dst, &dstWrote, &dstRead);
    } 

    /*
     * The number of characters that we got may be less than the number
     * that we started with because "\r\n" sequences may have been
     * turned into just '\n' in dst.
     */

    numChars -= (dstRead - dstWrote);

    if ((unsigned) numChars > (unsigned) toRead) {
	/*
	 * Got too many chars.
	 */

	char *eof;

	eof = Tcl_UtfAtIndex(dst, toRead);
	statePtr->inputEncodingState = oldState;
	Tcl_ExternalToUtf(NULL, statePtr->encoding, src, srcLen,
		statePtr->inputEncodingFlags, &statePtr->inputEncodingState,
		dst, eof - dst + TCL_UTF_MAX, &srcRead, &dstWrote, &numChars);
	dstRead = dstWrote;
	TranslateInputEOL(statePtr, dst, dst, &dstWrote, &dstRead);
	numChars -= (dstRead - dstWrote);
    }
    statePtr->inputEncodingFlags &= ~TCL_ENCODING_START;

    bufPtr->nextRemoved += srcRead;
    if (dstWrote > srcRead + 1) {
	*factorPtr = dstWrote * UTF_EXPANSION_FACTOR / srcRead;
    }
    *offsetPtr += dstWrote;
    return numChars;
}

/*
 *---------------------------------------------------------------------------
 *
 * TranslateInputEOL --
 *
 *	Perform input EOL and EOF translation on the source buffer,
 *	leaving the translated result in the destination buffer.  
 *
 * Results:
 *	The return value is 1 if the EOF character was found when copying
 *	bytes to the destination buffer, 0 otherwise.  
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static int
TranslateInputEOL(statePtr, dstStart, srcStart, dstLenPtr, srcLenPtr)
    ChannelState *statePtr;	/* Channel being read, for EOL translation
				 * and EOF character. */
    char *dstStart;		/* Output buffer filled with chars by
				 * applying appropriate EOL translation to
				 * source characters. */
    CONST char *srcStart;	/* Source characters. */
    int *dstLenPtr;		/* On entry, the maximum length of output
				 * buffer in bytes; must be <= *srcLenPtr.  On
				 * exit, the number of bytes actually used in
				 * output buffer. */
    int *srcLenPtr;		/* On entry, the length of source buffer.
				 * On exit, the number of bytes read from
				 * the source buffer. */
{
    int dstLen, srcLen, inEofChar;
    CONST char *eof;

    dstLen = *dstLenPtr;

    eof = NULL;
    inEofChar = statePtr->inEofChar;
    if (inEofChar != '\0') {
	/*
	 * Find EOF in translated buffer then compress out the EOL.  The
	 * source buffer may be much longer than the destination buffer --
	 * we only want to return EOF if the EOF has been copied to the
	 * destination buffer.
	 */

	CONST char *src, *srcMax;

	srcMax = srcStart + *srcLenPtr;
	for (src = srcStart; src < srcMax; src++) {
	    if (*src == inEofChar) {
		eof = src;
		srcLen = src - srcStart;
		if (srcLen < dstLen) {
		    dstLen = srcLen;
		}
		*srcLenPtr = srcLen;
		break;
	    }
	}
    }
    switch (statePtr->inputTranslation) {
	case TCL_TRANSLATE_LF: {
	    if (dstStart != srcStart) {
		memcpy((VOID *) dstStart, (VOID *) srcStart, (size_t) dstLen);
	    }
	    srcLen = dstLen;
	    break;
	}
	case TCL_TRANSLATE_CR: {
	    char *dst, *dstEnd;
	    
	    if (dstStart != srcStart) {
		memcpy((VOID *) dstStart, (VOID *) srcStart, (size_t) dstLen);
	    }
	    dstEnd = dstStart + dstLen;
	    for (dst = dstStart; dst < dstEnd; dst++) {
		if (*dst == '\r') {
		    *dst = '\n';
		}
	    }
	    srcLen = dstLen;
	    break;
	}
	case TCL_TRANSLATE_CRLF: {
	    char *dst;
	    CONST char *src, *srcEnd, *srcMax;
	    
	    dst = dstStart;
	    src = srcStart;
	    srcEnd = srcStart + dstLen;
	    srcMax = srcStart + *srcLenPtr;

	    for ( ; src < srcEnd; ) {
		if (*src == '\r') {
		    src++;
		    if (src >= srcMax) {
			statePtr->flags |= INPUT_NEED_NL;
		    } else if (*src == '\n') {
			*dst++ = *src++;
		    } else {
			*dst++ = '\r';
		    }
		} else {
		    *dst++ = *src++;
		}
	    }
	    srcLen = src - srcStart;
	    dstLen = dst - dstStart;
	    break;
	}
	case TCL_TRANSLATE_AUTO: {
	    char *dst;
	    CONST char *src, *srcEnd, *srcMax;

	    dst = dstStart;
	    src = srcStart;
	    srcEnd = srcStart + dstLen;
	    srcMax = srcStart + *srcLenPtr;

	    if ((statePtr->flags & INPUT_SAW_CR) && (src < srcMax)) {
		if (*src == '\n') {
		    src++;
		}
		statePtr->flags &= ~INPUT_SAW_CR;
	    }
	    for ( ; src < srcEnd; ) {
		if (*src == '\r') {
		    src++;
		    if (src >= srcMax) {
			statePtr->flags |= INPUT_SAW_CR;
		    } else if (*src == '\n') {
			if (srcEnd < srcMax) {
			    srcEnd++;
			}
			src++;
		    }
		    *dst++ = '\n';
		} else {
		    *dst++ = *src++;
		}
	    }
	    srcLen = src - srcStart;
	    dstLen = dst - dstStart;
	    break;
	}
	default: {		/* lint. */
	    return 0;
	}
    }
    *dstLenPtr = dstLen;

    if ((eof != NULL) && (srcStart + srcLen >= eof)) {
	/*
	 * EOF character was seen in EOL translated range.  Leave current
	 * file position pointing at the EOF character, but don't store the
	 * EOF character in the output string.
	 */

	statePtr->flags |= (CHANNEL_EOF | CHANNEL_STICKY_EOF);
	statePtr->inputEncodingFlags |= TCL_ENCODING_END;
	statePtr->flags &= ~(INPUT_SAW_CR | INPUT_NEED_NL);
	return 1;
    }

    *srcLenPtr = srcLen;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Ungets --
 *
 *	Causes the supplied string to be added to the input queue of
 *	the channel, at either the head or tail of the queue.
 *
 * Results:
 *	The number of bytes stored in the channel, or -1 on error.
 *
 * Side effects:
 *	Adds input to the input queue of a channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Ungets(chan, str, len, atEnd)
    Tcl_Channel chan;		/* The channel for which to add the input. */
    char *str;			/* The input itself. */
    int len;			/* The length of the input. */
    int atEnd;			/* If non-zero, add at end of queue; otherwise
                                 * add at head of queue. */    
{
    Channel *chanPtr;		/* The real IO channel. */
    ChannelState *statePtr;	/* State of actual channel. */
    ChannelBuffer *bufPtr;	/* Buffer to contain the data. */
    int i, flags;

    chanPtr = (Channel *) chan;
    statePtr = chanPtr->state;

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    /*
     * CheckChannelErrors clears too many flag bits in this one case.
     */
     
    flags = statePtr->flags;
    if (CheckChannelErrors(statePtr, TCL_READABLE) != 0) {
	len = -1;
	goto done;
    }
    statePtr->flags = flags;

    /*
     * If we have encountered a sticky EOF, just punt without storing.
     * (sticky EOF is set if we have seen the input eofChar, to prevent
     * reading beyond the eofChar). Otherwise, clear the EOF flags, and
     * clear the BLOCKED bit. We want to discover these conditions anew
     * in each operation.
     */

    if (statePtr->flags & CHANNEL_STICKY_EOF) {
	goto done;
    }
    statePtr->flags &= (~(CHANNEL_BLOCKED | CHANNEL_EOF));

    bufPtr = AllocChannelBuffer(len);
    for (i = 0; i < len; i++) {
        bufPtr->buf[i] = str[i];
    }
    bufPtr->nextAdded += len;

    if (statePtr->inQueueHead == (ChannelBuffer *) NULL) {
        bufPtr->nextPtr = (ChannelBuffer *) NULL;
        statePtr->inQueueHead = bufPtr;
        statePtr->inQueueTail = bufPtr;
    } else if (atEnd) {
        bufPtr->nextPtr = (ChannelBuffer *) NULL;
        statePtr->inQueueTail->nextPtr = bufPtr;
        statePtr->inQueueTail = bufPtr;
    } else {
        bufPtr->nextPtr = statePtr->inQueueHead;
        statePtr->inQueueHead = bufPtr;
    }

    done:
    /*
     * Update the notifier state so we don't block while there is still
     * data in the buffers.
     */

    UpdateInterest(chanPtr);
    return len;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Flush --
 *
 *	Flushes output data on a channel.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May flush output queued on this channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Flush(chan)
    Tcl_Channel chan;			/* The Channel to flush. */
{
    int result;				/* Of calling FlushChannel. */
    Channel *chanPtr  = (Channel *) chan;	/* The actual channel. */
    ChannelState *statePtr = chanPtr->state;	/* State of actual channel. */

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    if (CheckChannelErrors(statePtr, TCL_WRITABLE) != 0) {
	return -1;
    }

    /*
     * Force current output buffer to be output also.
     */

    if ((statePtr->curOutPtr != NULL)
	    && (statePtr->curOutPtr->nextAdded > 0)) {
        statePtr->flags |= BUFFER_READY;
    }
    
    result = FlushChannel(NULL, chanPtr, 0);
    if (result != 0) {
        return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DiscardInputQueued --
 *
 *	Discards any input read from the channel but not yet consumed
 *	by Tcl reading commands.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May discard input from the channel. If discardLastBuffer is zero,
 *	leaves one buffer in place for back-filling.
 *
 *----------------------------------------------------------------------
 */

static void
DiscardInputQueued(statePtr, discardSavedBuffers)
    ChannelState *statePtr;	/* Channel on which to discard
                                 * the queued input. */
    int discardSavedBuffers;	/* If non-zero, discard all buffers including
                                 * last one. */
{
    ChannelBuffer *bufPtr, *nxtPtr;	/* Loop variables. */

    bufPtr = statePtr->inQueueHead;
    statePtr->inQueueHead = (ChannelBuffer *) NULL;
    statePtr->inQueueTail = (ChannelBuffer *) NULL;
    for (; bufPtr != (ChannelBuffer *) NULL; bufPtr = nxtPtr) {
        nxtPtr = bufPtr->nextPtr;
        RecycleBuffer(statePtr, bufPtr, discardSavedBuffers);
    }

    /*
     * If discardSavedBuffers is nonzero, must also discard any previously
     * saved buffer in the saveInBufPtr field.
     */
    
    if (discardSavedBuffers) {
        if (statePtr->saveInBufPtr != (ChannelBuffer *) NULL) {
            ckfree((char *) statePtr->saveInBufPtr);
            statePtr->saveInBufPtr = (ChannelBuffer *) NULL;
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * GetInput --
 *
 *	Reads input data from a device into a channel buffer.  
 *
 * Results:
 *	The return value is the Posix error code if an error occurred while
 *	reading from the file, or 0 otherwise.  
 *
 * Side effects:
 *	Reads from the underlying device.
 *
 *---------------------------------------------------------------------------
 */

static int
GetInput(chanPtr)
    Channel *chanPtr;		/* Channel to read input from. */
{
    int toRead;			/* How much to read? */
    int result;			/* Of calling driver. */
    int nread;			/* How much was read from channel? */
    ChannelBuffer *bufPtr;	/* New buffer to add to input queue. */
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */

    /*
     * Prevent reading from a dead channel -- a channel that has been closed
     * but not yet deallocated, which can happen if the exit handler for
     * channel cleanup has run but the channel is still registered in some
     * interpreter.
     */
    
    if (CheckForDeadChannel(NULL, statePtr)) {
	return EINVAL;
    }

    /*
     * See if we can fill an existing buffer. If we can, read only
     * as much as will fit in it. Otherwise allocate a new buffer,
     * add it to the input queue and attempt to fill it to the max.
     */

    bufPtr = statePtr->inQueueTail;
    if ((bufPtr != NULL) && (bufPtr->nextAdded < bufPtr->bufLength)) {
        toRead = bufPtr->bufLength - bufPtr->nextAdded;
    } else {
	bufPtr = statePtr->saveInBufPtr;
	statePtr->saveInBufPtr = NULL;
	if (bufPtr == NULL) {
	    bufPtr = AllocChannelBuffer(statePtr->bufSize);
	}
        bufPtr->nextPtr = (ChannelBuffer *) NULL;

        toRead = statePtr->bufSize;
        if (statePtr->inQueueTail == NULL) {
            statePtr->inQueueHead = bufPtr;
        } else {
            statePtr->inQueueTail->nextPtr = bufPtr;
        }
        statePtr->inQueueTail = bufPtr;
    }
      
    /*
     * If EOF is set, we should avoid calling the driver because on some
     * platforms it is impossible to read from a device after EOF.
     */

    if (statePtr->flags & CHANNEL_EOF) {
	return 0;
    }

    nread = (*chanPtr->typePtr->inputProc)(chanPtr->instanceData,
	    bufPtr->buf + bufPtr->nextAdded, toRead, &result);

    if (nread > 0) {
	bufPtr->nextAdded += nread;

	/*
	 * If we get a short read, signal up that we may be BLOCKED. We
	 * should avoid calling the driver because on some platforms we
	 * will block in the low level reading code even though the
	 * channel is set into nonblocking mode.
	 */
            
	if (nread < toRead) {
	    statePtr->flags |= CHANNEL_BLOCKED;
	}
    } else if (nread == 0) {
	statePtr->flags |= CHANNEL_EOF;
	statePtr->inputEncodingFlags |= TCL_ENCODING_END;
    } else if (nread < 0) {
	if ((result == EWOULDBLOCK) || (result == EAGAIN)) {
	    statePtr->flags |= CHANNEL_BLOCKED;
	    result = EAGAIN;
	}
	Tcl_SetErrno(result);
	return result;
    } 
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Seek --
 *
 *	Implements seeking on Tcl Channels. This is a public function
 *	so that other C facilities may be implemented on top of it.
 *
 * Results:
 *	The new access point or -1 on error. If error, use Tcl_GetErrno()
 *	to retrieve the POSIX error code for the error that occurred.
 *
 * Side effects:
 *	May flush output on the channel. May discard queued input.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Seek(chan, offset, mode)
    Tcl_Channel chan;		/* The channel on which to seek. */
    int offset;			/* Offset to seek to. */
    int mode;			/* Relative to which location to seek? */
{
    Channel *chanPtr = (Channel *) chan;	/* The real IO channel. */
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *bufPtr;
    int inputBuffered, outputBuffered;
    int result;			/* Of device driver operations. */
    int curPos;			/* Position on the device. */
    int wasAsync;		/* Was the channel nonblocking before the
                                 * seek operation? If so, must restore to
                                 * nonblocking mode after the seek. */

    if (CheckChannelErrors(statePtr, TCL_WRITABLE | TCL_READABLE) != 0) {
	return -1;
    }

    /*
     * Disallow seek on dead channels -- channels that have been closed but
     * not yet been deallocated. Such channels can be found if the exit
     * handler for channel cleanup has run but the channel is still
     * registered in an interpreter.
     */

    if (CheckForDeadChannel(NULL, statePtr)) return -1;

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    /*
     * Disallow seek on channels whose type does not have a seek procedure
     * defined. This means that the channel does not support seeking.
     */

    if (chanPtr->typePtr->seekProc == (Tcl_DriverSeekProc *) NULL) {
        Tcl_SetErrno(EINVAL);
        return -1;
    }

    /*
     * Compute how much input and output is buffered. If both input and
     * output is buffered, cannot compute the current position.
     */

    for (bufPtr = statePtr->inQueueHead, inputBuffered = 0;
	 bufPtr != (ChannelBuffer *) NULL;
	 bufPtr = bufPtr->nextPtr) {
        inputBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    for (bufPtr = statePtr->outQueueHead, outputBuffered = 0;
	 bufPtr != (ChannelBuffer *) NULL;
	 bufPtr = bufPtr->nextPtr) {
        outputBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    if ((statePtr->curOutPtr != (ChannelBuffer *) NULL) &&
	    (statePtr->curOutPtr->nextAdded > statePtr->curOutPtr->nextRemoved)) {
        statePtr->flags |= BUFFER_READY;
        outputBuffered +=
            (statePtr->curOutPtr->nextAdded - statePtr->curOutPtr->nextRemoved);
    }

    if ((inputBuffered != 0) && (outputBuffered != 0)) {
        Tcl_SetErrno(EFAULT);
        return -1;
    }

    /*
     * If we are seeking relative to the current position, compute the
     * corrected offset taking into account the amount of unread input.
     */

    if (mode == SEEK_CUR) {
        offset -= inputBuffered;
    }

    /*
     * Discard any queued input - this input should not be read after
     * the seek.
     */

    DiscardInputQueued(statePtr, 0);

    /*
     * Reset EOF and BLOCKED flags. We invalidate them by moving the
     * access point. Also clear CR related flags.
     */

    statePtr->flags &=
        (~(CHANNEL_EOF | CHANNEL_STICKY_EOF | CHANNEL_BLOCKED | INPUT_SAW_CR));
    
    /*
     * If the channel is in asynchronous output mode, switch it back
     * to synchronous mode and cancel any async flush that may be
     * scheduled. After the flush, the channel will be put back into
     * asynchronous output mode.
     */

    wasAsync = 0;
    if (statePtr->flags & CHANNEL_NONBLOCKING) {
        wasAsync = 1;
        result = StackSetBlockMode(chanPtr, TCL_MODE_BLOCKING);
	if (result != 0) {
	    return -1;
	}
        statePtr->flags &= (~(CHANNEL_NONBLOCKING));
        if (statePtr->flags & BG_FLUSH_SCHEDULED) {
            statePtr->flags &= (~(BG_FLUSH_SCHEDULED));
        }
    }
    
    /*
     * If the flush fails we cannot recover the original position. In
     * that case the seek is not attempted because we do not know where
     * the access position is - instead we return the error. FlushChannel
     * has already called Tcl_SetErrno() to report the error upwards.
     * If the flush succeeds we do the seek also.
     */
    
    if (FlushChannel(NULL, chanPtr, 0) != 0) {
        curPos = -1;
    } else {

        /*
         * Now seek to the new position in the channel as requested by the
         * caller.
         */

        curPos = (chanPtr->typePtr->seekProc) (chanPtr->instanceData,
                (long) offset, mode, &result);
        if (curPos == -1) {
            Tcl_SetErrno(result);
        }
    }
    
    /*
     * Restore to nonblocking mode if that was the previous behavior.
     *
     * NOTE: Even if there was an async flush active we do not restore
     * it now because we already flushed all the queued output, above.
     */
    
    if (wasAsync) {
        statePtr->flags |= CHANNEL_NONBLOCKING;
        result = StackSetBlockMode(chanPtr, TCL_MODE_NONBLOCKING);
	if (result != 0) {
	    return -1;
	}
    }

    return curPos;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Tell --
 *
 *	Returns the position of the next character to be read/written on
 *	this channel.
 *
 * Results:
 *	A nonnegative integer on success, -1 on failure. If failed,
 *	use Tcl_GetErrno() to retrieve the POSIX error code for the
 *	error that occurred.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Tell(chan)
    Tcl_Channel chan;			/* The channel to return pos for. */
{
    Channel *chanPtr = (Channel *) chan;	/* The real IO channel. */
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *bufPtr;
    int inputBuffered, outputBuffered;
    int result;				/* Of calling device driver. */
    int curPos;				/* Position on device. */

    if (CheckChannelErrors(statePtr, TCL_WRITABLE | TCL_READABLE) != 0) {
	return -1;
    }

    /*
     * Disallow tell on dead channels -- channels that have been closed but
     * not yet been deallocated. Such channels can be found if the exit
     * handler for channel cleanup has run but the channel is still
     * registered in an interpreter.
     */

    if (CheckForDeadChannel(NULL, statePtr)) {
	return -1;
    }

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    /*
     * Disallow tell on channels whose type does not have a seek procedure
     * defined. This means that the channel does not support seeking.
     */

    if (chanPtr->typePtr->seekProc == (Tcl_DriverSeekProc *) NULL) {
        Tcl_SetErrno(EINVAL);
        return -1;
    }

    /*
     * Compute how much input and output is buffered. If both input and
     * output is buffered, cannot compute the current position.
     */

    for (bufPtr = statePtr->inQueueHead, inputBuffered = 0;
	 bufPtr != (ChannelBuffer *) NULL;
	 bufPtr = bufPtr->nextPtr) {
        inputBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    for (bufPtr = statePtr->outQueueHead, outputBuffered = 0;
	 bufPtr != (ChannelBuffer *) NULL;
	 bufPtr = bufPtr->nextPtr) {
        outputBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    if ((statePtr->curOutPtr != (ChannelBuffer *) NULL) &&
	    (statePtr->curOutPtr->nextAdded > statePtr->curOutPtr->nextRemoved)) {
        statePtr->flags |= BUFFER_READY;
        outputBuffered +=
            (statePtr->curOutPtr->nextAdded - statePtr->curOutPtr->nextRemoved);
    }

    if ((inputBuffered != 0) && (outputBuffered != 0)) {
        Tcl_SetErrno(EFAULT);
        return -1;
    }

    /*
     * Get the current position in the device and compute the position
     * where the next character will be read or written.
     */

    curPos = (chanPtr->typePtr->seekProc) (chanPtr->instanceData,
            (long) 0, SEEK_CUR, &result);
    if (curPos == -1) {
        Tcl_SetErrno(result);
        return -1;
    }
    if (inputBuffered != 0) {
        return (curPos - inputBuffered);
    }
    return (curPos + outputBuffered);
}

/*
 *---------------------------------------------------------------------------
 *
 * CheckChannelErrors --
 *
 *	See if the channel is in an ready state and can perform the
 *	desired operation.
 *
 * Results:
 *	The return value is 0 if the channel is OK, otherwise the
 *	return value is -1 and errno is set to indicate the error.
 *
 * Side effects:
 *	May clear the EOF and/or BLOCKED bits if reading from channel.
 *
 *---------------------------------------------------------------------------
 */
 
static int
CheckChannelErrors(statePtr, direction)
    ChannelState *statePtr;	/* Channel to check. */
    int direction;		/* Test if channel supports desired operation:
				 * TCL_READABLE, TCL_WRITABLE. */
{
    /*
     * Check for unreported error.
     */

    if (statePtr->unreportedError != 0) {
        Tcl_SetErrno(statePtr->unreportedError);
        statePtr->unreportedError = 0;
        return -1;
    }

    /*
     * Fail if the channel is not opened for desired operation.
     */

    if ((statePtr->flags & direction) == 0) {
        Tcl_SetErrno(EACCES);
        return -1;
    }

    /*
     * Fail if the channel is in the middle of a background copy.
     */

    if (statePtr->csPtr != NULL) {
	Tcl_SetErrno(EBUSY);
	return -1;
    }

    if (direction == TCL_READABLE) {
	/*
	 * If we have not encountered a sticky EOF, clear the EOF bit
	 * (sticky EOF is set if we have seen the input eofChar, to prevent
	 * reading beyond the eofChar). Also, always clear the BLOCKED bit.
	 * We want to discover these conditions anew in each operation.
	 */
	
	if ((statePtr->flags & CHANNEL_STICKY_EOF) == 0) {
	    statePtr->flags &= ~CHANNEL_EOF;
	}
	statePtr->flags &= ~(CHANNEL_BLOCKED | CHANNEL_NEED_MORE_DATA);
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Eof --
 *
 *	Returns 1 if the channel is at EOF, 0 otherwise.
 *
 * Results:
 *	1 or 0, always.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Eof(chan)
    Tcl_Channel chan;			/* Does this channel have EOF? */
{
    ChannelState *statePtr = ((Channel *) chan)->state;
					/* State of real channel structure. */

    return ((statePtr->flags & CHANNEL_STICKY_EOF) ||
            ((statePtr->flags & CHANNEL_EOF) &&
		    (Tcl_InputBuffered(chan) == 0))) ? 1 : 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InputBlocked --
 *
 *	Returns 1 if input is blocked on this channel, 0 otherwise.
 *
 * Results:
 *	0 or 1, always.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_InputBlocked(chan)
    Tcl_Channel chan;			/* Is this channel blocked? */
{
    ChannelState *statePtr = ((Channel *) chan)->state;
					/* State of real channel structure. */

    return (statePtr->flags & CHANNEL_BLOCKED) ? 1 : 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InputBuffered --
 *
 *	Returns the number of bytes of input currently buffered in the
 *	internal buffer of a channel.
 *
 * Results:
 *	The number of input bytes buffered, or zero if the channel is not
 *	open for reading.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_InputBuffered(chan)
    Tcl_Channel chan;			/* The channel to query. */
{
    ChannelState *statePtr = ((Channel *) chan)->state;
					/* State of real channel structure. */
    ChannelBuffer *bufPtr;
    int bytesBuffered;

    for (bytesBuffered = 0, bufPtr = statePtr->inQueueHead;
	 bufPtr != (ChannelBuffer *) NULL;
	 bufPtr = bufPtr->nextPtr) {
        bytesBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    return bytesBuffered;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetChannelBufferSize --
 *
 *	Sets the size of buffers to allocate to store input or output
 *	in the channel. The size must be between 10 bytes and 1 MByte.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the size of buffers subsequently allocated for this channel.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetChannelBufferSize(chan, sz)
    Tcl_Channel chan;			/* The channel whose buffer size
                                         * to set. */
    int sz;				/* The size to set. */
{
    ChannelState *statePtr;		/* State of real channel structure. */
    
    /*
     * If the buffer size is smaller than 10 bytes or larger than one MByte,
     * do not accept the requested size and leave the current buffer size.
     */
    
    if (sz < 10) {
        return;
    }
    if (sz > (1024 * 1024)) {
        return;
    }

    statePtr = ((Channel *) chan)->state;
    statePtr->bufSize = sz;

    if (statePtr->outputStage != NULL) {
	ckfree((char *) statePtr->outputStage);
	statePtr->outputStage = NULL;
    }
    if ((statePtr->encoding != NULL) && (statePtr->flags & TCL_WRITABLE)) {
	statePtr->outputStage = (char *)
	    ckalloc((unsigned) (statePtr->bufSize + 2));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelBufferSize --
 *
 *	Retrieves the size of buffers to allocate for this channel.
 *
 * Results:
 *	The size.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetChannelBufferSize(chan)
    Tcl_Channel chan;		/* The channel for which to find the
                                 * buffer size. */
{
    ChannelState *statePtr = ((Channel *) chan)->state;
					/* State of real channel structure. */

    return statePtr->bufSize;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BadChannelOption --
 *
 *	This procedure generates a "bad option" error message in an
 *	(optional) interpreter.  It is used by channel drivers when 
 *      a invalid Set/Get option is requested. Its purpose is to concatenate
 *      the generic options list to the specific ones and factorize
 *      the generic options error message string.
 *
 * Results:
 *	TCL_ERROR.
 *
 * Side effects:
 *	An error message is generated in interp's result object to
 *	indicate that a command was invoked with the a bad option
 *	The message has the form
 *		bad option "blah": should be one of 
 *              <...generic options...>+<...specific options...>
 *	"blah" is the optionName argument and "<specific options>"
 *	is a space separated list of specific option words.
 *      The function takes good care of inserting minus signs before
 *      each option, commas after, and an "or" before the last option.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_BadChannelOption(interp, optionName, optionList)
    Tcl_Interp *interp;			/* Current interpreter. (can be NULL)*/
    char *optionName;			/* 'bad option' name */
    char *optionList;			/* Specific options list to append 
					 * to the standard generic options.
					 * can be NULL for generic options 
					 * only.
					 */
{
    if (interp) {
	CONST char *genericopt = 
	    "blocking buffering buffersize encoding eofchar translation";
	char **argv;
	int  argc, i;
	Tcl_DString ds;

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, (char *) genericopt, -1);
	if (optionList && (*optionList)) {
	    Tcl_DStringAppend(&ds, " ", 1);
	    Tcl_DStringAppend(&ds, optionList, -1);
	}
	if (Tcl_SplitList(interp, Tcl_DStringValue(&ds), 
		&argc, &argv) != TCL_OK) {
	    panic("malformed option list in channel driver");
	}
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "bad option \"", optionName, 
		"\": should be one of ", (char *) NULL);
	argc--;
	for (i = 0; i < argc; i++) {
	    Tcl_AppendResult(interp, "-", argv[i], ", ", (char *) NULL);
	}
	Tcl_AppendResult(interp, "or -", argv[i], (char *) NULL);
	Tcl_DStringFree(&ds);
	ckfree((char *) argv);
    }
    Tcl_SetErrno(EINVAL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelOption --
 *
 *	Gets a mode associated with an IO channel. If the optionName arg
 *	is non NULL, retrieves the value of that option. If the optionName
 *	arg is NULL, retrieves a list of alternating option names and
 *	values for the given channel.
 *
 * Results:
 *	A standard Tcl result. Also sets the supplied DString to the
 *	string value of the option(s) returned.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetChannelOption(interp, chan, optionName, dsPtr)
    Tcl_Interp *interp;		/* For error reporting - can be NULL. */
    Tcl_Channel chan;		/* Channel on which to get option. */
    char *optionName;		/* Option to get. */
    Tcl_DString *dsPtr;		/* Where to store value(s). */
{
    size_t len;			/* Length of optionName string. */
    char optionVal[128];	/* Buffer for sprintf. */
    Channel *chanPtr = (Channel *) chan;
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    int flags;

    /*
     * Disallow options on dead channels -- channels that have been closed but
     * not yet been deallocated. Such channels can be found if the exit
     * handler for channel cleanup has run but the channel is still
     * registered in an interpreter.
     */

    if (CheckForDeadChannel(interp, statePtr)) {
	return TCL_ERROR;
    }

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    /*
     * If we are in the middle of a background copy, use the saved flags.
     */

    if (statePtr->csPtr) {
	if (chanPtr == statePtr->csPtr->readPtr) {
	    flags = statePtr->csPtr->readFlags;
	} else {
	    flags = statePtr->csPtr->writeFlags;
	}
    } else {
	flags = statePtr->flags;
    }

    /*
     * If the optionName is NULL it means that we want a list of all
     * options and values.
     */
    
    if (optionName == (char *) NULL) {
        len = 0;
    } else {
        len = strlen(optionName);
    }
    
    if ((len == 0) || ((len > 2) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-blocking", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-blocking");
        }
        Tcl_DStringAppendElement(dsPtr,
		(flags & CHANNEL_NONBLOCKING) ? "0" : "1");
        if (len > 0) {
            return TCL_OK;
        }
    }
    if ((len == 0) || ((len > 7) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-buffering", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-buffering");
        }
        if (flags & CHANNEL_LINEBUFFERED) {
            Tcl_DStringAppendElement(dsPtr, "line");
        } else if (flags & CHANNEL_UNBUFFERED) {
            Tcl_DStringAppendElement(dsPtr, "none");
        } else {
            Tcl_DStringAppendElement(dsPtr, "full");
        }
        if (len > 0) {
            return TCL_OK;
        }
    }
    if ((len == 0) || ((len > 7) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-buffersize", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-buffersize");
        }
        TclFormatInt(optionVal, statePtr->bufSize);
        Tcl_DStringAppendElement(dsPtr, optionVal);
        if (len > 0) {
            return TCL_OK;
        }
    }
    if ((len == 0) ||
	    ((len > 2) && (optionName[1] == 'e') &&
		    (strncmp(optionName, "-encoding", len) == 0))) {
	if (len == 0) {
	    Tcl_DStringAppendElement(dsPtr, "-encoding");
	}
	if (statePtr->encoding == NULL) {
	    Tcl_DStringAppendElement(dsPtr, "binary");
	} else {
	    Tcl_DStringAppendElement(dsPtr,
		    Tcl_GetEncodingName(statePtr->encoding));
	}
	if (len > 0) {
	    return TCL_OK;
	}
    }
    if ((len == 0) ||
            ((len > 2) && (optionName[1] == 'e') &&
                    (strncmp(optionName, "-eofchar", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-eofchar");
        }
        if (((flags & (TCL_READABLE|TCL_WRITABLE)) ==
                (TCL_READABLE|TCL_WRITABLE)) && (len == 0)) {
            Tcl_DStringStartSublist(dsPtr);
        }
        if (flags & TCL_READABLE) {
            if (statePtr->inEofChar == 0) {
                Tcl_DStringAppendElement(dsPtr, "");
            } else {
                char buf[4];

                sprintf(buf, "%c", statePtr->inEofChar);
                Tcl_DStringAppendElement(dsPtr, buf);
            }
        }
        if (flags & TCL_WRITABLE) {
            if (statePtr->outEofChar == 0) {
                Tcl_DStringAppendElement(dsPtr, "");
            } else {
                char buf[4];

                sprintf(buf, "%c", statePtr->outEofChar);
                Tcl_DStringAppendElement(dsPtr, buf);
            }
        }
        if (((flags & (TCL_READABLE|TCL_WRITABLE)) ==
                (TCL_READABLE|TCL_WRITABLE)) && (len == 0)) {
            Tcl_DStringEndSublist(dsPtr);
        }
        if (len > 0) {
            return TCL_OK;
        }
    }
    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 't') &&
                    (strncmp(optionName, "-translation", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-translation");
        }
        if (((flags & (TCL_READABLE|TCL_WRITABLE)) ==
                (TCL_READABLE|TCL_WRITABLE)) && (len == 0)) {
            Tcl_DStringStartSublist(dsPtr);
        }
        if (flags & TCL_READABLE) {
            if (statePtr->inputTranslation == TCL_TRANSLATE_AUTO) {
                Tcl_DStringAppendElement(dsPtr, "auto");
            } else if (statePtr->inputTranslation == TCL_TRANSLATE_CR) {
                Tcl_DStringAppendElement(dsPtr, "cr");
            } else if (statePtr->inputTranslation == TCL_TRANSLATE_CRLF) {
                Tcl_DStringAppendElement(dsPtr, "crlf");
            } else {
                Tcl_DStringAppendElement(dsPtr, "lf");
            }
        }
        if (flags & TCL_WRITABLE) {
            if (statePtr->outputTranslation == TCL_TRANSLATE_AUTO) {
                Tcl_DStringAppendElement(dsPtr, "auto");
            } else if (statePtr->outputTranslation == TCL_TRANSLATE_CR) {
                Tcl_DStringAppendElement(dsPtr, "cr");
            } else if (statePtr->outputTranslation == TCL_TRANSLATE_CRLF) {
                Tcl_DStringAppendElement(dsPtr, "crlf");
            } else {
                Tcl_DStringAppendElement(dsPtr, "lf");
            }
        }
        if (((flags & (TCL_READABLE|TCL_WRITABLE)) ==
                (TCL_READABLE|TCL_WRITABLE)) && (len == 0)) {
            Tcl_DStringEndSublist(dsPtr);
        }
        if (len > 0) {
            return TCL_OK;
        }
    }
    if (chanPtr->typePtr->getOptionProc != (Tcl_DriverGetOptionProc *) NULL) {
	/*
	 * let the driver specific handle additional options
	 * and result code and message.
	 */

        return (chanPtr->typePtr->getOptionProc) (chanPtr->instanceData,
		interp, optionName, dsPtr);
    } else {
	/*
	 * no driver specific options case.
	 */

        if (len == 0) {
            return TCL_OK;
        }
	return Tcl_BadChannelOption(interp, optionName, NULL);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_SetChannelOption --
 *
 *	Sets an option on a channel.
 *
 * Results:
 *	A standard Tcl result.  On error, sets interp's result object
 *	if interp is not NULL.
 *
 * Side effects:
 *	May modify an option on a device.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_SetChannelOption(interp, chan, optionName, newValue)
    Tcl_Interp *interp;		/* For error reporting - can be NULL. */
    Tcl_Channel chan;		/* Channel on which to set mode. */
    char *optionName;		/* Which option to set? */
    char *newValue;		/* New value for option. */
{
    int newMode;		/* New (numeric) mode to sert. */
    Channel *chanPtr = (Channel *) chan;	/* The real IO channel. */
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    size_t len;			/* Length of optionName string. */
    int argc;
    char **argv;

    /*
     * If the channel is in the middle of a background copy, fail.
     */

    if (statePtr->csPtr) {
	if (interp) {
	    Tcl_AppendResult(interp,
		    "unable to set channel options: background copy in progress",
		    (char *) NULL);
	}
        return TCL_ERROR;
    }

    /*
     * Disallow options on dead channels -- channels that have been closed but
     * not yet been deallocated. Such channels can be found if the exit
     * handler for channel cleanup has run but the channel is still
     * registered in an interpreter.
     */

    if (CheckForDeadChannel(NULL, statePtr)) {
	return TCL_ERROR;
    }

    /*
     * This operation should occur at the top of a channel stack.
     */

    chanPtr = statePtr->topChanPtr;

    len = strlen(optionName);

    if ((len > 2) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-blocking", len) == 0)) {
        if (Tcl_GetBoolean(interp, newValue, &newMode) == TCL_ERROR) {
            return TCL_ERROR;
        }
        if (newMode) {
            newMode = TCL_MODE_BLOCKING;
        } else {
            newMode = TCL_MODE_NONBLOCKING;
        }
	return SetBlockMode(interp, chanPtr, newMode);
    } else if ((len > 7) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-buffering", len) == 0)) {
        len = strlen(newValue);
        if ((newValue[0] == 'f') && (strncmp(newValue, "full", len) == 0)) {
            statePtr->flags &=
                (~(CHANNEL_UNBUFFERED|CHANNEL_LINEBUFFERED));
        } else if ((newValue[0] == 'l') &&
                (strncmp(newValue, "line", len) == 0)) {
            statePtr->flags &= (~(CHANNEL_UNBUFFERED));
            statePtr->flags |= CHANNEL_LINEBUFFERED;
        } else if ((newValue[0] == 'n') &&
                (strncmp(newValue, "none", len) == 0)) {
            statePtr->flags &= (~(CHANNEL_LINEBUFFERED));
            statePtr->flags |= CHANNEL_UNBUFFERED;
        } else {
            if (interp) {
                Tcl_AppendResult(interp, "bad value for -buffering: ",
                        "must be one of full, line, or none",
                        (char *) NULL);
                return TCL_ERROR;
            }
        }
	return TCL_OK;
    } else if ((len > 7) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-buffersize", len) == 0)) {
        statePtr->bufSize = atoi(newValue);	/* INTL: "C", UTF safe. */
        if ((statePtr->bufSize < 10) || (statePtr->bufSize > (1024 * 1024))) {
            statePtr->bufSize = CHANNELBUFFER_DEFAULT_SIZE;
        }
    } else if ((len > 2) && (optionName[1] == 'e') &&
	    (strncmp(optionName, "-encoding", len) == 0)) {
	Tcl_Encoding encoding;

	if ((newValue[0] == '\0') || (strcmp(newValue, "binary") == 0)) {
	    encoding = NULL;
	} else {
	    encoding = Tcl_GetEncoding(interp, newValue);
	    if (encoding == NULL) {
		return TCL_ERROR;
	    }
	}
	Tcl_FreeEncoding(statePtr->encoding);
	statePtr->encoding = encoding;
	statePtr->inputEncodingState = NULL;
	statePtr->inputEncodingFlags = TCL_ENCODING_START;
	statePtr->outputEncodingState = NULL;
	statePtr->outputEncodingFlags = TCL_ENCODING_START;
	statePtr->flags &= ~CHANNEL_NEED_MORE_DATA;
	UpdateInterest(chanPtr);
    } else if ((len > 2) && (optionName[1] == 'e') &&
            (strncmp(optionName, "-eofchar", len) == 0)) {
        if (Tcl_SplitList(interp, newValue, &argc, &argv) == TCL_ERROR) {
            return TCL_ERROR;
        }
        if (argc == 0) {
            statePtr->inEofChar = 0;
            statePtr->outEofChar = 0;
        } else if (argc == 1) {
            if (statePtr->flags & TCL_WRITABLE) {
                statePtr->outEofChar = (int) argv[0][0];
            }
            if (statePtr->flags & TCL_READABLE) {
                statePtr->inEofChar = (int) argv[0][0];
            }
        } else if (argc != 2) {
            if (interp) {
                Tcl_AppendResult(interp,
                        "bad value for -eofchar: should be a list of one or",
                        " two elements", (char *) NULL);
            }
            ckfree((char *) argv);
            return TCL_ERROR;
        } else {
            if (statePtr->flags & TCL_READABLE) {
                statePtr->inEofChar = (int) argv[0][0];
            }
            if (statePtr->flags & TCL_WRITABLE) {
                statePtr->outEofChar = (int) argv[1][0];
            }
        }
        if (argv != (char **) NULL) {
            ckfree((char *) argv);
        }
	return TCL_OK;
    } else if ((len > 1) && (optionName[1] == 't') &&
            (strncmp(optionName, "-translation", len) == 0)) {
	char *readMode, *writeMode;

        if (Tcl_SplitList(interp, newValue, &argc, &argv) == TCL_ERROR) {
            return TCL_ERROR;
        }

        if (argc == 1) {
	    readMode = (statePtr->flags & TCL_READABLE) ? argv[0] : NULL;
	    writeMode = (statePtr->flags & TCL_WRITABLE) ? argv[0] : NULL;
	} else if (argc == 2) {
	    readMode = (statePtr->flags & TCL_READABLE) ? argv[0] : NULL;
	    writeMode = (statePtr->flags & TCL_WRITABLE) ? argv[1] : NULL;
	} else {
            if (interp) {
                Tcl_AppendResult(interp,
                        "bad value for -translation: must be a one or two",
                        " element list", (char *) NULL);
            }
            ckfree((char *) argv);
            return TCL_ERROR;
	}

	if (readMode) {
	    if (*readMode == '\0') {
		newMode = statePtr->inputTranslation;
	    } else if (strcmp(readMode, "auto") == 0) {
		newMode = TCL_TRANSLATE_AUTO;
	    } else if (strcmp(readMode, "binary") == 0) {
		newMode = TCL_TRANSLATE_LF;
		statePtr->inEofChar = 0;
		Tcl_FreeEncoding(statePtr->encoding);		    
		statePtr->encoding = NULL;
	    } else if (strcmp(readMode, "lf") == 0) {
		newMode = TCL_TRANSLATE_LF;
	    } else if (strcmp(readMode, "cr") == 0) {
		newMode = TCL_TRANSLATE_CR;
	    } else if (strcmp(readMode, "crlf") == 0) {
		newMode = TCL_TRANSLATE_CRLF;
	    } else if (strcmp(readMode, "platform") == 0) {
		newMode = TCL_PLATFORM_TRANSLATION;
	    } else {
		if (interp) {
		    Tcl_AppendResult(interp,
			    "bad value for -translation: ",
			    "must be one of auto, binary, cr, lf, crlf,",
			    " or platform", (char *) NULL);
		}
		ckfree((char *) argv);
		return TCL_ERROR;
	    }

	    /*
	     * Reset the EOL flags since we need to look at any buffered
	     * data to see if the new translation mode allows us to
	     * complete the line.
	     */

	    if (newMode != statePtr->inputTranslation) {
		statePtr->inputTranslation = (Tcl_EolTranslation) newMode;
		statePtr->flags &= ~(INPUT_SAW_CR);
		statePtr->flags &= ~(CHANNEL_NEED_MORE_DATA);
		UpdateInterest(chanPtr);
	    }
	}
	if (writeMode) {
	    if (*writeMode == '\0') {
		/* Do nothing. */
	    } else if (strcmp(writeMode, "auto") == 0) {
		/*
		 * This is a hack to get TCP sockets to produce output
		 * in CRLF mode if they are being set into AUTO mode.
		 * A better solution for achieving this effect will be
		 * coded later.
		 */

		if (strcmp(chanPtr->typePtr->typeName, "tcp") == 0) {
		    statePtr->outputTranslation = TCL_TRANSLATE_CRLF;
		} else {
		    statePtr->outputTranslation = TCL_PLATFORM_TRANSLATION;
		}
	    } else if (strcmp(writeMode, "binary") == 0) {
		statePtr->outEofChar = 0;
		statePtr->outputTranslation = TCL_TRANSLATE_LF;
		Tcl_FreeEncoding(statePtr->encoding);		    
		statePtr->encoding = NULL;
	    } else if (strcmp(writeMode, "lf") == 0) {
		statePtr->outputTranslation = TCL_TRANSLATE_LF;
	    } else if (strcmp(writeMode, "cr") == 0) {
		statePtr->outputTranslation = TCL_TRANSLATE_CR;
	    } else if (strcmp(writeMode, "crlf") == 0) {
		statePtr->outputTranslation = TCL_TRANSLATE_CRLF;
	    } else if (strcmp(writeMode, "platform") == 0) {
		statePtr->outputTranslation = TCL_PLATFORM_TRANSLATION;
	    } else {
		if (interp) {
		    Tcl_AppendResult(interp,
			    "bad value for -translation: ",
			    "must be one of auto, binary, cr, lf, crlf,",
			    " or platform", (char *) NULL);
		}
		ckfree((char *) argv);
		return TCL_ERROR;
	    }
	}
        ckfree((char *) argv);            
        return TCL_OK;
    } else if (chanPtr->typePtr->setOptionProc != NULL) {
        return (*chanPtr->typePtr->setOptionProc)(chanPtr->instanceData,
                interp, optionName, newValue);
    } else {
	return Tcl_BadChannelOption(interp, optionName, (char *) NULL);
    }

    /*
     * If bufsize changes, need to get rid of old utility buffer.
     */

    if (statePtr->saveInBufPtr != NULL) {
	RecycleBuffer(statePtr, statePtr->saveInBufPtr, 1);
	statePtr->saveInBufPtr = NULL;
    }
    if (statePtr->inQueueHead != NULL) {
	if ((statePtr->inQueueHead->nextPtr == NULL)
		&& (statePtr->inQueueHead->nextAdded ==
			statePtr->inQueueHead->nextRemoved)) {
	    RecycleBuffer(statePtr, statePtr->inQueueHead, 1);
	    statePtr->inQueueHead = NULL;
	    statePtr->inQueueTail = NULL;
	}
    }

    /*
     * If encoding or bufsize changes, need to update output staging buffer.
     */

    if (statePtr->outputStage != NULL) {
	ckfree((char *) statePtr->outputStage);
	statePtr->outputStage = NULL;
    }
    if ((statePtr->encoding != NULL) && (statePtr->flags & TCL_WRITABLE)) {
	statePtr->outputStage = (char *) 
	    ckalloc((unsigned) (statePtr->bufSize + 2));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupChannelHandlers --
 *
 *	Removes channel handlers that refer to the supplied interpreter,
 *	so that if the actual channel is not closed now, these handlers
 *	will not run on subsequent events on the channel. This would be
 *	erroneous, because the interpreter no longer has a reference to
 *	this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes channel handlers.
 *
 *----------------------------------------------------------------------
 */

static void
CleanupChannelHandlers(interp, chanPtr)
    Tcl_Interp *interp;
    Channel *chanPtr;
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    EventScriptRecord *sPtr, *prevPtr, *nextPtr;

    /*
     * Remove fileevent records on this channel that refer to the
     * given interpreter.
     */
    
    for (sPtr = statePtr->scriptRecordPtr,
             prevPtr = (EventScriptRecord *) NULL;
	 sPtr != (EventScriptRecord *) NULL;
	 sPtr = nextPtr) {
        nextPtr = sPtr->nextPtr;
        if (sPtr->interp == interp) {
            if (prevPtr == (EventScriptRecord *) NULL) {
                statePtr->scriptRecordPtr = nextPtr;
            } else {
                prevPtr->nextPtr = nextPtr;
            }

            Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                    ChannelEventScriptInvoker, (ClientData) sPtr);

	    Tcl_DecrRefCount(sPtr->scriptPtr);
            ckfree((char *) sPtr);
        } else {
            prevPtr = sPtr;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NotifyChannel --
 *
 *	This procedure is called by a channel driver when a driver
 *	detects an event on a channel.  This procedure is responsible
 *	for actually handling the event by invoking any channel
 *	handler callbacks.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Whatever the channel handler callback procedure does.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_NotifyChannel(channel, mask)
    Tcl_Channel channel;	/* Channel that detected an event. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION: indicates
				 * which events were detected. */
{
    Channel *chanPtr = (Channel *) channel;
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelHandler *chPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    NextChannelHandler nh;
#ifdef TCL_CHANNEL_VERSION_2
    Channel* upChanPtr;
    Tcl_ChannelType* upTypePtr;

    /*
     * In contrast to the other API functions this procedure walks towards
     * the top of a stack and not down from it.
     *
     * The channel calling this procedure is the one who generated the event,
     * and thus does not take part in handling it. IOW, its HandlerProc is
     * not called, instead we begin with the channel above it.
     *
     * This behaviour also allows the transformation channels to
     * generate their own events and pass them upward.
     */

    while (mask && (chanPtr->upChanPtr != ((Channel*) NULL))) {
        upChanPtr = chanPtr->upChanPtr;
	upTypePtr = upChanPtr->typePtr;

	if ((Tcl_ChannelVersion(upTypePtr) == TCL_CHANNEL_VERSION_2) &&
		(Tcl_ChannelHandlerProc(upTypePtr) !=
			((Tcl_DriverHandlerProc *) NULL))) {

	    Tcl_DriverHandlerProc* handlerProc =
		Tcl_ChannelHandlerProc(upTypePtr);

	  mask = (*handlerProc) (upChanPtr->instanceData, mask);
	}

	/* ELSE:
	 * Ignore transformations which are unable to handle the event
	 * coming from below. Assume that they don't change the mask and
	 * pass it on.
	 */

	chanPtr = upChanPtr;
    }

    channel = (Tcl_Channel) chanPtr;

    /*
     * Here we have either reached the top of the stack or the mask is
     * empty.  We break out of the procedure if it is the latter.
     */

    if (!mask) {
        return;
    }

    /*
     * We are now above the topmost channel in a stack and have events
     * left. Now call the channel handlers as usual.
     *
     * Preserve the channel struct in case the script closes it.
     */
     
    Tcl_Preserve((ClientData) channel);

    /*
     * If we are flushing in the background, be sure to call FlushChannel
     * for writable events.  Note that we have to discard the writable
     * event so we don't call any write handlers before the flush is
     * complete.
     */

    if ((statePtr->flags & BG_FLUSH_SCHEDULED) && (mask & TCL_WRITABLE)) {
      FlushChannel(NULL, chanPtr, 1);
      mask &= ~TCL_WRITABLE;
    }

    /*
     * Add this invocation to the list of recursive invocations of
     * ChannelHandlerEventProc.
     */
    
    nh.nextHandlerPtr = (ChannelHandler *) NULL;
    nh.nestedHandlerPtr = tsdPtr->nestedHandlerPtr;
    tsdPtr->nestedHandlerPtr = &nh;

    for (chPtr = statePtr->chPtr; chPtr != (ChannelHandler *) NULL; ) {

      /*
       * If this channel handler is interested in any of the events that
       * have occurred on the channel, invoke its procedure.
       */
        
      if ((chPtr->mask & mask) != 0) {
	nh.nextHandlerPtr = chPtr->nextPtr;
	(*(chPtr->proc))(chPtr->clientData, mask);
	chPtr = nh.nextHandlerPtr;
      } else {
	chPtr = chPtr->nextPtr;
      }
    }

    /*
     * Update the notifier interest, since it may have changed after
     * invoking event handlers. Skip that if the channel was deleted
     * in the call to the channel handler.
     */

    if (chanPtr->typePtr != NULL) {
        UpdateInterest(chanPtr);
    }

    Tcl_Release((ClientData) channel);

    tsdPtr->nestedHandlerPtr = nh.nestedHandlerPtr;
#else
    /* Walk all channels in a stack ! and notify them in order.
     */

    while (chanPtr != (Channel *) NULL) {
        /*
	 * Preserve the channel struct in case the script closes it.
	 */
     
        Tcl_Preserve((ClientData) channel);

	/*
	 * If we are flushing in the background, be sure to call FlushChannel
	 * for writable events.  Note that we have to discard the writable
	 * event so we don't call any write handlers before the flush is
	 * complete.
	 */

	if ((statePtr->flags & BG_FLUSH_SCHEDULED) && (mask & TCL_WRITABLE)) {
	    FlushChannel(NULL, chanPtr, 1);
	    mask &= ~TCL_WRITABLE;
	}

	/*
	 * Add this invocation to the list of recursive invocations of
	 * ChannelHandlerEventProc.
	 */
    
	nh.nextHandlerPtr = (ChannelHandler *) NULL;
	nh.nestedHandlerPtr = tsdPtr->nestedHandlerPtr;
	tsdPtr->nestedHandlerPtr = &nh;

	for (chPtr = statePtr->chPtr; chPtr != (ChannelHandler *) NULL; ) {

	    /*
	     * If this channel handler is interested in any of the events that
	     * have occurred on the channel, invoke its procedure.
	     */
        
	    if ((chPtr->mask & mask) != 0) {
		nh.nextHandlerPtr = chPtr->nextPtr;
		(*(chPtr->proc))(chPtr->clientData, mask);
		chPtr = nh.nextHandlerPtr;
	    } else {
		chPtr = chPtr->nextPtr;
	    }
	}

	/*
	 * Update the notifier interest, since it may have changed after
	 * invoking event handlers. Skip that if the channel was deleted
	 * in the call to the channel handler.
	 */

	if (chanPtr->typePtr != NULL) {
	    UpdateInterest(chanPtr);

	    /* Walk down the stack.
	     */
	    chanPtr = chanPtr->downChanPtr;
	} else {
	    /* Stop walking the chain, the whole stack was destroyed!
	     */
	    chanPtr = (Channel *) NULL;
	}

	Tcl_Release((ClientData) channel);

	tsdPtr->nestedHandlerPtr = nh.nestedHandlerPtr;

	channel = (Tcl_Channel) chanPtr;
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateInterest --
 *
 *	Arrange for the notifier to call us back at appropriate times
 *	based on the current state of the channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May schedule a timer or driver handler.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateInterest(chanPtr)
    Channel *chanPtr;		/* Channel to update. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    int mask = statePtr->interestMask;

    /*
     * If there are flushed buffers waiting to be written, then
     * we need to watch for the channel to become writable.
     */

    if (statePtr->flags & BG_FLUSH_SCHEDULED) {
	mask |= TCL_WRITABLE;
    }

    /*
     * If there is data in the input queue, and we aren't waiting for more
     * data, then we need to schedule a timer so we don't block in the
     * notifier.  Also, cancel the read interest so we don't get duplicate
     * events.
     */

    if (mask & TCL_READABLE) {
	if (!(statePtr->flags & CHANNEL_NEED_MORE_DATA)
		&& (statePtr->inQueueHead != (ChannelBuffer *) NULL)
		&& (statePtr->inQueueHead->nextRemoved <
			statePtr->inQueueHead->nextAdded)) {
	    mask &= ~TCL_READABLE;
	    if (!statePtr->timer) {
		statePtr->timer = Tcl_CreateTimerHandler(0, ChannelTimerProc,
			(ClientData) chanPtr);
	    }
	}
    }
    (chanPtr->typePtr->watchProc)(chanPtr->instanceData, mask);
}

/*
 *----------------------------------------------------------------------
 *
 * ChannelTimerProc --
 *
 *	Timer handler scheduled by UpdateInterest to monitor the
 *	channel buffers until they are empty.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May invoke channel handlers.
 *
 *----------------------------------------------------------------------
 */

static void
ChannelTimerProc(clientData)
    ClientData clientData;
{
    Channel *chanPtr = (Channel *) clientData;
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */

    if (!(statePtr->flags & CHANNEL_NEED_MORE_DATA)
	    && (statePtr->interestMask & TCL_READABLE)
	    && (statePtr->inQueueHead != (ChannelBuffer *) NULL)
	    && (statePtr->inQueueHead->nextRemoved <
		    statePtr->inQueueHead->nextAdded)) {
	/*
	 * Restart the timer in case a channel handler reenters the
	 * event loop before UpdateInterest gets called by Tcl_NotifyChannel.
	 */

	statePtr->timer = Tcl_CreateTimerHandler(0, ChannelTimerProc,
		(ClientData) chanPtr);
	Tcl_NotifyChannel((Tcl_Channel)chanPtr, TCL_READABLE);
 
    } else {
	statePtr->timer = NULL;
	UpdateInterest(chanPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateChannelHandler --
 *
 *	Arrange for a given procedure to be invoked whenever the
 *	channel indicated by the chanPtr arg becomes readable or
 *	writable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on, whenever the I/O channel given by chanPtr becomes
 *	ready in the way indicated by mask, proc will be invoked.
 *	See the manual entry for details on the calling sequence
 *	to proc.  If there is already an event handler for chan, proc
 *	and clientData, then the mask will be updated.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CreateChannelHandler(chan, mask, proc, clientData)
    Tcl_Channel chan;		/* The channel to create the handler for. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions under which
				 * proc should be called. Use 0 to
                                 * disable a registered handler. */
    Tcl_ChannelProc *proc;	/* Procedure to call for each
				 * selected event. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    ChannelHandler *chPtr;
    Channel *chanPtr = (Channel *) chan;
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */

    /*
     * Check whether this channel handler is not already registered. If
     * it is not, create a new record, else reuse existing record (smash
     * current values).
     */

    for (chPtr = statePtr->chPtr;
	 chPtr != (ChannelHandler *) NULL;
	 chPtr = chPtr->nextPtr) {
        if ((chPtr->chanPtr == chanPtr) && (chPtr->proc == proc) &&
                (chPtr->clientData == clientData)) {
            break;
        }
    }
    if (chPtr == (ChannelHandler *) NULL) {
        chPtr = (ChannelHandler *) ckalloc((unsigned) sizeof(ChannelHandler));
        chPtr->mask = 0;
        chPtr->proc = proc;
        chPtr->clientData = clientData;
        chPtr->chanPtr = chanPtr;
        chPtr->nextPtr = statePtr->chPtr;
        statePtr->chPtr = chPtr;
    }

    /*
     * The remainder of the initialization below is done regardless of
     * whether or not this is a new record or a modification of an old
     * one.
     */

    chPtr->mask = mask;

    /*
     * Recompute the interest mask for the channel - this call may actually
     * be disabling an existing handler.
     */
    
    statePtr->interestMask = 0;
    for (chPtr = statePtr->chPtr;
	 chPtr != (ChannelHandler *) NULL;
	 chPtr = chPtr->nextPtr) {
	statePtr->interestMask |= chPtr->mask;
    }

    UpdateInterest(statePtr->topChanPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteChannelHandler --
 *
 *	Cancel a previously arranged callback arrangement for an IO
 *	channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a callback was previously registered for this chan, proc and
 *	 clientData , it is removed and the callback will no longer be called
 *	when the channel becomes ready for IO.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteChannelHandler(chan, proc, clientData)
    Tcl_Channel chan;		/* The channel for which to remove the
                                 * callback. */
    Tcl_ChannelProc *proc;	/* The procedure in the callback to delete. */
    ClientData clientData;	/* The client data in the callback
                                 * to delete. */
    
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    ChannelHandler *chPtr, *prevChPtr;
    Channel *chanPtr = (Channel *) chan;
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    NextChannelHandler *nhPtr;

    /*
     * Find the entry and the previous one in the list.
     */

    for (prevChPtr = (ChannelHandler *) NULL, chPtr = statePtr->chPtr;
	 chPtr != (ChannelHandler *) NULL;
	 chPtr = chPtr->nextPtr) {
        if ((chPtr->chanPtr == chanPtr) && (chPtr->clientData == clientData)
                && (chPtr->proc == proc)) {
            break;
        }
        prevChPtr = chPtr;
    }

    /*
     * If not found, return without doing anything.
     */

    if (chPtr == (ChannelHandler *) NULL) {
        return;
    }

    /*
     * If ChannelHandlerEventProc is about to process this handler, tell it to
     * process the next one instead - we are going to delete *this* one.
     */

    for (nhPtr = tsdPtr->nestedHandlerPtr;
	 nhPtr != (NextChannelHandler *) NULL;
	 nhPtr = nhPtr->nestedHandlerPtr) {
        if (nhPtr->nextHandlerPtr == chPtr) {
            nhPtr->nextHandlerPtr = chPtr->nextPtr;
        }
    }

    /*
     * Splice it out of the list of channel handlers.
     */
    
    if (prevChPtr == (ChannelHandler *) NULL) {
        statePtr->chPtr = chPtr->nextPtr;
    } else {
        prevChPtr->nextPtr = chPtr->nextPtr;
    }
    ckfree((char *) chPtr);

    /*
     * Recompute the interest list for the channel, so that infinite loops
     * will not result if Tcl_DeleteChannelHandler is called inside an
     * event.
     */

    statePtr->interestMask = 0;
    for (chPtr = statePtr->chPtr;
	 chPtr != (ChannelHandler *) NULL;
	 chPtr = chPtr->nextPtr) {
        statePtr->interestMask |= chPtr->mask;
    }

    UpdateInterest(statePtr->topChanPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteScriptRecord --
 *
 *	Delete a script record for this combination of channel, interp
 *	and mask.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes a script record and cancels a channel event handler.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteScriptRecord(interp, chanPtr, mask)
    Tcl_Interp *interp;		/* Interpreter in which script was to be
                                 * executed. */
    Channel *chanPtr;		/* The channel for which to delete the
                                 * script record (if any). */
    int mask;			/* Events in mask must exactly match mask
                                 * of script to delete. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    EventScriptRecord *esPtr, *prevEsPtr;

    for (esPtr = statePtr->scriptRecordPtr,
             prevEsPtr = (EventScriptRecord *) NULL;
	 esPtr != (EventScriptRecord *) NULL;
	 prevEsPtr = esPtr, esPtr = esPtr->nextPtr) {
        if ((esPtr->interp == interp) && (esPtr->mask == mask)) {
            if (esPtr == statePtr->scriptRecordPtr) {
                statePtr->scriptRecordPtr = esPtr->nextPtr;
            } else {
                prevEsPtr->nextPtr = esPtr->nextPtr;
            }

            Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                    ChannelEventScriptInvoker, (ClientData) esPtr);
            
	    Tcl_DecrRefCount(esPtr->scriptPtr);
            ckfree((char *) esPtr);

            break;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CreateScriptRecord --
 *
 *	Creates a record to store a script to be executed when a specific
 *	event fires on a specific channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Causes the script to be stored for later execution.
 *
 *----------------------------------------------------------------------
 */

static void
CreateScriptRecord(interp, chanPtr, mask, scriptPtr)
    Tcl_Interp *interp;			/* Interpreter in which to execute
                                         * the stored script. */
    Channel *chanPtr;			/* Channel for which script is to
                                         * be stored. */
    int mask;				/* Set of events for which script
                                         * will be invoked. */
    Tcl_Obj *scriptPtr;			/* Pointer to script object. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    EventScriptRecord *esPtr;

    for (esPtr = statePtr->scriptRecordPtr;
	 esPtr != (EventScriptRecord *) NULL;
	 esPtr = esPtr->nextPtr) {
        if ((esPtr->interp == interp) && (esPtr->mask == mask)) {
	    Tcl_DecrRefCount(esPtr->scriptPtr);
	    esPtr->scriptPtr = (Tcl_Obj *) NULL;
            break;
        }
    }
    if (esPtr == (EventScriptRecord *) NULL) {
        esPtr = (EventScriptRecord *) ckalloc((unsigned)
                sizeof(EventScriptRecord));
        Tcl_CreateChannelHandler((Tcl_Channel) chanPtr, mask,
                ChannelEventScriptInvoker, (ClientData) esPtr);
        esPtr->nextPtr = statePtr->scriptRecordPtr;
        statePtr->scriptRecordPtr = esPtr;
    }
    esPtr->chanPtr = chanPtr;
    esPtr->interp = interp;
    esPtr->mask = mask;
    Tcl_IncrRefCount(scriptPtr);
    esPtr->scriptPtr = scriptPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ChannelEventScriptInvoker --
 *
 *	Invokes a script scheduled by "fileevent" for when the channel
 *	becomes ready for IO. This function is invoked by the channel
 *	handler which was created by the Tcl "fileevent" command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Whatever the script does.
 *
 *----------------------------------------------------------------------
 */

static void
ChannelEventScriptInvoker(clientData, mask)
    ClientData clientData;	/* The script+interp record. */
    int mask;			/* Not used. */
{
    Tcl_Interp *interp;		/* Interpreter in which to eval the script. */
    Channel *chanPtr;		/* The channel for which this handler is
                                 * registered. */
    EventScriptRecord *esPtr;	/* The event script + interpreter to eval it
                                 * in. */
    int result;			/* Result of call to eval script. */

    esPtr	= (EventScriptRecord *) clientData;
    chanPtr	= esPtr->chanPtr;
    mask	= esPtr->mask;
    interp	= esPtr->interp;

    /*
     * We must preserve the interpreter so we can report errors on it
     * later.  Note that we do not need to preserve the channel because
     * that is done by Tcl_NotifyChannel before calling channel handlers.
     */
    
    Tcl_Preserve((ClientData) interp);
    result = Tcl_EvalObjEx(interp, esPtr->scriptPtr, TCL_EVAL_GLOBAL);

    /*
     * On error, cause a background error and remove the channel handler
     * and the script record.
     *
     * NOTE: Must delete channel handler before causing the background error
     * because the background error may want to reinstall the handler.
     */
    
    if (result != TCL_OK) {
	if (chanPtr->typePtr != NULL) {
	    DeleteScriptRecord(interp, chanPtr, mask);
	}
        Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FileEventObjCmd --
 *
 *	This procedure implements the "fileevent" Tcl command. See the
 *	user documentation for details on what it does. This command is
 *	based on the Tk command "fileevent" which in turn is based on work
 *	contributed by Mark Diekhans.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May create a channel handler for the specified channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_FileEventObjCmd(clientData, interp, objc, objv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Interpreter in which the channel
                                         * for which to create the handler
                                         * is found. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Channel *chanPtr;			/* The channel to create
                                         * the handler for. */
    ChannelState *statePtr;		/* state info for channel */
    Tcl_Channel chan;			/* The opaque type for the channel. */
    char *chanName;
    int modeIndex;			/* Index of mode argument. */
    int mask;
    static char *modeOptions[] = {"readable", "writable", NULL};
    static int maskArray[] = {TCL_READABLE, TCL_WRITABLE};

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 1, objv, "channelId event ?script?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[2], modeOptions, "event name", 0,
	    &modeIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    mask = maskArray[modeIndex];

    chanName = Tcl_GetString(objv[1]);
    chan = Tcl_GetChannel(interp, chanName, NULL);
    if (chan == (Tcl_Channel) NULL) {
	return TCL_ERROR;
    }
    chanPtr  = (Channel *) chan;
    statePtr = chanPtr->state;
    if ((statePtr->flags & mask) == 0) {
        Tcl_AppendResult(interp, "channel is not ",
                (mask == TCL_READABLE) ? "readable" : "writable",
                (char *) NULL);
        return TCL_ERROR;
    }
    
    /*
     * If we are supposed to return the script, do so.
     */

    if (objc == 3) {
	EventScriptRecord *esPtr;
	for (esPtr = statePtr->scriptRecordPtr;
             esPtr != (EventScriptRecord *) NULL;
             esPtr = esPtr->nextPtr) {
	    if ((esPtr->interp == interp) && (esPtr->mask == mask)) {
		Tcl_SetObjResult(interp, esPtr->scriptPtr);
		break;
	    }
	}
        return TCL_OK;
    }

    /*
     * If we are supposed to delete a stored script, do so.
     */

    if (*(Tcl_GetString(objv[3])) == '\0') {
        DeleteScriptRecord(interp, chanPtr, mask);
        return TCL_OK;
    }

    /*
     * Make the script record that will link between the event and the
     * script to invoke. This also creates a channel event handler which
     * will evaluate the script in the supplied interpreter.
     */

    CreateScriptRecord(interp, chanPtr, mask, objv[3]);
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclTestChannelCmd --
 *
 *	Implements the Tcl "testchannel" debugging command and its
 *	subcommands. This is part of the testing environment but must be
 *	in this file instead of tclTest.c because it needs access to the
 *	fields of struct Channel.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
TclTestChannelCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter for result. */
    int argc;			/* Count of additional args. */
    char **argv;		/* Additional arg strings. */
{
    char *cmdName;		/* Sub command. */
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_HashSearch hSearch;	/* Search variable. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    Channel *chanPtr;		/* The actual channel. */
    ChannelState *statePtr;	/* state info for channel */
    Tcl_Channel chan;		/* The opaque type. */
    size_t len;			/* Length of subcommand string. */
    int IOQueued;		/* How much IO is queued inside channel? */
    ChannelBuffer *bufPtr;	/* For iterating over queued IO. */
    char buf[TCL_INTEGER_SPACE];/* For sprintf. */
    int mode;			/* rw mode of the channel */
    
    if (argc < 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " subcommand ?additional args..?\"", (char *) NULL);
        return TCL_ERROR;
    }
    cmdName = argv[1];
    len = strlen(cmdName);

    chanPtr = (Channel *) NULL;

    if (argc > 2) {
        chan = Tcl_GetChannel(interp, argv[2], &mode);
        if (chan == (Tcl_Channel) NULL) {
            return TCL_ERROR;
        }
        chanPtr		= (Channel *) chan;
	statePtr	= chanPtr->state;
        chanPtr		= statePtr->topChanPtr;
	chan		= (Tcl_Channel) chanPtr;
    }

    if ((cmdName[0] == 'i') && (strncmp(cmdName, "info", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " info channelName\"", (char *) NULL);
            return TCL_ERROR;
        }
        Tcl_AppendElement(interp, argv[2]);
        Tcl_AppendElement(interp, chanPtr->typePtr->typeName);
        if (statePtr->flags & TCL_READABLE) {
            Tcl_AppendElement(interp, "read");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (statePtr->flags & TCL_WRITABLE) {
            Tcl_AppendElement(interp, "write");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (statePtr->flags & CHANNEL_NONBLOCKING) {
            Tcl_AppendElement(interp, "nonblocking");
        } else {
            Tcl_AppendElement(interp, "blocking");
        }
        if (statePtr->flags & CHANNEL_LINEBUFFERED) {
            Tcl_AppendElement(interp, "line");
        } else if (statePtr->flags & CHANNEL_UNBUFFERED) {
            Tcl_AppendElement(interp, "none");
        } else {
            Tcl_AppendElement(interp, "full");
        }
        if (statePtr->flags & BG_FLUSH_SCHEDULED) {
            Tcl_AppendElement(interp, "async_flush");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (statePtr->flags & CHANNEL_EOF) {
            Tcl_AppendElement(interp, "eof");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (statePtr->flags & CHANNEL_BLOCKED) {
            Tcl_AppendElement(interp, "blocked");
        } else {
            Tcl_AppendElement(interp, "unblocked");
        }
        if (statePtr->inputTranslation == TCL_TRANSLATE_AUTO) {
            Tcl_AppendElement(interp, "auto");
            if (statePtr->flags & INPUT_SAW_CR) {
                Tcl_AppendElement(interp, "saw_cr");
            } else {
                Tcl_AppendElement(interp, "");
            }
        } else if (statePtr->inputTranslation == TCL_TRANSLATE_LF) {
            Tcl_AppendElement(interp, "lf");
            Tcl_AppendElement(interp, "");
        } else if (statePtr->inputTranslation == TCL_TRANSLATE_CR) {
            Tcl_AppendElement(interp, "cr");
            Tcl_AppendElement(interp, "");
        } else if (statePtr->inputTranslation == TCL_TRANSLATE_CRLF) {
            Tcl_AppendElement(interp, "crlf");
            if (statePtr->flags & INPUT_SAW_CR) {
                Tcl_AppendElement(interp, "queued_cr");
            } else {
                Tcl_AppendElement(interp, "");
            }
        }
        if (statePtr->outputTranslation == TCL_TRANSLATE_AUTO) {
            Tcl_AppendElement(interp, "auto");
        } else if (statePtr->outputTranslation == TCL_TRANSLATE_LF) {
            Tcl_AppendElement(interp, "lf");
        } else if (statePtr->outputTranslation == TCL_TRANSLATE_CR) {
            Tcl_AppendElement(interp, "cr");
        } else if (statePtr->outputTranslation == TCL_TRANSLATE_CRLF) {
            Tcl_AppendElement(interp, "crlf");
        }
        for (IOQueued = 0, bufPtr = statePtr->inQueueHead;
	     bufPtr != (ChannelBuffer *) NULL;
	     bufPtr = bufPtr->nextPtr) {
            IOQueued += bufPtr->nextAdded - bufPtr->nextRemoved;
        }
        TclFormatInt(buf, IOQueued);
        Tcl_AppendElement(interp, buf);
        
        IOQueued = 0;
        if (statePtr->curOutPtr != (ChannelBuffer *) NULL) {
            IOQueued = statePtr->curOutPtr->nextAdded -
                statePtr->curOutPtr->nextRemoved;
        }
        for (bufPtr = statePtr->outQueueHead;
	     bufPtr != (ChannelBuffer *) NULL;
	     bufPtr = bufPtr->nextPtr) {
            IOQueued += (bufPtr->nextAdded - bufPtr->nextRemoved);
        }
        TclFormatInt(buf, IOQueued);
        Tcl_AppendElement(interp, buf);
        
        TclFormatInt(buf, Tcl_Tell((Tcl_Channel) chanPtr));
        Tcl_AppendElement(interp, buf);

        TclFormatInt(buf, statePtr->refCount);
        Tcl_AppendElement(interp, buf);

        return TCL_OK;
    }

    if ((cmdName[0] == 'i') &&
            (strncmp(cmdName, "inputbuffered", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        
        for (IOQueued = 0, bufPtr = statePtr->inQueueHead;
	     bufPtr != (ChannelBuffer *) NULL;
	     bufPtr = bufPtr->nextPtr) {
            IOQueued += bufPtr->nextAdded - bufPtr->nextRemoved;
        }
        TclFormatInt(buf, IOQueued);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_OK;
    }

    if ((cmdName[0] == 'm') && (strncmp(cmdName, "mode", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        
        if (statePtr->flags & TCL_READABLE) {
            Tcl_AppendElement(interp, "read");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (statePtr->flags & TCL_WRITABLE) {
            Tcl_AppendElement(interp, "write");
        } else {
            Tcl_AppendElement(interp, "");
        }
        return TCL_OK;
    }
    
    if ((cmdName[0] == 'n') && (strncmp(cmdName, "name", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        Tcl_AppendResult(interp, statePtr->channelName, (char *) NULL);
        return TCL_OK;
    }

    if ((cmdName[0] == 'o') && (strncmp(cmdName, "open", len) == 0)) {
        hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
        if (hTblPtr == (Tcl_HashTable *) NULL) {
            return TCL_OK;
        }
        for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
	     hPtr != (Tcl_HashEntry *) NULL;
	     hPtr = Tcl_NextHashEntry(&hSearch)) {
            Tcl_AppendElement(interp, Tcl_GetHashKey(hTblPtr, hPtr));
        }
        return TCL_OK;
    }

    if ((cmdName[0] == 'o') &&
            (strncmp(cmdName, "outputbuffered", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }

        IOQueued = 0;
        if (statePtr->curOutPtr != (ChannelBuffer *) NULL) {
            IOQueued = statePtr->curOutPtr->nextAdded -
                statePtr->curOutPtr->nextRemoved;
        }
        for (bufPtr = statePtr->outQueueHead;
	     bufPtr != (ChannelBuffer *) NULL;
	     bufPtr = bufPtr->nextPtr) {
            IOQueued += (bufPtr->nextAdded - bufPtr->nextRemoved);
        }
        TclFormatInt(buf, IOQueued);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_OK;
    }

    if ((cmdName[0] == 'q') &&
            (strncmp(cmdName, "queuedcr", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }

        Tcl_AppendResult(interp,
                (statePtr->flags & INPUT_SAW_CR) ? "1" : "0",
                (char *) NULL);
        return TCL_OK;
    }

    if ((cmdName[0] == 'r') && (strncmp(cmdName, "readable", len) == 0)) {
        hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
        if (hTblPtr == (Tcl_HashTable *) NULL) {
            return TCL_OK;
        }
        for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
	     hPtr != (Tcl_HashEntry *) NULL;
	     hPtr = Tcl_NextHashEntry(&hSearch)) {
            chanPtr  = (Channel *) Tcl_GetHashValue(hPtr);
	    statePtr = chanPtr->state;
            if (statePtr->flags & TCL_READABLE) {
                Tcl_AppendElement(interp, Tcl_GetHashKey(hTblPtr, hPtr));
            }
        }
        return TCL_OK;
    }

    if ((cmdName[0] == 'r') && (strncmp(cmdName, "refcount", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        
        TclFormatInt(buf, statePtr->refCount);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_OK;
    }

    if ((cmdName[0] == 't') && (strncmp(cmdName, "type", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        Tcl_AppendResult(interp, chanPtr->typePtr->typeName,
		(char *) NULL);
        return TCL_OK;
    }

    if ((cmdName[0] == 'w') && (strncmp(cmdName, "writable", len) == 0)) {
        hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
        if (hTblPtr == (Tcl_HashTable *) NULL) {
            return TCL_OK;
        }
        for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
	     hPtr != (Tcl_HashEntry *) NULL;
	     hPtr = Tcl_NextHashEntry(&hSearch)) {
            chanPtr = (Channel *) Tcl_GetHashValue(hPtr);
	    statePtr = chanPtr->state;
            if (statePtr->flags & TCL_WRITABLE) {
                Tcl_AppendElement(interp, Tcl_GetHashKey(hTblPtr, hPtr));
            }
        }
        return TCL_OK;
    }

    if ((cmdName[0] == 't') && (strncmp(cmdName, "transform", len) == 0)) {
	/*
	 * Syntax: transform channel -command command
	 */

	TransformChannelData	*ctrl;
	int			res;
	Tcl_Obj			*command;
	Tcl_DString		ds;

        if (argc != 5) {
            Tcl_AppendResult(interp, "wrong # args: should be ",
		    "channel transform channelId -command cmd", (char *) NULL);
            return TCL_ERROR;
        }
	if (strcmp(argv[3], "-command") != 0) {
	    Tcl_AppendResult(interp, "bad argument \"", argv[3],
		    "\": should be \"-command\"", (char *) NULL);
	    return TCL_ERROR;
	}

	command = Tcl_NewStringObj(argv[4], -1);

	/*
	 * Now initialize the transformation state and stack it upon the
	 * specified channel. One of the necessary things to do is to
	 * retrieve the blocking regime of the underlying channel and to
	 * use the same for us too.
	 */

	ctrl = (TransformChannelData*) ckalloc(sizeof(TransformChannelData));

	ctrl->readIsFlushed = 0;

	Tcl_DStringInit (&ds);
	Tcl_GetChannelOption(interp, chan, "-blocking", &ds);

	ctrl->flags         = 0;

	if (ds.string[0] == '0') {
	    ctrl->flags |= CHANNEL_ASYNC;
	}

	Tcl_DStringFree (&ds);

	ctrl->self	= chan;
	ctrl->watchMask	= 0;
	ctrl->mode	= mode;
	ctrl->timer	= (Tcl_TimerToken) NULL;
	ctrl->maxRead	= 4096; /* Initial value not relevant */
	ctrl->interp	= interp;
	ctrl->command	= command;

	Tcl_IncrRefCount(ctrl->command);

	ResultInit(&ctrl->result);

	ctrl->self = Tcl_StackChannel(interp, &transformChannelType,
		(ClientData) ctrl, mode, chan);
	if (ctrl->self == (Tcl_Channel) NULL) {
	    Tcl_AppendResult(interp, "\nfailed to stack channel \"",
		    Tcl_GetChannelName(chan), "\"", (char *) NULL);
	    goto cleanup;
	}

	/*
	 * At last initialize the transformation at the script level.
	 */

	if (ctrl->mode & TCL_WRITABLE) {
	    res = ExecuteCallback (ctrl, NO_INTERP, A_CREATE_WRITE,
		    NULL, 0, TRANSMIT_DONT, P_NO_PRESERVE);

	    if (res != TCL_OK) {
		Tcl_UnstackChannel(interp, chan);
		goto cleanup;
	    }
	}

	if (ctrl->mode & TCL_READABLE) {
	    res = ExecuteCallback (ctrl, NO_INTERP, A_CREATE_READ,
		    NULL, 0, TRANSMIT_DONT, P_NO_PRESERVE);

	    if (res != TCL_OK) {
		ExecuteCallback (ctrl, NO_INTERP, A_DELETE_WRITE,
			NULL, 0, TRANSMIT_DONT, P_NO_PRESERVE);

		Tcl_UnstackChannel(interp, chan);
		goto cleanup;
	    }
	}

	return TCL_OK;

	cleanup:
	Tcl_DecrRefCount(ctrl->command);
	ResultClear(&ctrl->result);
	ckfree((VOID *) ctrl);

	return TCL_ERROR;
    }

    if ((cmdName[0] == 'u') && (strncmp(cmdName, "unstack", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
	return Tcl_UnstackChannel(interp, chan);
    }

    Tcl_AppendResult(interp, "bad option \"", cmdName, "\": should be ",
            "info, open, readable, writable, transform, unstack",
            (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclTestChannelEventCmd --
 *
 *	This procedure implements the "testchannelevent" command. It is
 *	used to test the Tcl channel event mechanism. It is present in
 *	this file instead of tclTest.c because it needs access to the
 *	internal structure of the channel.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates, deletes and returns channel event handlers.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
TclTestChannelEventCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Obj *resultListPtr;
    Channel *chanPtr;
    ChannelState *statePtr;	/* state info for channel */
    EventScriptRecord *esPtr, *prevEsPtr, *nextEsPtr;
    char *cmd;
    int index, i, mask, len;

    if ((argc < 3) || (argc > 5)) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " channelName cmd ?arg1? ?arg2?\"", (char *) NULL);
        return TCL_ERROR;
    }
    chanPtr = (Channel *) Tcl_GetChannel(interp, argv[1], NULL);
    if (chanPtr == (Channel *) NULL) {
        return TCL_ERROR;
    }
    statePtr = chanPtr->state;

    cmd = argv[2];
    len = strlen(cmd);
    if ((cmd[0] == 'a') && (strncmp(cmd, "add", (unsigned) len) == 0)) {
        if (argc != 5) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " channelName add eventSpec script\"", (char *) NULL);
            return TCL_ERROR;
        }
        if (strcmp(argv[3], "readable") == 0) {
            mask = TCL_READABLE;
        } else if (strcmp(argv[3], "writable") == 0) {
            mask = TCL_WRITABLE;
        } else if (strcmp(argv[3], "none") == 0) {
            mask = 0;
	} else {
            Tcl_AppendResult(interp, "bad event name \"", argv[3],
                    "\": must be readable, writable, or none", (char *) NULL);
            return TCL_ERROR;
        }

        esPtr = (EventScriptRecord *) ckalloc((unsigned)
                sizeof(EventScriptRecord));
        esPtr->nextPtr = statePtr->scriptRecordPtr;
        statePtr->scriptRecordPtr = esPtr;
        
        esPtr->chanPtr = chanPtr;
        esPtr->interp = interp;
        esPtr->mask = mask;
	esPtr->scriptPtr = Tcl_NewStringObj(argv[4], -1);
	Tcl_IncrRefCount(esPtr->scriptPtr);

        Tcl_CreateChannelHandler((Tcl_Channel) chanPtr, mask,
                ChannelEventScriptInvoker, (ClientData) esPtr);
        
        return TCL_OK;
    }

    if ((cmd[0] == 'd') && (strncmp(cmd, "delete", (unsigned) len) == 0)) {
        if (argc != 4) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " channelName delete index\"", (char *) NULL);
            return TCL_ERROR;
        }
        if (Tcl_GetInt(interp, argv[3], &index) == TCL_ERROR) {
            return TCL_ERROR;
        }
        if (index < 0) {
            Tcl_AppendResult(interp, "bad event index: ", argv[3],
                    ": must be nonnegative", (char *) NULL);
            return TCL_ERROR;
        }
        for (i = 0, esPtr = statePtr->scriptRecordPtr;
	     (i < index) && (esPtr != (EventScriptRecord *) NULL);
	     i++, esPtr = esPtr->nextPtr) {
	    /* Empty loop body. */
        }
        if (esPtr == (EventScriptRecord *) NULL) {
            Tcl_AppendResult(interp, "bad event index ", argv[3],
                    ": out of range", (char *) NULL);
            return TCL_ERROR;
        }
        if (esPtr == statePtr->scriptRecordPtr) {
            statePtr->scriptRecordPtr = esPtr->nextPtr;
        } else {
            for (prevEsPtr = statePtr->scriptRecordPtr;
		 (prevEsPtr != (EventScriptRecord *) NULL) &&
		     (prevEsPtr->nextPtr != esPtr);
		 prevEsPtr = prevEsPtr->nextPtr) {
                /* Empty loop body. */
            }
            if (prevEsPtr == (EventScriptRecord *) NULL) {
                panic("TclTestChannelEventCmd: damaged event script list");
            }
            prevEsPtr->nextPtr = esPtr->nextPtr;
        }
        Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                ChannelEventScriptInvoker, (ClientData) esPtr);
	Tcl_DecrRefCount(esPtr->scriptPtr);
        ckfree((char *) esPtr);

        return TCL_OK;
    }

    if ((cmd[0] == 'l') && (strncmp(cmd, "list", (unsigned) len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " channelName list\"", (char *) NULL);
            return TCL_ERROR;
        }
	resultListPtr = Tcl_GetObjResult(interp);
        for (esPtr = statePtr->scriptRecordPtr;
	     esPtr != (EventScriptRecord *) NULL;
	     esPtr = esPtr->nextPtr) {
	    if (esPtr->mask) {
 	        Tcl_ListObjAppendElement(interp, resultListPtr, Tcl_NewStringObj(
		    (esPtr->mask == TCL_READABLE) ? "readable" : "writable", -1));
 	    } else {
 	        Tcl_ListObjAppendElement(interp, resultListPtr, 
			Tcl_NewStringObj("none", -1));
	    }
  	    Tcl_ListObjAppendElement(interp, resultListPtr, esPtr->scriptPtr);
        }
	Tcl_SetObjResult(interp, resultListPtr);
        return TCL_OK;
    }

    if ((cmd[0] == 'r') && (strncmp(cmd, "removeall", (unsigned) len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " channelName removeall\"", (char *) NULL);
            return TCL_ERROR;
        }
        for (esPtr = statePtr->scriptRecordPtr;
	     esPtr != (EventScriptRecord *) NULL;
	     esPtr = nextEsPtr) {
            nextEsPtr = esPtr->nextPtr;
            Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                    ChannelEventScriptInvoker, (ClientData) esPtr);
	    Tcl_DecrRefCount(esPtr->scriptPtr);
            ckfree((char *) esPtr);
        }
        statePtr->scriptRecordPtr = (EventScriptRecord *) NULL;
        return TCL_OK;
    }

    if  ((cmd[0] == 's') && (strncmp(cmd, "set", (unsigned) len) == 0)) {
        if (argc != 5) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " channelName delete index event\"", (char *) NULL);
            return TCL_ERROR;
        }
        if (Tcl_GetInt(interp, argv[3], &index) == TCL_ERROR) {
            return TCL_ERROR;
        }
        if (index < 0) {
            Tcl_AppendResult(interp, "bad event index: ", argv[3],
                    ": must be nonnegative", (char *) NULL);
            return TCL_ERROR;
        }
        for (i = 0, esPtr = statePtr->scriptRecordPtr;
	     (i < index) && (esPtr != (EventScriptRecord *) NULL);
	     i++, esPtr = esPtr->nextPtr) {
	    /* Empty loop body. */
        }
        if (esPtr == (EventScriptRecord *) NULL) {
            Tcl_AppendResult(interp, "bad event index ", argv[3],
                    ": out of range", (char *) NULL);
            return TCL_ERROR;
        }

        if (strcmp(argv[4], "readable") == 0) {
            mask = TCL_READABLE;
        } else if (strcmp(argv[4], "writable") == 0) {
            mask = TCL_WRITABLE;
        } else if (strcmp(argv[4], "none") == 0) {
            mask = 0;
	} else {
            Tcl_AppendResult(interp, "bad event name \"", argv[4],
                    "\": must be readable, writable, or none", (char *) NULL);
            return TCL_ERROR;
        }
	esPtr->mask = mask;
        Tcl_CreateChannelHandler((Tcl_Channel) chanPtr, mask,
                ChannelEventScriptInvoker, (ClientData) esPtr);
	return TCL_OK;
    }    
    Tcl_AppendResult(interp, "bad command ", cmd, ", must be one of ",
            "add, delete, list, set, or removeall", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCopyChannel --
 *
 *	This routine copies data from one channel to another, either
 *	synchronously or asynchronously.  If a command script is
 *	supplied, the operation runs in the background.  The script
 *	is invoked when the copy completes.  Otherwise the function
 *	waits until the copy is completed before returning.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May schedule a background copy operation that causes both
 *	channels to be marked busy.
 *
 *----------------------------------------------------------------------
 */

int
TclCopyChannel(interp, inChan, outChan, toRead, cmdPtr)
    Tcl_Interp *interp;		/* Current interpreter. */
    Tcl_Channel inChan;		/* Channel to read from. */
    Tcl_Channel outChan;	/* Channel to write to. */
    int toRead;			/* Amount of data to copy, or -1 for all. */
    Tcl_Obj *cmdPtr;		/* Pointer to script to execute or NULL. */
{
    Channel *inPtr = (Channel *) inChan;
    Channel *outPtr = (Channel *) outChan;
    ChannelState *inStatePtr, *outStatePtr;
    int readFlags, writeFlags;
    CopyState *csPtr;
    int nonBlocking = (cmdPtr) ? CHANNEL_NONBLOCKING : 0;

    inStatePtr	= inPtr->state;
    outStatePtr	= outPtr->state;

    if (inStatePtr->csPtr) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "channel \"",
		Tcl_GetChannelName(inChan), "\" is busy", NULL);
	return TCL_ERROR;
    }
    if (outStatePtr->csPtr) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "channel \"",
		Tcl_GetChannelName(outChan), "\" is busy", NULL);
	return TCL_ERROR;
    }

    readFlags	= inStatePtr->flags;
    writeFlags	= outStatePtr->flags;

    /*
     * Set up the blocking mode appropriately.  Background copies need
     * non-blocking channels.  Foreground copies need blocking channels.
     * If there is an error, restore the old blocking mode.
     */

    if (nonBlocking != (readFlags & CHANNEL_NONBLOCKING)) {
	if (SetBlockMode(interp, inPtr,
		nonBlocking ? TCL_MODE_NONBLOCKING : TCL_MODE_BLOCKING)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
    }	    
    if (inPtr != outPtr) {
	if (nonBlocking != (writeFlags & CHANNEL_NONBLOCKING)) {
	    if (SetBlockMode(NULL, outPtr,
		    nonBlocking ? TCL_MODE_BLOCKING : TCL_MODE_NONBLOCKING)
		    != TCL_OK) {
		if (nonBlocking != (readFlags & CHANNEL_NONBLOCKING)) {
		    SetBlockMode(NULL, inPtr,
			    (readFlags & CHANNEL_NONBLOCKING)
			    ? TCL_MODE_NONBLOCKING : TCL_MODE_BLOCKING);
		    return TCL_ERROR;
		}
	    }
	}
    }

    /*
     * Make sure the output side is unbuffered.
     */

    outStatePtr->flags = (outStatePtr->flags & ~(CHANNEL_LINEBUFFERED))
	| CHANNEL_UNBUFFERED;

    /*
     * Allocate a new CopyState to maintain info about the current copy in
     * progress.  This structure will be deallocated when the copy is
     * completed.
     */

    csPtr = (CopyState*) ckalloc(sizeof(CopyState) + inStatePtr->bufSize);
    csPtr->bufSize    = inStatePtr->bufSize;
    csPtr->readPtr    = inPtr;
    csPtr->writePtr   = outPtr;
    csPtr->readFlags  = readFlags;
    csPtr->writeFlags = writeFlags;
    csPtr->toRead     = toRead;
    csPtr->total      = 0;
    csPtr->interp     = interp;
    if (cmdPtr) {
	Tcl_IncrRefCount(cmdPtr);
    }
    csPtr->cmdPtr = cmdPtr;
    inStatePtr->csPtr = csPtr;
    outStatePtr->csPtr = csPtr;

    /*
     * Start copying data between the channels.
     */

    return CopyData(csPtr, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * CopyData --
 *
 *	This function implements the lowest level of the copying
 *	mechanism for TclCopyChannel.
 *
 * Results:
 *	Returns TCL_OK on success, else TCL_ERROR.
 *
 * Side effects:
 *	Moves data between channels, may create channel handlers.
 *
 *----------------------------------------------------------------------
 */

static int
CopyData(csPtr, mask)
    CopyState *csPtr;		/* State of copy operation. */
    int mask;			/* Current channel event flags. */
{
    Tcl_Interp *interp;
    Tcl_Obj *cmdPtr, *errObj = NULL;
    Tcl_Channel inChan, outChan;
    ChannelState *inStatePtr, *outStatePtr;
    int result = TCL_OK;
    int size;
    int total;

    inChan	= (Tcl_Channel) csPtr->readPtr;
    outChan	= (Tcl_Channel) csPtr->writePtr;
    inStatePtr	= csPtr->readPtr->state;
    outStatePtr	= csPtr->writePtr->state;
    interp	= csPtr->interp;
    cmdPtr	= csPtr->cmdPtr;

    /*
     * Copy the data the slow way, using the translation mechanism.
     *
     * Note: We have make sure that we use the topmost channel in a stack
     * for the copying. The caller uses Tcl_GetChannel to access it, and
     * thus gets the bottom of the stack.
     */

    while (csPtr->toRead != 0) {

	/*
	 * Check for unreported background errors.
	 */

	if (inStatePtr->unreportedError != 0) {
	    Tcl_SetErrno(inStatePtr->unreportedError);
	    inStatePtr->unreportedError = 0;
	    goto readError;
	}
	if (outStatePtr->unreportedError != 0) {
	    Tcl_SetErrno(outStatePtr->unreportedError);
	    outStatePtr->unreportedError = 0;
	    goto writeError;
	}
	
	/*
	 * Read up to bufSize bytes.
	 */

	if ((csPtr->toRead == -1) || (csPtr->toRead > csPtr->bufSize)) {
	    size = csPtr->bufSize;
	} else {
	    size = csPtr->toRead;
	}
	size = DoRead(inStatePtr->topChanPtr, csPtr->buffer, size);

	if (size < 0) {
	    readError:
	    errObj = Tcl_NewObj();
	    Tcl_AppendStringsToObj(errObj, "error reading \"",
		    Tcl_GetChannelName(inChan), "\": ",
		    Tcl_PosixError(interp), (char *) NULL);
	    break;
	} else if (size == 0) {
	    /*
	     * We had an underflow on the read side.  If we are at EOF,
	     * then the copying is done, otherwise set up a channel
	     * handler to detect when the channel becomes readable again.
	     */
	    
	    if (Tcl_Eof(inChan)) {
		break;
	    } else if (!(mask & TCL_READABLE)) {
		if (mask & TCL_WRITABLE) {
		    Tcl_DeleteChannelHandler(outChan, CopyEventProc,
			    (ClientData) csPtr);
		}
		Tcl_CreateChannelHandler(inChan, TCL_READABLE,
			CopyEventProc, (ClientData) csPtr);
	    }
	    return TCL_OK;
	}

	/*
	 * Now write the buffer out.
	 */

	size = DoWrite(outStatePtr->topChanPtr, csPtr->buffer, size);
	if (size < 0) {
	    writeError:
	    errObj = Tcl_NewObj();
	    Tcl_AppendStringsToObj(errObj, "error writing \"",
		    Tcl_GetChannelName(outChan), "\": ",
		    Tcl_PosixError(interp), (char *) NULL);
	    break;
	}

	/*
	 * Check to see if the write is happening in the background.  If so,
	 * stop copying and wait for the channel to become writable again.
	 */

	if (outStatePtr->flags & BG_FLUSH_SCHEDULED) {
	    if (!(mask & TCL_WRITABLE)) {
		if (mask & TCL_READABLE) {
		    Tcl_DeleteChannelHandler(outChan, CopyEventProc,
			    (ClientData) csPtr);
		}
		Tcl_CreateChannelHandler(outChan, TCL_WRITABLE,
			CopyEventProc, (ClientData) csPtr);
	    }
	    return TCL_OK;
	}

	/*
	 * Update the current byte count if we care.
	 */

	if (csPtr->toRead != -1) {
	    csPtr->toRead -= size;
	}
	csPtr->total += size;

	/*
	 * For background copies, we only do one buffer per invocation so
	 * we don't starve the rest of the system.
	 */

	if (cmdPtr) {
	    /*
	     * The first time we enter this code, there won't be a
	     * channel handler established yet, so do it here.
	     */

	    if (mask == 0) {
		Tcl_CreateChannelHandler(outChan, TCL_WRITABLE,
			CopyEventProc, (ClientData) csPtr);
	    }
	    return TCL_OK;
	}
    }

    /*
     * Make the callback or return the number of bytes transferred.
     * The local total is used because StopCopy frees csPtr.
     */

    total = csPtr->total;
    if (cmdPtr) {
	/*
	 * Get a private copy of the command so we can mutate it
	 * by adding arguments.  Note that StopCopy frees our saved
	 * reference to the original command obj.
	 */

	cmdPtr = Tcl_DuplicateObj(cmdPtr);
	Tcl_IncrRefCount(cmdPtr);
	StopCopy(csPtr);
	Tcl_Preserve((ClientData) interp);

	Tcl_ListObjAppendElement(interp, cmdPtr, Tcl_NewIntObj(total));
	if (errObj) {
	    Tcl_ListObjAppendElement(interp, cmdPtr, errObj);
	}
	if (Tcl_EvalObjEx(interp, cmdPtr, TCL_EVAL_GLOBAL) != TCL_OK) {
	    Tcl_BackgroundError(interp);
	    result = TCL_ERROR;
	}
	Tcl_DecrRefCount(cmdPtr);
	Tcl_Release((ClientData) interp);
    } else {
	StopCopy(csPtr);
	if (errObj) {
	    Tcl_SetObjResult(interp, errObj);
	    result = TCL_ERROR;
	} else {
	    Tcl_ResetResult(interp);
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), total);
	}
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DoRead --
 *
 *	Reads a given number of bytes from a channel.
 *
 * Results:
 *	The number of characters read, or -1 on error. Use Tcl_GetErrno()
 *	to retrieve the error code for the error that occurred.
 *
 * Side effects:
 *	May cause input to be buffered.
 *
 *----------------------------------------------------------------------
 */

static int
DoRead(chanPtr, bufPtr, toRead)
    Channel *chanPtr;		/* The channel from which to read. */
    char *bufPtr;		/* Where to store input read. */
    int toRead;			/* Maximum number of bytes to read. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    int copied;			/* How many characters were copied into
                                 * the result string? */
    int copiedNow;		/* How many characters were copied from
                                 * the current input buffer? */
    int result;			/* Of calling GetInput. */

    /*
     * If we have not encountered a sticky EOF, clear the EOF bit. Either
     * way clear the BLOCKED bit. We want to discover these anew during
     * each operation.
     */

    if (!(statePtr->flags & CHANNEL_STICKY_EOF)) {
        statePtr->flags &= ~CHANNEL_EOF;
    }
    statePtr->flags &= ~(CHANNEL_BLOCKED | CHANNEL_NEED_MORE_DATA);
    
    for (copied = 0; copied < toRead; copied += copiedNow) {
        copiedNow = CopyAndTranslateBuffer(statePtr, bufPtr + copied,
                toRead - copied);
        if (copiedNow == 0) {
            if (statePtr->flags & CHANNEL_EOF) {
		goto done;
            }
            if (statePtr->flags & CHANNEL_BLOCKED) {
                if (statePtr->flags & CHANNEL_NONBLOCKING) {
		    goto done;
                }
                statePtr->flags &= (~(CHANNEL_BLOCKED));
            }
            result = GetInput(chanPtr);
            if (result != 0) {
                if (result != EAGAIN) {
                    copied = -1;
                }
		goto done;
            }
        }
    }

    statePtr->flags &= (~(CHANNEL_BLOCKED));

    done:
    /*
     * Update the notifier state so we don't block while there is still
     * data in the buffers.
     */

    UpdateInterest(chanPtr);
    return copied;
}

/*
 *----------------------------------------------------------------------
 *
 * CopyAndTranslateBuffer --
 *
 *	Copy at most one buffer of input to the result space, doing
 *	eol translations according to mode in effect currently.
 *
 * Results:
 *	Number of bytes stored in the result buffer (as opposed to the
 *	number of bytes read from the channel).  May return
 *	zero if no input is available to be translated.
 *
 * Side effects:
 *	Consumes buffered input. May deallocate one buffer.
 *
 *----------------------------------------------------------------------
 */

static int
CopyAndTranslateBuffer(statePtr, result, space)
    ChannelState *statePtr;	/* Channel state from which to read input. */
    char *result;		/* Where to store the copied input. */
    int space;			/* How many bytes are available in result
                                 * to store the copied input? */
{
    ChannelBuffer *bufPtr;	/* The buffer from which to copy bytes. */
    int bytesInBuffer;		/* How many bytes are available to be
                                 * copied in the current input buffer? */
    int copied;			/* How many characters were already copied
                                 * into the destination space? */
    int i;			/* Iterates over the copied input looking
                                 * for the input eofChar. */
    
    /*
     * If there is no input at all, return zero. The invariant is that either
     * there is no buffer in the queue, or if the first buffer is empty, it
     * is also the last buffer (and thus there is no input in the queue).
     * Note also that if the buffer is empty, we leave it in the queue.
     */
    
    if (statePtr->inQueueHead == (ChannelBuffer *) NULL) {
        return 0;
    }
    bufPtr = statePtr->inQueueHead;
    bytesInBuffer = bufPtr->nextAdded - bufPtr->nextRemoved;

    copied = 0;
    switch (statePtr->inputTranslation) {
        case TCL_TRANSLATE_LF: {
            if (bytesInBuffer == 0) {
                return 0;
            }

	    /*
             * Copy the current chunk into the result buffer.
             */

	    if (bytesInBuffer < space) {
		space = bytesInBuffer;
	    }
	    memcpy((VOID *) result,
		    (VOID *) (bufPtr->buf + bufPtr->nextRemoved),
		    (size_t) space);
	    bufPtr->nextRemoved += space;
	    copied = space;
            break;
	}
        case TCL_TRANSLATE_CR: {
	    char *end;
	    
            if (bytesInBuffer == 0) {
                return 0;
            }

	    /*
             * Copy the current chunk into the result buffer, then
             * replace all \r with \n.
             */

	    if (bytesInBuffer < space) {
		space = bytesInBuffer;
	    }
	    memcpy((VOID *) result,
		    (VOID *) (bufPtr->buf + bufPtr->nextRemoved),
		    (size_t) space);
	    bufPtr->nextRemoved += space;
	    copied = space;

	    for (end = result + copied; result < end; result++) {
		if (*result == '\r') {
		    *result = '\n';
		}
            }
            break;
	}
        case TCL_TRANSLATE_CRLF: {
	    char *src, *end, *dst;
	    int curByte;
	    
            /*
             * If there is a held-back "\r" at EOF, produce it now.
             */
            
	    if (bytesInBuffer == 0) {
                if ((statePtr->flags & (INPUT_SAW_CR | CHANNEL_EOF)) ==
                        (INPUT_SAW_CR | CHANNEL_EOF)) {
                    result[0] = '\r';
                    statePtr->flags &= ~INPUT_SAW_CR;
                    return 1;
                }
                return 0;
            }

            /*
             * Copy the current chunk and replace "\r\n" with "\n"
             * (but not standalone "\r"!).
             */

	    if (bytesInBuffer < space) {
		space = bytesInBuffer;
	    }
	    memcpy((VOID *) result,
		    (VOID *) (bufPtr->buf + bufPtr->nextRemoved),
		    (size_t) space);
	    bufPtr->nextRemoved += space;
	    copied = space;

	    end = result + copied;
	    dst = result;
	    for (src = result; src < end; src++) {
		curByte = *src;
		if (curByte == '\n') {
                    statePtr->flags &= ~INPUT_SAW_CR;
		} else if (statePtr->flags & INPUT_SAW_CR) {
		    statePtr->flags &= ~INPUT_SAW_CR;
		    *dst = '\r';
		    dst++;
		}
		if (curByte == '\r') {
		    statePtr->flags |= INPUT_SAW_CR;
		} else {
		    *dst = (char) curByte;
		    dst++;
		}
	    }
	    copied = dst - result;
	    break;
	}
        case TCL_TRANSLATE_AUTO: {
	    char *src, *end, *dst;
	    int curByte;
	
            if (bytesInBuffer == 0) {
                return 0;
            }

            /*
             * Loop over the current buffer, converting "\r" and "\r\n"
             * to "\n".
             */

	    if (bytesInBuffer < space) {
		space = bytesInBuffer;
	    }
	    memcpy((VOID *) result,
		    (VOID *) (bufPtr->buf + bufPtr->nextRemoved),
		    (size_t) space);
	    bufPtr->nextRemoved += space;
	    copied = space;

	    end = result + copied;
	    dst = result;
	    for (src = result; src < end; src++) {
		curByte = *src;
		if (curByte == '\r') {
		    statePtr->flags |= INPUT_SAW_CR;
		    *dst = '\n';
		    dst++;
		} else {
		    if ((curByte != '\n') || 
			    !(statePtr->flags & INPUT_SAW_CR)) {
			*dst = (char) curByte;
			dst++;
		    }
		    statePtr->flags &= ~INPUT_SAW_CR;
		}
	    }
	    copied = dst - result;
            break;
	}
        default: {
            panic("unknown eol translation mode");
	}
    }

    /*
     * If an in-stream EOF character is set for this channel, check that
     * the input we copied so far does not contain the EOF char.  If it does,
     * copy only up to and excluding that character.
     */
    
    if (statePtr->inEofChar != 0) {
        for (i = 0; i < copied; i++) {
            if (result[i] == (char) statePtr->inEofChar) {
		/*
		 * Set sticky EOF so that no further input is presented
		 * to the caller.
		 */
		
		statePtr->flags |= (CHANNEL_EOF | CHANNEL_STICKY_EOF);
		statePtr->inputEncodingFlags |= TCL_ENCODING_END;
		copied = i;
                break;
            }
        }
    }

    /*
     * If the current buffer is empty recycle it.
     */

    if (bufPtr->nextRemoved == bufPtr->nextAdded) {
        statePtr->inQueueHead = bufPtr->nextPtr;
        if (statePtr->inQueueHead == (ChannelBuffer *) NULL) {
            statePtr->inQueueTail = (ChannelBuffer *) NULL;
        }
        RecycleBuffer(statePtr, bufPtr, 0);
    }

    /*
     * Return the number of characters copied into the result buffer.
     * This may be different from the number of bytes consumed, because
     * of EOL translations.
     */

    return copied;
}

/*
 *----------------------------------------------------------------------
 *
 * DoWrite --
 *
 *	Puts a sequence of characters into an output buffer, may queue the
 *	buffer for output if it gets full, and also remembers whether the
 *	current buffer is ready e.g. if it contains a newline and we are in
 *	line buffering mode.
 *
 * Results:
 *	The number of bytes written or -1 in case of error. If -1,
 *	Tcl_GetErrno will return the error code.
 *
 * Side effects:
 *	May buffer up output and may cause output to be produced on the
 *	channel.
 *
 *----------------------------------------------------------------------
 */

static int
DoWrite(chanPtr, src, srcLen)
    Channel *chanPtr;			/* The channel to buffer output for. */
    char *src;				/* Data to write. */
    int srcLen;				/* Number of bytes to write. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    ChannelBuffer *outBufPtr;		/* Current output buffer. */
    int foundNewline;			/* Did we find a newline in output? */
    char *dPtr;
    char *sPtr;				/* Search variables for newline. */
    int crsent;				/* In CRLF eol translation mode,
                                         * remember the fact that a CR was
                                         * output to the channel without
                                         * its following NL. */
    int i;				/* Loop index for newline search. */
    int destCopied;			/* How many bytes were used in this
                                         * destination buffer to hold the
                                         * output? */
    int totalDestCopied;		/* How many bytes total were
                                         * copied to the channel buffer? */
    int srcCopied;			/* How many bytes were copied from
                                         * the source string? */
    char *destPtr;			/* Where in line to copy to? */

    /*
     * If we are in network (or windows) translation mode, record the fact
     * that we have not yet sent a CR to the channel.
     */

    crsent = 0;
    
    /*
     * Loop filling buffers and flushing them until all output has been
     * consumed.
     */

    srcCopied = 0;
    totalDestCopied = 0;

    while (srcLen > 0) {
        
        /*
         * Make sure there is a current output buffer to accept output.
         */

        if (statePtr->curOutPtr == (ChannelBuffer *) NULL) {
            statePtr->curOutPtr = AllocChannelBuffer(statePtr->bufSize);
        }

        outBufPtr = statePtr->curOutPtr;

        destCopied = outBufPtr->bufLength - outBufPtr->nextAdded;
        if (destCopied > srcLen) {
            destCopied = srcLen;
        }
        
        destPtr = outBufPtr->buf + outBufPtr->nextAdded;
        switch (statePtr->outputTranslation) {
            case TCL_TRANSLATE_LF:
                srcCopied = destCopied;
                memcpy((VOID *) destPtr, (VOID *) src, (size_t) destCopied);
                break;
            case TCL_TRANSLATE_CR:
                srcCopied = destCopied;
                memcpy((VOID *) destPtr, (VOID *) src, (size_t) destCopied);
                for (dPtr = destPtr; dPtr < destPtr + destCopied; dPtr++) {
                    if (*dPtr == '\n') {
                        *dPtr = '\r';
                    }
                }
                break;
            case TCL_TRANSLATE_CRLF:
                for (srcCopied = 0, dPtr = destPtr, sPtr = src;
                     dPtr < destPtr + destCopied;
                     dPtr++, sPtr++, srcCopied++) {
                    if (*sPtr == '\n') {
                        if (crsent) {
                            *dPtr = '\n';
                            crsent = 0;
                        } else {
                            *dPtr = '\r';
                            crsent = 1;
                            sPtr--, srcCopied--;
                        }
                    } else {
                        *dPtr = *sPtr;
                    }
                }
                break;
            case TCL_TRANSLATE_AUTO:
                panic("Tcl_Write: AUTO output translation mode not supported");
            default:
                panic("Tcl_Write: unknown output translation mode");
        }

        /*
         * The current buffer is ready for output if it is full, or if it
         * contains a newline and this channel is line-buffered, or if it
         * contains any output and this channel is unbuffered.
         */

        outBufPtr->nextAdded += destCopied;
        if (!(statePtr->flags & BUFFER_READY)) {
            if (outBufPtr->nextAdded == outBufPtr->bufLength) {
                statePtr->flags |= BUFFER_READY;
            } else if (statePtr->flags & CHANNEL_LINEBUFFERED) {
                for (sPtr = src, i = 0, foundNewline = 0;
		     (i < srcCopied) && (!foundNewline);
		     i++, sPtr++) {
                    if (*sPtr == '\n') {
                        foundNewline = 1;
                        break;
                    }
                }
                if (foundNewline) {
                    statePtr->flags |= BUFFER_READY;
                }
            } else if (statePtr->flags & CHANNEL_UNBUFFERED) {
                statePtr->flags |= BUFFER_READY;
            }
        }
        
        totalDestCopied += srcCopied;
        src += srcCopied;
        srcLen -= srcCopied;

        if (statePtr->flags & BUFFER_READY) {
            if (FlushChannel(NULL, chanPtr, 0) != 0) {
                return -1;
            }
        }
    } /* Closes "while" */

    return totalDestCopied;
}

/*
 *----------------------------------------------------------------------
 *
 * CopyEventProc --
 *
 *	This routine is invoked as a channel event handler for
 *	the background copy operation.  It is just a trivial wrapper
 *	around the CopyData routine.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
CopyEventProc(clientData, mask)
    ClientData clientData;
    int mask;
{
    (void) CopyData((CopyState *)clientData, mask);
}

/*
 *----------------------------------------------------------------------
 *
 * StopCopy --
 *
 *	This routine halts a copy that is in progress.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes any pending channel handlers and restores the blocking
 *	and buffering modes of the channels.  The CopyState is freed.
 *
 *----------------------------------------------------------------------
 */

static void
StopCopy(csPtr)
    CopyState *csPtr;		/* State for bg copy to stop . */
{
    ChannelState *inStatePtr, *outStatePtr;
    int nonBlocking;

    if (!csPtr) {
	return;
    }

    inStatePtr	= csPtr->readPtr->state;
    outStatePtr	= csPtr->writePtr->state;

    /*
     * Restore the old blocking mode and output buffering mode.
     */

    nonBlocking = (csPtr->readFlags & CHANNEL_NONBLOCKING);
    if (nonBlocking != (inStatePtr->flags & CHANNEL_NONBLOCKING)) {
	SetBlockMode(NULL, csPtr->readPtr,
		nonBlocking ? TCL_MODE_NONBLOCKING : TCL_MODE_BLOCKING);
    }
    if (csPtr->readPtr != csPtr->writePtr) {
	if (nonBlocking != (outStatePtr->flags & CHANNEL_NONBLOCKING)) {
	    SetBlockMode(NULL, csPtr->writePtr,
		    nonBlocking ? TCL_MODE_NONBLOCKING : TCL_MODE_BLOCKING);
	}
    }
    outStatePtr->flags &= ~(CHANNEL_LINEBUFFERED | CHANNEL_UNBUFFERED);
    outStatePtr->flags |=
	csPtr->writeFlags & (CHANNEL_LINEBUFFERED | CHANNEL_UNBUFFERED);

    if (csPtr->cmdPtr) {
	Tcl_DeleteChannelHandler((Tcl_Channel)csPtr->readPtr, CopyEventProc,
		(ClientData)csPtr);
	if (csPtr->readPtr != csPtr->writePtr) {
	    Tcl_DeleteChannelHandler((Tcl_Channel)csPtr->writePtr,
		    CopyEventProc, (ClientData)csPtr);
	}
        Tcl_DecrRefCount(csPtr->cmdPtr);
    }
    inStatePtr->csPtr  = NULL;
    outStatePtr->csPtr = NULL;
    ckfree((char*) csPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * StackSetBlockMode --
 *
 *	This function sets the blocking mode for a channel, iterating
 *	through each channel in a stack and updates the state flags.
 *
 * Results:
 *	0 if OK, result code from failed blockModeProc otherwise.
 *
 * Side effects:
 *	Modifies the blocking mode of the channel and possibly generates
 *	an error.
 *
 *----------------------------------------------------------------------
 */

static int
StackSetBlockMode(chanPtr, mode)
    Channel *chanPtr;		/* Channel to modify. */
    int mode;			/* One of TCL_MODE_BLOCKING or
				 * TCL_MODE_NONBLOCKING. */
{
    int result = 0;
    Tcl_DriverBlockModeProc *blockModeProc;

    /*
     * Start at the top of the channel stack
     */

    chanPtr = chanPtr->state->topChanPtr;
    while (chanPtr != (Channel *) NULL) {
	blockModeProc = Tcl_ChannelBlockModeProc(chanPtr->typePtr);
	if (blockModeProc != NULL) {
	    result = (*blockModeProc) (chanPtr->instanceData, mode);
	    if (result != 0) {
		Tcl_SetErrno(result);
		return result;
	    }
	}
	chanPtr = chanPtr->downChanPtr;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SetBlockMode --
 *
 *	This function sets the blocking mode for a channel and updates
 *	the state flags.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Modifies the blocking mode of the channel and possibly generates
 *	an error.
 *
 *----------------------------------------------------------------------
 */

static int
SetBlockMode(interp, chanPtr, mode)
    Tcl_Interp *interp;		/* Interp for error reporting. */
    Channel *chanPtr;		/* Channel to modify. */
    int mode;			/* One of TCL_MODE_BLOCKING or
				 * TCL_MODE_NONBLOCKING. */
{
    ChannelState *statePtr = chanPtr->state;	/* state info for channel */
    int result = 0;

    result = StackSetBlockMode(chanPtr, mode);
    if (result != 0) {
	if (interp != (Tcl_Interp *) NULL) {
	    Tcl_AppendResult(interp, "error setting blocking mode: ",
		    Tcl_PosixError(interp), (char *) NULL);
	}
	return TCL_ERROR;
    }
    if (mode == TCL_MODE_BLOCKING) {
	statePtr->flags &= (~(CHANNEL_NONBLOCKING | BG_FLUSH_SCHEDULED));
    } else {
	statePtr->flags |= CHANNEL_NONBLOCKING;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelNames --
 *
 *	Return the names of all open channels in the interp.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	Interp result modified with list of channel names.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetChannelNames(interp)
    Tcl_Interp *interp;		/* Interp for error reporting. */
{
    return Tcl_GetChannelNamesEx(interp, (char *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelNamesEx --
 *
 *	Return the names of open channels in the interp filtered
 *	filtered through a pattern.  If pattern is NULL, it returns
 *	all the open channels.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	Interp result modified with list of channel names.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetChannelNamesEx(interp, pattern)
    Tcl_Interp *interp;		/* Interp for error reporting. */
    char *pattern;		/* pattern to filter on. */
{
    ChannelState *statePtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    char *name;
    Tcl_Obj *resultPtr;

    resultPtr = Tcl_GetObjResult(interp);
    for (statePtr = tsdPtr->firstCSPtr;
	 statePtr != NULL;
	 statePtr = statePtr->nextCSPtr) {
        if (statePtr->topChanPtr == (Channel *) tsdPtr->stdinChannel) {
	    name = "stdin";
	} else if (statePtr->topChanPtr == (Channel *) tsdPtr->stdoutChannel) {
	    name = "stdout";
	} else if (statePtr->topChanPtr == (Channel *) tsdPtr->stderrChannel) {
	    name = "stderr";
	} else {
	    name = statePtr->channelName;
	}
	if (((pattern == NULL) || Tcl_StringMatch(name, pattern)) &&
		(Tcl_ListObjAppendElement(interp, resultPtr,
			Tcl_NewStringObj(name, -1)) != TCL_OK)) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}



/*
 * tclIOGT.c --
 *
 *	Implements a generic transformation exposing the underlying API
 *	at the script level.
 *
 *
 * Copyright (c) 1999 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 * SOFTWARE AND ITS DOCUMENTATION, EVEN IF I HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * I SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND
 * I HAVE NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * CVS: $Id: tclIO.c,v 1.20.2.3.2.4 2000/07/14 04:16:12 hobbs Exp $
 */

/*
  #include <string.h>
  #include "tclInt.h"
  #include "tclIO.h"
*/


/*
 *------------------------------------------------------*
 *
 *	ExecuteCallback --
 *
 *	Executes the defined callback for buffer and
 *	operation.
 *
 *	Sideeffects:
 *		As of the executed tcl script.
 *
 *	Result:
 *		A standard TCL error code. In case of an
 *		error a message is left in the result area
 *		of the specified interpreter.
 *
 *------------------------------------------------------*
 */

static int
ExecuteCallback (dataPtr, interp, op, buf, bufLen, transmit, preserve)
    TransformChannelData* dataPtr;     /* Transformation with the callback */
    Tcl_Interp*           interp;   /* Current interpreter, possibly NULL */
    unsigned char*        op;       /* Operation invoking the callback */
    unsigned char*        buf;      /* Buffer to give to the script. */
    int                   bufLen;   /* Ands its length */
    int                   transmit; /* Flag, determines whether the result
				     * of the callback is sent to the
				     * underlying channel or not. */
    int                   preserve; /* Flag. If true the procedure will
				     * preserver the result state of all
				     * accessed interpreters. */
{
    /*
     * Step 1, create the complete command to execute. Do this by appending
     * operation and buffer to operate upon to a copy of the callback
     * definition. We *cannot* create a list containing 3 objects and then use
     * 'Tcl_EvalObjv', because the command may contain additional prefixed
     * arguments. Feather's curried commands would come in handy here.
     */

    Tcl_Obj*        resObj; /* See below, switch (transmit) */
    int             resLen;
    unsigned char*  resBuf;
    Tcl_SavedResult ciSave;

    int res = TCL_OK;
    Tcl_Obj* command = Tcl_DuplicateObj (dataPtr->command);
    Tcl_Obj* temp;


    if (preserve) {
	Tcl_SaveResult (dataPtr->interp, &ciSave);
    }

    if (command == (Tcl_Obj*) NULL) {
        /* Memory allocation problem */
        res = TCL_ERROR;
        goto cleanup;
    }

    Tcl_IncrRefCount(command);

    temp = Tcl_NewStringObj((char*) op, -1);

    if (temp == (Tcl_Obj*) NULL) {
        /* Memory allocation problem */
        res = TCL_ERROR;
        goto cleanup;
    }

    res = Tcl_ListObjAppendElement(dataPtr->interp, command, temp);

    if (res != TCL_OK)
	goto cleanup;

    /*
     * Use a byte-array to prevent the misinterpretation of binary data
     * coming through as UTF while at the tcl level.
     */

    temp = Tcl_NewByteArrayObj(buf, bufLen);

    if (temp == (Tcl_Obj*) NULL) {
        /* Memory allocation problem */
	res = TCL_ERROR;
        goto cleanup;
    }

    res = Tcl_ListObjAppendElement (dataPtr->interp, command, temp);

    if (res != TCL_OK)
        goto cleanup;

    /*
     * Step 2, execute the command at the global level of the interpreter
     * used to create the transformation. Destroy the command afterward.
     * If an error occured and the current interpreter is defined and not
     * equal to the interpreter for the callback, then copy the error
     * message into current interpreter. Don't copy if in preservation mode.
     */

    res = Tcl_GlobalEvalObj (dataPtr->interp, command);
    Tcl_DecrRefCount (command);
    command = (Tcl_Obj*) NULL;

    if ((res != TCL_OK) && (interp != NO_INTERP) &&
	    (dataPtr->interp != interp) && !preserve) {
        Tcl_SetObjResult(interp, Tcl_GetObjResult(dataPtr->interp));
	return res;
    }

    /*
     * Step 3, transmit a possible conversion result to the underlying
     * channel, or ourselves.
     */

    switch (transmit) {
	case TRANSMIT_DONT:
	    /* nothing to do */
	    break;

	case TRANSMIT_DOWN:
	    resObj = Tcl_GetObjResult(dataPtr->interp);
	    resBuf = (unsigned char*) Tcl_GetByteArrayFromObj(resObj, &resLen);
	    Tcl_WriteRaw(Tcl_GetStackedChannel(dataPtr->self),
		    (char*) resBuf, resLen);
	    break;

	case TRANSMIT_SELF:
	    resObj = Tcl_GetObjResult (dataPtr->interp);
	    resBuf = (unsigned char*) Tcl_GetByteArrayFromObj(resObj, &resLen);
	    Tcl_WriteRaw(dataPtr->self, (char*) resBuf, resLen);
	    break;

	case TRANSMIT_IBUF:
	    resObj = Tcl_GetObjResult (dataPtr->interp);
	    resBuf = (unsigned char*) Tcl_GetByteArrayFromObj(resObj, &resLen);
	    ResultAdd(&dataPtr->result, resBuf, resLen);
	    break;

	case TRANSMIT_NUM:
	    /* Interpret result as integer number */
	    resObj = Tcl_GetObjResult (dataPtr->interp);
	    Tcl_GetIntFromObj(dataPtr->interp, resObj, &dataPtr->maxRead);
	    break;
    }

    Tcl_ResetResult(dataPtr->interp);

    if (preserve) {
	Tcl_RestoreResult(dataPtr->interp, &ciSave);
    }

    return res;

    cleanup:
    if (preserve) {
	Tcl_RestoreResult(dataPtr->interp, &ciSave);
    }

    if (command != (Tcl_Obj*) NULL) {
        Tcl_DecrRefCount(command);
    }

    return res;
}

/*
 *------------------------------------------------------*
 *
 *	TransformBlockModeProc --
 *
 *	Trap handler. Called by the generic IO system
 *	during option processing to change the blocking
 *	mode of the channel.
 *
 *	Sideeffects:
 *		Forwards the request to the underlying
 *		channel.
 *
 *	Result:
 *		0 if successful, errno when failed.
 *
 *------------------------------------------------------*
 */

static int
TransformBlockModeProc (instanceData, mode)
    ClientData  instanceData; /* State of transformation */
    int         mode;         /* New blocking mode */
{
    TransformChannelData* dataPtr = (TransformChannelData*) instanceData;

    if (mode == TCL_MODE_NONBLOCKING) {
        dataPtr->flags |= CHANNEL_ASYNC;
    } else {
        dataPtr->flags &= ~(CHANNEL_ASYNC);
    }
    return 0;
}

/*
 *------------------------------------------------------*
 *
 *	TransformCloseProc --
 *
 *	Trap handler. Called by the generic IO system
 *	during destruction of the transformation channel.
 *
 *	Sideeffects:
 *		Releases the memory allocated in
 *		'Tcl_TransformObjCmd'.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static int
TransformCloseProc (instanceData, interp)
    ClientData  instanceData;
    Tcl_Interp* interp;
{
    TransformChannelData* dataPtr = (TransformChannelData*) instanceData;

    /*
     * Important: In this procedure 'dataPtr->self' already points to
     * the underlying channel.
     */

    /*
     * There is no need to cancel an existing channel handler, this is already
     * done. Either by 'Tcl_UnstackChannel' or by the general cleanup in
     * 'Tcl_Close'.
     *
     * But we have to cancel an active timer to prevent it from firing on the
     * removed channel.
     */

    if (dataPtr->timer != (Tcl_TimerToken) NULL) {
        Tcl_DeleteTimerHandler (dataPtr->timer);
	dataPtr->timer = (Tcl_TimerToken) NULL;
    }

    /*
     * Now flush data waiting in internal buffers to output and input. The
     * input must be done despite the fact that there is no real receiver
     * for it anymore. But the scripts might have sideeffects other parts
     * of the system rely on (f.e. signaling the close to interested parties).
     */

    if (dataPtr->mode & TCL_WRITABLE) {
        ExecuteCallback (dataPtr, interp, A_FLUSH_WRITE,
		NULL, 0, TRANSMIT_DOWN, 1);
    }

    if ((dataPtr->mode & TCL_READABLE) && !dataPtr->readIsFlushed) {
	dataPtr->readIsFlushed = 1;
        ExecuteCallback (dataPtr, interp, A_FLUSH_READ,
		NULL, 0, TRANSMIT_IBUF, 1);
    }

    if (dataPtr->mode & TCL_WRITABLE) {
        ExecuteCallback (dataPtr, interp, A_DELETE_WRITE,
		NULL, 0, TRANSMIT_DONT, 1);
    }

    if (dataPtr->mode & TCL_READABLE) {
        ExecuteCallback (dataPtr, interp, A_DELETE_READ,
		NULL, 0, TRANSMIT_DONT, 1);
    }

    /*
     * General cleanup
     */

    ResultClear(&dataPtr->result);
    Tcl_DecrRefCount(dataPtr->command);
    ckfree((VOID*) dataPtr);

    return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	TransformInputProc --
 *
 *	Called by the generic IO system to convert read data.
 *
 *	Sideeffects:
 *		As defined by the conversion.
 *
 *	Result:
 *		A transformed buffer.
 *
 *------------------------------------------------------*
 */

static int
TransformInputProc (instanceData, buf, toRead, errorCodePtr)
    ClientData instanceData;
    char*      buf;
    int        toRead;
    int*       errorCodePtr;
{
    TransformChannelData* dataPtr = (TransformChannelData*) instanceData;
    int gotBytes, read, res, copied;
    Tcl_Channel downChan;

    /* should assert (dataPtr->mode & TCL_READABLE) */

    if (toRead == 0) {
	/* Catch a no-op.
	 */
	return 0;
    }

    gotBytes = 0;
    downChan = Tcl_GetStackedChannel(dataPtr->self);

    while (toRead > 0) {
        /*
	 * Loop until the request is satisfied (or no data is available from
	 * below, possibly EOF).
	 */

        copied    = ResultCopy (&dataPtr->result, UCHARP (buf), toRead);

	toRead   -= copied;
	buf      += copied;
	gotBytes += copied;

	if (toRead == 0) {
	    /* The request was completely satisfied from our buffers.
	     * We can break out of the loop and return to the caller.
	     */
	    return gotBytes;
	}

	/*
	 * Length (dataPtr->result) == 0, toRead > 0 here . Use the incoming
	 * 'buf'! as target to store the intermediary information read
	 * from the underlying channel.
	 *
	 * Ask the tcl level how much data it allows us to read from
	 * the underlying channel. This feature allows the transform to
	 * signal EOF upstream although there is none downstream. Useful
	 * to control an unbounded 'fcopy', either through counting bytes,
	 * or by pattern matching.
	 */

	ExecuteCallback (dataPtr, NO_INTERP, A_QUERY_MAXREAD,
		NULL, 0, TRANSMIT_NUM /* -> maxRead */, 1);

	if (dataPtr->maxRead >= 0) {
	    if (dataPtr->maxRead < toRead) {
	        toRead = dataPtr->maxRead;
	    }
	} /* else: 'maxRead < 0' == Accept the current value of toRead */

	if (toRead <= 0) {
	    return gotBytes;
	}

	read = Tcl_ReadRaw(downChan, buf, toRead);

	if (read < 0) {
	    /* Report errors to caller. EAGAIN is a special situation.
	     * If we had some data before we report that instead of the
	     * request to re-try.
	     */

	    if ((Tcl_GetErrno() == EAGAIN) && (gotBytes > 0)) {
	        return gotBytes;
	    }

	    *errorCodePtr = Tcl_GetErrno();
	    return -1;      
	}

	if (read == 0) {
	    /*
	     * Check wether we hit on EOF in the underlying channel or
	     * not. If not differentiate between blocking and
	     * non-blocking modes. In non-blocking mode we ran
	     * temporarily out of data. Signal this to the caller via
	     * EWOULDBLOCK and error return (-1). In the other cases
	     * we simply return what we got and let the caller wait
	     * for more. On the other hand, if we got an EOF we have
	     * to convert and flush all waiting partial data.
	     */

	    if (! Tcl_Eof (downChan)) {
	        if ((gotBytes == 0) && (dataPtr->flags & CHANNEL_ASYNC)) {
		    *errorCodePtr = EWOULDBLOCK;
		    return -1;
		} else {
		    return gotBytes;
		}
	    } else {
	        if (dataPtr->readIsFlushed) {
		    /* Already flushed, nothing to do anymore
		     */
		    return gotBytes;
		}

		dataPtr->readIsFlushed = 1;

		ExecuteCallback (dataPtr, NO_INTERP, A_FLUSH_READ,
			NULL, 0, TRANSMIT_IBUF, P_PRESERVE);

		if (ResultLength (&dataPtr->result) == 0) {
		    /* we had nothing to flush */
		    return gotBytes;
		}

		continue; /* at: while (toRead > 0) */
	    }
	} /* read == 0 */

	/* Transform the read chunk and add the result to our
	 * read buffer (dataPtr->result)
	 */

	res = ExecuteCallback (dataPtr, NO_INTERP, A_READ,
		UCHARP (buf), read, TRANSMIT_IBUF,
		P_PRESERVE);

	if (res != TCL_OK) {
	    *errorCodePtr = EINVAL;
	    return -1;
	}
    } /* while toRead > 0 */

    return gotBytes;
}

/*
 *------------------------------------------------------*
 *
 *	TransformOutputProc --
 *
 *	Called by the generic IO system to convert data
 *	waiting to be written.
 *
 *	Sideeffects:
 *		As defined by the transformation.
 *
 *	Result:
 *		A transformed buffer.
 *
 *------------------------------------------------------*
 */

static int
TransformOutputProc (instanceData, buf, toWrite, errorCodePtr)
    ClientData instanceData;
    char*      buf;
    int        toWrite;
    int*       errorCodePtr;
{
    TransformChannelData* dataPtr = (TransformChannelData*) instanceData;
    int res;

    /* should assert (dataPtr->mode & TCL_WRITABLE) */

    if (toWrite == 0) {
	/* Catch a no-op.
	 */
	return 0;
    }

    res = ExecuteCallback (dataPtr, NO_INTERP, A_WRITE,
	    UCHARP (buf), toWrite,
	    TRANSMIT_DOWN, P_NO_PRESERVE);

    if (res != TCL_OK) {
        *errorCodePtr = EINVAL;
	return -1;
    }

    return toWrite;
}

/*
 *------------------------------------------------------*
 *
 *	TransformSeekProc --
 *
 *	This procedure is called by the generic IO level
 *	to move the access point in a channel.
 *
 *	Sideeffects:
 *		Moves the location at which the channel
 *		will be accessed in future operations.
 *		Flushes all transformation buffers, then
 *		forwards it to the underlying channel.
 *
 *	Result:
 *		-1 if failed, the new position if
 *		successful. An output argument contains
 *		the POSIX error code if an error
 *		occurred, or zero.
 *
 *------------------------------------------------------*
 */

static int
TransformSeekProc (instanceData, offset, mode, errorCodePtr)
    ClientData instanceData;	/* The channel to manipulate */
    long       offset;		/* Size of movement. */
    int        mode;		/* How to move */
    int*       errorCodePtr;	/* Location of error flag. */
{
    int result;
    TransformChannelData* dataPtr	= (TransformChannelData*) instanceData;
    Tcl_Channel           parent        = Tcl_GetStackedChannel(dataPtr->self);
    Tcl_ChannelType*      parentType	= Tcl_GetChannelType(parent);
    Tcl_DriverSeekProc*   parentSeekProc = Tcl_ChannelSeekProc(parentType);

    if ((offset == 0) && (mode == SEEK_CUR)) {
        /* This is no seek but a request to tell the caller the current
	 * location. Simply pass the request down.
	 */

	result = (*parentSeekProc) (Tcl_GetChannelInstanceData(parent),
		offset, mode, errorCodePtr);
	return result;
    }

    /*
     * It is a real request to change the position. Flush all data waiting
     * for output and discard everything in the input buffers. Then pass
     * the request down, unchanged.
     */

    if (dataPtr->mode & TCL_WRITABLE) {
        ExecuteCallback (dataPtr, NO_INTERP, A_FLUSH_WRITE,
		NULL, 0, TRANSMIT_DOWN, P_NO_PRESERVE);
    }

    if (dataPtr->mode & TCL_READABLE) {
        ExecuteCallback (dataPtr, NO_INTERP, A_CLEAR_READ,
		NULL, 0, TRANSMIT_DONT, P_NO_PRESERVE);
	ResultClear(&dataPtr->result);
	dataPtr->readIsFlushed = 0;
    }

    result = (*parentSeekProc) (Tcl_GetChannelInstanceData(parent),
	    offset, mode, errorCodePtr);
    return result;
}

/*
 *------------------------------------------------------*
 *
 *	TransformSetOptionProc --
 *
 *	Called by generic layer to handle the reconfi-
 *	guration of channel specific options. As this
 *	channel type does not have such, it simply passes
 *	all requests downstream.
 *
 *	Sideeffects:
 *		As defined by the channel downstream.
 *
 *	Result:
 *		A standard TCL error code.
 *
 *------------------------------------------------------*
 */

static int
TransformSetOptionProc (instanceData, interp, optionName, value)
    ClientData instanceData;
    Tcl_Interp *interp;
    char *optionName;
    char *value;
{
    TransformChannelData* dataPtr = (TransformChannelData*) instanceData;
    Tcl_Channel downChan = Tcl_GetStackedChannel(dataPtr->self);
    Tcl_DriverSetOptionProc *setOptionProc;

    setOptionProc = Tcl_ChannelSetOptionProc(Tcl_GetChannelType(downChan));
    if (setOptionProc != NULL) {
	return (*setOptionProc)(Tcl_GetChannelInstanceData(downChan),
		interp, optionName, value);
    }
    return TCL_ERROR;
}

/*
 *------------------------------------------------------*
 *
 *	TransformGetOptionProc --
 *
 *	Called by generic layer to handle requests for
 *	the values of channel specific options. As this
 *	channel type does not have such, it simply passes
 *	all requests downstream.
 *
 *	Sideeffects:
 *		As defined by the channel downstream.
 *
 *	Result:
 *		A standard TCL error code.
 *
 *------------------------------------------------------*
 */

static int
TransformGetOptionProc (instanceData, interp, optionName, dsPtr)
    ClientData   instanceData;
    Tcl_Interp*  interp;
    char*        optionName;
    Tcl_DString* dsPtr;
{
    TransformChannelData* dataPtr = (TransformChannelData*) instanceData;
    Tcl_Channel downChan = Tcl_GetStackedChannel(dataPtr->self);
    Tcl_DriverGetOptionProc *getOptionProc;

    getOptionProc = Tcl_ChannelGetOptionProc(Tcl_GetChannelType(downChan));
    if (getOptionProc != NULL) {
	return (*getOptionProc)(Tcl_GetChannelInstanceData(downChan),
		interp, optionName, dsPtr);
    } else if (optionName == (char*) NULL) {
	/*
	 * Request is query for all options, this is ok.
	 */
	return TCL_OK;
    }
    /*
     * Request for a specific option has to fail, we don't have any.
     */
    return TCL_ERROR;
}

/*
 *------------------------------------------------------*
 *
 *	TransformWatchProc --
 *
 *	Initialize the notifier to watch for events from
 *	this channel.
 *
 *	Sideeffects:
 *		Sets up the notifier so that a future
 *		event on the channel will be seen by Tcl.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
static void
TransformWatchProc (instanceData, mask)
    ClientData instanceData;	/* Channel to watch */
    int        mask;		/* Events of interest */
{
    /* The caller expressed interest in events occuring for this
     * channel. We are forwarding the call to the underlying
     * channel now.
     */

    TransformChannelData* dataPtr = (TransformChannelData*) instanceData;
    Tcl_Channel     downChan;

    dataPtr->watchMask = mask;

    /* No channel handlers any more. We will be notified automatically
     * about events on the channel below via a call to our
     * 'TransformNotifyProc'. But we have to pass the interest down now.
     * We are allowed to add additional 'interest' to the mask if we want
     * to. But this transformation has no such interest. It just passes
     * the request down, unchanged.
     */

    downChan = Tcl_GetStackedChannel(dataPtr->self);

    (Tcl_GetChannelType(downChan))
	->watchProc(Tcl_GetChannelInstanceData(downChan), mask);

    /*
     * Management of the internal timer.
     */

    if ((dataPtr->timer != (Tcl_TimerToken) NULL) &&
	    (!(mask & TCL_READABLE) || (ResultLength(&dataPtr->result) == 0))) {

        /* A pending timer exists, but either is there no (more)
	 * interest in the events it generates or nothing is availablee
	 * for reading, so remove it.
	 */

        Tcl_DeleteTimerHandler (dataPtr->timer);
	dataPtr->timer = (Tcl_TimerToken) NULL;
    }

    if ((dataPtr->timer == (Tcl_TimerToken) NULL) &&
	    (mask & TCL_READABLE) && (ResultLength (&dataPtr->result) > 0)) {

        /* There is no pending timer, but there is interest in readable
	 * events and we actually have data waiting, so generate a timer
	 * to flush that.
	 */

	dataPtr->timer = Tcl_CreateTimerHandler (DELAY,
		TransformChannelHandlerTimer, (ClientData) dataPtr);
    }
}

/*
 *------------------------------------------------------*
 *
 *	TransformGetFileHandleProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve
 *	OS specific file handle from inside this channel.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		The appropriate Tcl_File or NULL if not
 *		present. 
 *
 *------------------------------------------------------*
 */
static int
TransformGetFileHandleProc (instanceData, direction, handlePtr)
    ClientData  instanceData;	/* Channel to query */
    int         direction;	/* Direction of interest */
    ClientData* handlePtr;	/* Place to store the handle into */
{
    /*
     * Return the handle belonging to parent channel.
     * IOW, pass the request down and the result up.
     */

    TransformChannelData* dataPtr = (TransformChannelData*) instanceData;

    return Tcl_GetChannelHandle(Tcl_GetStackedChannel(dataPtr->self),
	    direction, handlePtr);
}

/*
 *------------------------------------------------------*
 *
 *	TransformNotifyProc --
 *
 *	------------------------------------------------*
 *	Handler called by Tcl to inform us of activity
 *	on the underlying channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		May process the incoming event by itself.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static int
TransformNotifyProc (clientData, mask)
    ClientData	   clientData; /* The state of the notified transformation */
    int		   mask;       /* The mask of occuring events */
{
    TransformChannelData* dataPtr = (TransformChannelData*) clientData;

    /*
     * An event occured in the underlying channel.  This
     * transformation doesn't process such events thus returns the
     * incoming mask unchanged.
     */

    if (dataPtr->timer != (Tcl_TimerToken) NULL) {
	/*
	 * Delete an existing timer. It was not fired, yet we are
	 * here, so the channel below generated such an event and we
	 * don't have to. The renewal of the interest after the
	 * execution of channel handlers will eventually cause us to
	 * recreate the timer (in TransformWatchProc).
	 */

	Tcl_DeleteTimerHandler (dataPtr->timer);
	dataPtr->timer = (Tcl_TimerToken) NULL;
    }

    return mask;
}

/*
 *------------------------------------------------------*
 *
 *	TransformChannelHandlerTimer --
 *
 *	Called by the notifier (-> timer) to flush out
 *	information waiting in the input buffer.
 *
 *	Sideeffects:
 *		As of 'Tcl_NotifyChannel'.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
TransformChannelHandlerTimer (clientData)
    ClientData clientData; /* Transformation to query */
{
    TransformChannelData* dataPtr = (TransformChannelData*) clientData;

    dataPtr->timer = (Tcl_TimerToken) NULL;

    if (!(dataPtr->watchMask & TCL_READABLE) ||
	    (ResultLength (&dataPtr->result) == 0)) {
	/* The timer fired, but either is there no (more)
	 * interest in the events it generates or nothing is available
	 * for reading, so ignore it and don't recreate it.
	 */

	return;
    }

    Tcl_NotifyChannel(dataPtr->self, TCL_READABLE);
}

/*
 *------------------------------------------------------*
 *
 *	ResultClear --
 *
 *	Deallocates any memory allocated by 'ResultAdd'.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ResultClear (r)
    ResultBuffer* r; /* Reference to the buffer to clear out */
{
    r->used = 0;

    if (r->allocated) {
        ckfree((char*) r->buf);
	r->buf       = UCHARP (NULL);
	r->allocated = 0;
    }
}

/*
 *------------------------------------------------------*
 *
 *	ResultInit --
 *
 *	Initializes the specified buffer structure. The
 *	structure will contain valid information for an
 *	emtpy buffer.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ResultInit (r)
    ResultBuffer* r; /* Reference to the structure to initialize */
{
    r->used      = 0;
    r->allocated = 0;
    r->buf       = UCHARP (NULL);
}

/*
 *------------------------------------------------------*
 *
 *	ResultLength --
 *
 *	Returns the number of bytes stored in the buffer.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		An integer, see above too.
 *
 *------------------------------------------------------*
 */

static int
ResultLength (r)
    ResultBuffer* r; /* The structure to query */
{
    return r->used;
}

/*
 *------------------------------------------------------*
 *
 *	ResultCopy --
 *
 *	Copies the requested number of bytes from the
 *	buffer into the specified array and removes them
 *	from the buffer afterward. Copies less if there
 *	is not enough data in the buffer.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		The number of actually copied bytes,
 *		possibly less than 'toRead'.
 *
 *------------------------------------------------------*
 */

static int
ResultCopy (r, buf, toRead)
    ResultBuffer*  r;      /* The buffer to read from */
    unsigned char* buf;    /* The buffer to copy into */
    int            toRead; /* Number of requested bytes */
{
    if (r->used == 0) {
        /* Nothing to copy in the case of an empty buffer.
	 */

        return 0;
    }

    if (r->used == toRead) {
        /* We have just enough. Copy everything to the caller.
	 */

        memcpy ((VOID*) buf, (VOID*) r->buf, (size_t) toRead);
	r->used = 0;
	return toRead;
    }

    if (r->used > toRead) {
        /* The internal buffer contains more than requested.
	 * Copy the requested subset to the caller, and shift
	 * the remaining bytes down.
	 */

        memcpy  ((VOID*) buf,    (VOID*) r->buf,            (size_t) toRead);
	memmove ((VOID*) r->buf, (VOID*) (r->buf + toRead),
		(size_t) r->used - toRead);

	r->used -= toRead;
	return toRead;
    }

    /* There is not enough in the buffer to satisfy the caller, so
     * take everything.
     */

    memcpy((VOID*) buf, (VOID*) r->buf, (size_t) r->used);
    toRead  = r->used;
    r->used = 0;
    return toRead;
}

/*
 *------------------------------------------------------*
 *
 *	ResultAdd --
 *
 *	Adds the bytes in the specified array to the
 *	buffer, by appending it.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ResultAdd (r, buf, toWrite)
    ResultBuffer*  r;       /* The buffer to extend */
    unsigned char* buf;     /* The buffer to read from */
    int            toWrite; /* The number of bytes in 'buf' */
{
    if ((r->used + toWrite) > r->allocated) {
        /* Extension of the internal buffer is required.
	 */

        if (r->allocated == 0) {
	    r->allocated = toWrite + INCREMENT;
	    r->buf       = UCHARP (ckalloc((unsigned) r->allocated));
	} else {
	    r->allocated += toWrite + INCREMENT;
	    r->buf        = UCHARP (ckrealloc((char*) r->buf,
		    (unsigned) r->allocated));
	}
    }

    /* now copy data */
    memcpy(r->buf + r->used, buf, (size_t) toWrite);
    r->used += toWrite;
}
