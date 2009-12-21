/*
 * tclPlatDecls.h --
 *
 *	Declarations of platform specific Tcl APIs.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * All rights reserved.
 *
 * RCS: @(#) $Id: tclPlatDecls.h,v 1.34 2009/12/21 23:25:40 nijtmans Exp $
 */

#ifndef _TCLPLATDECLS
#define _TCLPLATDECLS

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tcl
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_TCL_STUBS
#      define TCL_STORAGE_CLASS
#   else
#      define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif

/*
 *  Pull in the typedef of TCHAR for windows.
 */
#if defined(__WIN32__) && !defined(_TCHAR_DEFINED)
#   include <tchar.h>
#   ifndef _TCHAR_DEFINED
	/* Borland seems to forget to set this. */
        typedef _TCHAR TCHAR;
#	define _TCHAR_DEFINED
#   endif
#   if defined(_MSC_VER) && defined(__STDC__)
	/* MSVC++ misses this. */
	typedef _TCHAR TCHAR;
#   endif
#elif defined(__CYGWIN__)
    typedef char TCHAR;
#endif

/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */

#ifdef __WIN32__ /* WIN */
#ifndef Tcl_WinUtfToTChar_TCL_DECLARED
#define Tcl_WinUtfToTChar_TCL_DECLARED
/* 0 */
EXTERN TCHAR *		Tcl_WinUtfToTChar (const char * str, int len,
				Tcl_DString * dsPtr);
#endif
#ifndef Tcl_WinTCharToUtf_TCL_DECLARED
#define Tcl_WinTCharToUtf_TCL_DECLARED
/* 1 */
EXTERN char *		Tcl_WinTCharToUtf (const TCHAR * str, int len,
				Tcl_DString * dsPtr);
#endif
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
#ifndef Tcl_MacOSXOpenBundleResources_TCL_DECLARED
#define Tcl_MacOSXOpenBundleResources_TCL_DECLARED
/* 0 */
EXTERN int		Tcl_MacOSXOpenBundleResources (Tcl_Interp * interp,
				const char * bundleName, int hasResourceFile,
				int maxPathLen, char * libraryPath);
#endif
#ifndef Tcl_MacOSXOpenVersionedBundleResources_TCL_DECLARED
#define Tcl_MacOSXOpenVersionedBundleResources_TCL_DECLARED
/* 1 */
EXTERN int		Tcl_MacOSXOpenVersionedBundleResources (
				Tcl_Interp * interp, const char * bundleName,
				const char * bundleVersion,
				int hasResourceFile, int maxPathLen,
				char * libraryPath);
#endif
#endif /* MACOSX */

typedef struct TclPlatStubs {
    int magic;
    const struct TclPlatStubHooks *hooks;

#ifdef __WIN32__ /* WIN */
    TCHAR * (*tcl_WinUtfToTChar) (const char * str, int len, Tcl_DString * dsPtr); /* 0 */
    char * (*tcl_WinTCharToUtf) (const TCHAR * str, int len, Tcl_DString * dsPtr); /* 1 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
    int (*tcl_MacOSXOpenBundleResources) (Tcl_Interp * interp, const char * bundleName, int hasResourceFile, int maxPathLen, char * libraryPath); /* 0 */
    int (*tcl_MacOSXOpenVersionedBundleResources) (Tcl_Interp * interp, const char * bundleName, const char * bundleVersion, int hasResourceFile, int maxPathLen, char * libraryPath); /* 1 */
#endif /* MACOSX */
} TclPlatStubs;

#if defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS)
extern const TclPlatStubs *tclPlatStubsPtr;
#endif /* defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS) */

#if defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS)

/*
 * Inline function declarations:
 */

#ifdef __WIN32__ /* WIN */
#ifndef Tcl_WinUtfToTChar
#define Tcl_WinUtfToTChar \
	(tclPlatStubsPtr->tcl_WinUtfToTChar) /* 0 */
#endif
#ifndef Tcl_WinTCharToUtf
#define Tcl_WinTCharToUtf \
	(tclPlatStubsPtr->tcl_WinTCharToUtf) /* 1 */
#endif
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
#ifndef Tcl_MacOSXOpenBundleResources
#define Tcl_MacOSXOpenBundleResources \
	(tclPlatStubsPtr->tcl_MacOSXOpenBundleResources) /* 0 */
#endif
#ifndef Tcl_MacOSXOpenVersionedBundleResources
#define Tcl_MacOSXOpenVersionedBundleResources \
	(tclPlatStubsPtr->tcl_MacOSXOpenVersionedBundleResources) /* 1 */
#endif
#endif /* MACOSX */

#endif /* defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS) */

/* !END!: Do not edit above this line. */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TCLPLATDECLS */


