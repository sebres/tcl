/*
 * tclUtfData.c --
 *
 *	Declarations of Unicode character information tables.  This file is
 *	automatically generated by the tools/uniParse.tcl script.  Do not
 *	modify this file by hand.
 *
 * Copyright (c) 1998 by Scriptics Corporation.
 * All rights reserved.
 *
 * RCS: @(#) $Id: tclUniData.c,v 1.1.2.2 1998/11/11 04:54:21 stanton Exp $
 */

/*
 * A 16-bit Unicode character is split into two parts in order to index
 * into the following tables.  The lower OFFSET_BITS comprise an offset
 * into a page of characters.  The upper bits comprise the page number.
 */

#define OFFSET_BITS 6

/*
 * The pageMap is indexed by page number and returns an alternate page number
 * that identifies a unique page of characters.  Many Unicode characters map
 * to the same alternate page number.
 */

static char pageMap[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 
    20, 21, 22, 23, 24, 25, 26, 27, 28, 28, 28, 28, 28, 28, 28, 28, 29, 
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 
    28, 28, 47, 48, 49, 50, 51, 52, 53, 28, 28, 28, 54, 55, 56, 57, 58, 
    59, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 60, 60, 
    61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 75, 75, 
    76, 77, 78, 28, 28, 79, 80, 81, 82, 83, 83, 84, 85, 86, 85, 28, 28, 
    87, 88, 89, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 90, 91, 92, 93, 94, 56, 95, 28, 96, 97, 98, 99, 83, 100, 83, 
    101, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 102, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 
    56, 56, 56, 56, 56, 56, 56, 56, 56, 103, 28, 104, 104, 104, 104, 104, 
    104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 
    104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 105, 
    105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 
    105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 
    105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 
    105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 
    105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 
    105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 
    105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 
    105, 56, 56, 56, 56, 106, 28, 28, 28, 107, 108, 109, 110, 56, 56, 56, 
    56, 111, 112, 113, 114, 115, 116, 56, 117, 118, 119, 120, 121
};

/*
 * The groupMap is indexed by combining the alternate page number with
 * the page offset and returns a group number that identifies a unique
 * set of character attributes.
 */

static char groupMap[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 3, 3, 4, 3, 3, 3, 5, 6, 3, 7, 3, 8, 
    3, 3, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 3, 3, 7, 7, 7, 3, 3, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 5, 3, 6, 11, 12, 11, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 5, 7, 6, 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 4, 4, 
    4, 14, 14, 11, 14, 15, 16, 7, 8, 14, 11, 14, 7, 17, 17, 11, 15, 14, 
    3, 11, 17, 15, 18, 17, 17, 17, 3, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 7, 10, 10, 
    10, 10, 10, 10, 10, 15, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 7, 13, 13, 13, 13, 
    13, 13, 13, 19, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 22, 23, 20, 21, 20, 21, 20, 21, 15, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 15, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 24, 20, 21, 20, 21, 20, 21, 25, 15, 26, 20, 21, 
    20, 21, 27, 20, 21, 28, 28, 20, 21, 15, 29, 30, 31, 20, 21, 28, 32, 
    15, 33, 34, 20, 21, 15, 15, 33, 35, 15, 36, 20, 21, 20, 21, 20, 21, 
    37, 20, 21, 38, 39, 15, 20, 21, 38, 20, 21, 40, 40, 20, 21, 20, 21, 
    41, 20, 21, 15, 39, 20, 21, 39, 39, 39, 39, 39, 39, 42, 43, 44, 42, 
    43, 44, 42, 43, 44, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 45, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 15, 42, 43, 44, 20, 21, 0, 0, 0, 0, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 15, 15, 15, 46, 47, 15, 48, 48, 15, 49, 15, 50, 15, 15, 15, 15, 
    48, 15, 15, 51, 15, 15, 15, 15, 52, 53, 15, 15, 15, 15, 15, 53, 15, 
    15, 54, 15, 15, 55, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
    15, 56, 15, 15, 15, 15, 56, 15, 57, 57, 15, 15, 15, 15, 15, 15, 58, 
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 0, 0, 0, 0, 0, 0, 0, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 11, 11, 59, 59, 59, 59, 59, 59, 59, 11, 11, 11, 11, 11, 11, 
    11, 11, 11, 11, 11, 11, 11, 11, 59, 59, 11, 11, 11, 11, 11, 11, 11, 
    11, 11, 11, 11, 11, 11, 0, 59, 59, 59, 59, 59, 11, 11, 11, 11, 11, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 
    60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 60, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 3, 3, 0, 0, 0, 0, 59, 0, 0, 0, 3, 0, 0, 0, 0, 0, 11, 11, 61, 
    3, 62, 62, 62, 0, 63, 0, 64, 64, 15, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 65, 66, 66, 66, 15, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 13, 67, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    68, 69, 69, 0, 70, 71, 37, 37, 37, 72, 73, 0, 0, 0, 37, 0, 37, 0, 37, 
    0, 37, 0, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 74, 
    75, 45, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 76, 76, 76, 76, 
    76, 76, 76, 76, 76, 76, 76, 76, 0, 76, 76, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 10, 10, 10, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 13, 0, 75, 75, 75, 75, 75, 75, 75, 75, 75, 
    75, 75, 75, 0, 75, 75, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 14, 60, 60, 60, 60, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 39, 20, 
    21, 20, 21, 0, 0, 20, 21, 0, 0, 20, 21, 0, 0, 0, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 0, 0, 20, 21, 20, 21, 20, 21, 20, 21, 0, 0, 
    20, 21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 77, 77, 77, 77, 77, 77, 77, 77, 
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 0, 0, 59, 3, 3, 
    3, 3, 3, 3, 0, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 
    78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 
    78, 78, 78, 78, 78, 78, 78, 78, 15, 0, 3, 0, 0, 0, 0, 0, 0, 0, 60, 
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 0, 
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 
    60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 3, 60, 3, 60, 60, 3, 60, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    0, 0, 0, 0, 0, 39, 39, 39, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 3, 0, 0, 0, 3, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 
    0, 0, 0, 0, 59, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 60, 60, 60, 
    60, 60, 60, 60, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 9, 9, 
    9, 9, 9, 9, 9, 9, 9, 3, 3, 3, 3, 0, 0, 60, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 0, 39, 39, 
    39, 39, 39, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 0, 39, 39, 39, 39, 3, 39, 60, 60, 60, 60, 60, 60, 60, 79, 79, 
    60, 60, 60, 60, 60, 60, 59, 59, 60, 60, 14, 60, 60, 60, 60, 0, 0, 9, 
    9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 60, 80, 0, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 0, 60, 39, 
    80, 80, 80, 60, 60, 60, 60, 60, 60, 60, 60, 80, 80, 80, 80, 60, 0, 
    0, 14, 60, 60, 60, 60, 0, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 60, 60, 3, 3, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 3, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 80, 80, 0, 39, 39, 39, 39, 39, 39, 
    39, 39, 0, 0, 39, 39, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 39, 39, 39, 39, 
    39, 39, 39, 0, 39, 0, 0, 0, 39, 39, 39, 39, 0, 0, 60, 0, 80, 80, 80, 
    60, 60, 60, 60, 0, 0, 80, 80, 0, 0, 80, 80, 60, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 80, 0, 0, 0, 0, 39, 39, 0, 39, 39, 39, 60, 60, 0, 0, 9, 9, 9, 
    9, 9, 9, 9, 9, 9, 9, 39, 39, 4, 4, 17, 17, 17, 17, 17, 17, 14, 0, 0, 
    0, 0, 0, 0, 0, 60, 0, 0, 39, 39, 39, 39, 39, 39, 0, 0, 0, 0, 39, 39, 
    0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 0, 39, 39, 39, 39, 39, 39, 39, 0, 39, 39, 0, 
    39, 39, 0, 39, 39, 0, 0, 60, 0, 80, 80, 80, 60, 60, 0, 0, 0, 0, 60, 
    60, 0, 0, 60, 60, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 39, 39, 39, 
    39, 0, 39, 0, 0, 0, 0, 0, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 60, 60, 
    39, 39, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 60, 80, 0, 39, 
    39, 39, 39, 39, 39, 39, 0, 39, 0, 39, 39, 39, 0, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    0, 39, 39, 39, 39, 39, 39, 39, 0, 39, 39, 0, 39, 39, 39, 39, 39, 0, 
    0, 60, 39, 80, 80, 80, 60, 60, 60, 60, 60, 0, 60, 60, 80, 0, 80, 80, 
    60, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 39, 0, 0, 
    0, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 60, 80, 80, 0, 39, 39, 39, 39, 39, 39, 39, 39, 
    0, 0, 39, 39, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 39, 39, 39, 39, 39, 39, 
    39, 0, 39, 39, 0, 0, 39, 39, 39, 39, 0, 0, 60, 39, 80, 60, 80, 60, 
    60, 60, 0, 0, 0, 80, 80, 0, 0, 80, 80, 60, 0, 0, 0, 0, 0, 0, 0, 0, 
    60, 80, 0, 0, 0, 0, 39, 39, 0, 39, 39, 39, 0, 0, 0, 0, 9, 9, 9, 9, 
    9, 9, 9, 9, 9, 9, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 60, 80, 0, 39, 39, 39, 39, 39, 39, 0, 0, 0, 39, 39, 39, 0, 39, 
    39, 39, 39, 0, 0, 0, 39, 39, 0, 39, 0, 39, 39, 0, 0, 0, 39, 39, 0, 
    0, 0, 39, 39, 39, 0, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 0, 39, 39, 
    39, 0, 0, 0, 0, 80, 80, 60, 80, 80, 0, 0, 0, 80, 80, 80, 0, 80, 80, 
    80, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 17, 17, 17, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 80, 80, 0, 39, 39, 39, 39, 39, 39, 39, 
    39, 0, 39, 39, 39, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 0, 39, 39, 39, 39, 39, 0, 0, 0, 0, 60, 60, 60, 
    80, 80, 80, 80, 0, 60, 60, 60, 0, 60, 60, 60, 60, 0, 0, 0, 0, 0, 0, 
    0, 60, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 39, 39, 0, 0, 0, 0, 9, 9, 9, 
    9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 80, 80, 0, 39, 39, 39, 39, 39, 39, 39, 39, 0, 39, 39, 39, 0, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    0, 39, 39, 39, 39, 39, 0, 0, 0, 0, 80, 60, 80, 80, 80, 80, 80, 0, 60, 
    80, 80, 0, 80, 80, 60, 60, 0, 0, 0, 0, 0, 0, 0, 80, 80, 0, 0, 0, 0, 
    0, 0, 0, 39, 0, 39, 39, 0, 0, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 80, 0, 39, 39, 
    39, 39, 39, 39, 39, 39, 0, 39, 39, 39, 0, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 
    0, 0, 0, 80, 80, 80, 60, 60, 60, 0, 0, 80, 80, 80, 0, 80, 80, 80, 60, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, 39, 39, 0, 0, 
    0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 3, 
    39, 60, 39, 39, 60, 60, 60, 60, 60, 60, 60, 0, 0, 0, 0, 4, 39, 39, 
    39, 39, 39, 39, 59, 60, 60, 60, 60, 60, 60, 60, 60, 14, 9, 9, 9, 9, 
    9, 9, 9, 9, 9, 9, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 39, 
    39, 0, 39, 0, 0, 39, 39, 0, 39, 0, 0, 39, 0, 0, 0, 0, 0, 0, 39, 39, 
    39, 39, 0, 39, 39, 39, 39, 39, 39, 39, 0, 39, 39, 39, 0, 39, 0, 39, 
    0, 0, 39, 39, 0, 39, 39, 3, 39, 60, 39, 39, 60, 60, 60, 60, 60, 60, 
    0, 60, 60, 39, 0, 0, 39, 39, 39, 39, 39, 0, 59, 0, 60, 60, 60, 60, 
    60, 60, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 39, 39, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 14, 14, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
    3, 3, 3, 3, 3, 3, 14, 14, 14, 14, 14, 60, 60, 14, 14, 14, 14, 14, 14, 
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    14, 60, 14, 60, 14, 60, 5, 6, 5, 6, 5, 6, 39, 39, 39, 39, 39, 39, 39, 
    39, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 0, 0, 0, 0, 0, 0, 0, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 
    60, 60, 60, 80, 60, 60, 60, 60, 60, 3, 60, 60, 14, 14, 14, 14, 0, 0, 
    0, 0, 60, 60, 60, 60, 60, 60, 0, 60, 0, 60, 60, 60, 60, 60, 60, 60, 
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 0, 0, 0, 60, 
    60, 60, 60, 60, 60, 60, 0, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 
    77, 77, 77, 77, 77, 77, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
    15, 0, 0, 0, 0, 3, 0, 0, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 0, 0, 0, 0, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    0, 0, 0, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 0, 0, 0, 0, 0, 0, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 15, 15, 15, 15, 15, 81, 0, 0, 0, 0, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 0, 0, 0, 0, 0, 0, 82, 82, 82, 82, 82, 82, 82, 82, 83, 83, 
    83, 83, 83, 83, 83, 83, 82, 82, 82, 82, 82, 82, 0, 0, 83, 83, 83, 83, 
    83, 83, 0, 0, 82, 82, 82, 82, 82, 82, 82, 82, 83, 83, 83, 83, 83, 83, 
    83, 83, 82, 82, 82, 82, 82, 82, 82, 82, 83, 83, 83, 83, 83, 83, 83, 
    83, 82, 82, 82, 82, 82, 82, 0, 0, 83, 83, 83, 83, 83, 83, 0, 0, 15, 
    82, 15, 82, 15, 82, 15, 82, 0, 83, 0, 83, 0, 83, 0, 83, 82, 82, 82, 
    82, 82, 82, 82, 82, 83, 83, 83, 83, 83, 83, 83, 83, 84, 84, 85, 85, 
    85, 85, 86, 86, 87, 87, 88, 88, 89, 89, 0, 0, 82, 82, 82, 82, 82, 82, 
    82, 82, 83, 83, 83, 83, 83, 83, 83, 83, 82, 82, 82, 82, 82, 82, 82, 
    82, 83, 83, 83, 83, 83, 83, 83, 83, 82, 82, 82, 82, 82, 82, 82, 82, 
    83, 83, 83, 83, 83, 83, 83, 83, 82, 82, 15, 90, 15, 0, 15, 15, 83, 
    83, 91, 91, 92, 11, 37, 11, 11, 11, 15, 90, 15, 0, 15, 15, 93, 93, 
    93, 93, 92, 11, 11, 11, 82, 82, 15, 15, 0, 0, 15, 15, 83, 83, 94, 94, 
    0, 11, 11, 11, 82, 82, 15, 15, 15, 95, 15, 15, 83, 83, 96, 96, 97, 
    11, 11, 11, 0, 0, 15, 90, 15, 0, 15, 15, 98, 98, 99, 99, 92, 11, 11, 
    0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 100, 100, 100, 100, 8, 8, 8, 
    8, 8, 8, 3, 3, 16, 18, 5, 16, 16, 18, 5, 16, 3, 3, 3, 3, 3, 3, 3, 3, 
    101, 102, 100, 100, 100, 100, 100, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 16, 
    18, 3, 3, 3, 3, 12, 12, 3, 3, 3, 7, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 100, 100, 100, 100, 100, 100, 17, 0, 0, 0, 17, 17, 17, 17, 
    17, 17, 7, 7, 7, 5, 6, 15, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    7, 7, 7, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 79, 79, 79, 
    79, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 37, 14, 14, 14, 14, 37, 14, 14, 
    15, 37, 37, 37, 15, 15, 37, 37, 37, 15, 14, 37, 14, 14, 37, 37, 37, 
    37, 37, 37, 14, 14, 14, 14, 14, 14, 37, 14, 37, 14, 37, 14, 37, 37, 
    37, 37, 15, 15, 37, 37, 14, 37, 15, 39, 39, 39, 39, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 103, 103, 103, 103, 
    103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 104, 104, 
    104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 
    105, 105, 105, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 7, 7, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 7, 14, 7, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 0, 14, 14, 14, 14, 14, 14, 7, 
    7, 7, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 7, 7, 14, 14, 14, 14, 14, 14, 14, 5, 6, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 106, 106, 106, 
    106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 
    106, 106, 106, 106, 106, 106, 106, 106, 106, 107, 107, 107, 107, 107, 
    107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 
    107, 107, 107, 107, 107, 107, 107, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 0, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    0, 14, 14, 14, 14, 0, 14, 14, 14, 14, 0, 0, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 0, 14, 0, 14, 14, 14, 14, 0, 0, 0, 14, 
    0, 14, 14, 14, 14, 14, 14, 14, 0, 0, 14, 14, 14, 14, 14, 14, 14, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 14, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 2, 3, 3, 
    3, 14, 59, 3, 105, 5, 6, 5, 6, 5, 6, 5, 6, 5, 6, 14, 14, 5, 6, 5, 6, 
    5, 6, 5, 6, 8, 5, 6, 6, 14, 105, 105, 105, 105, 105, 105, 105, 105, 
    105, 60, 60, 60, 60, 60, 60, 8, 59, 59, 59, 59, 59, 14, 14, 0, 0, 0, 
    0, 0, 0, 0, 14, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 0, 0, 0, 0, 60, 60, 59, 59, 59, 59, 0, 0, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 3, 59, 59, 59, 0, 0, 0, 0, 0, 0, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 
    0, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 14, 
    14, 17, 17, 17, 17, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 
    0, 0, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 0, 0, 0, 14, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 
    0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 108, 108, 108, 108, 108, 
    108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 
    108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 
    108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 
    108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 
    108, 108, 108, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 15, 15, 15, 15, 15, 15, 15, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 15, 15, 15, 15, 15, 0, 0, 0, 0, 0, 0, 60, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 7, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 0, 39, 39, 39, 39, 39, 0, 39, 0, 39, 39, 0, 39, 
    39, 0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 0, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 60, 60, 60, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 8, 8, 
    12, 12, 5, 6, 5, 6, 5, 6, 5, 6, 5, 6, 5, 6, 5, 6, 5, 6, 0, 0, 0, 0, 
    3, 3, 3, 3, 12, 12, 12, 3, 3, 3, 0, 3, 3, 3, 3, 8, 5, 6, 5, 6, 5, 6, 
    3, 3, 3, 7, 8, 7, 7, 7, 0, 3, 4, 3, 3, 0, 0, 0, 0, 39, 39, 39, 0, 39, 
    0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 0, 0, 100, 0, 3, 3, 3, 4, 3, 3, 3, 5, 6, 3, 7, 3, 8, 
    3, 3, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 3, 3, 7, 7, 7, 3, 3, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 5, 3, 6, 11, 12, 11, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 5, 7, 6, 7, 0, 0, 3, 5, 6, 3, 3, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 59, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 59, 
    59, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 0, 0, 0, 
    39, 39, 39, 39, 39, 39, 0, 0, 39, 39, 39, 39, 39, 39, 0, 0, 39, 39, 
    39, 39, 39, 39, 0, 0, 39, 39, 39, 0, 0, 0, 4, 4, 7, 11, 14, 4, 4, 0, 
    7, 7, 7, 7, 7, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 
    0, 0
};

/*
 * Each group represents a unique set of character attributes.  The attributes
 * are encoded into a 32-bit value as follows:
 *
 * Bits 0-4	Character category: see the constants listed below.
 *
 * Bits 5-7	Case delta type: 000 = identity
 *				 010 = add delta for lower
 *				 011 = add delta for lower, add 1 for title
 *				 100 = sutract delta for title/upper
 *				 101 = sub delta for upper, sub 1 for title
 *				 110 = sub delta for upper, add delta for lower
 *
 * Bits 8-21	Reserved for future use.
 *
 * Bits 22-31	Case delta: delta for case conversions.  This should be the
 *			    highest field so we can easily sign extend.
 */

static int groups[] = {
    0, 15, 12, 25, 27, 21, 22, 26, 20, 9, 134217793, 28, 19, 134217858, 
    29, 2, 23, 11, 24, -507510654, 4194369, 4194434, -834666431, 973078658, 
    -507510719, 1258291330, 880803905, 864026689, 859832385, 331350081, 
    847249473, 851443777, 868220993, 884998209, 876609601, 893386817, 
    897581121, 1, 914358337, 5, 910164033, 918552641, 8388705, 4194499, 
    8388770, 331350146, 880803970, 864026754, 859832450, 847249538, 
    851443842, 868221058, 876609666, 884998274, 893386882, 897581186, 
    914358402, 910164098, 918552706, 4, 6, 159383617, 155189313, 268435521, 
    264241217, 159383682, 155189378, 130023554, 268435586, 264241282, 
    260046978, 239075458, 197132418, 226492546, 360710274, 335544450, 
    335544385, 201326657, 201326722, 7, 8, 247464066, -33554302, -33554367, 
    -310378366, -360710014, -419430270, -536870782, -469761918, -528482174, 
    -37748606, -310378431, -37748671, -360710079, -419430335, -29359998, 
    -469761983, -29360063, -536870847, -528482239, 16, 13, 14, 67108938, 
    67109002, 10, 109051997, 109052061, 18, 17
};

/*
 * The following constants are used to determine the category of a
 * Unicode character.
 */

#define UNICODE_CATEGORY_MASK 0X1F

enum {
    UNASSIGNED,
    UPPERCASE_LETTER,
    LOWERCASE_LETTER,
    TITLECASE_LETTER,
    MODIFIER_LETTER,
    OTHER_LETTER,
    NON_SPACING_MARK,
    ENCLOSING_MARK,
    COMBINING_SPACING_MARK,
    DECIMAL_DIGIT_NUMBER,
    LETTER_NUMBER,
    OTHER_NUMBER,
    SPACE_SEPARATOR,
    LINE_SEPARATOR,
    PARAGRAPH_SEPARATOR,
    CONTROL,
    FORMAT,
    PRIVATE_USE,
    SURROGATE,
    CONNECTOR_PUNCTUATION,
    DASH_PUNCTUATION,
    OPEN_PUNCTUATION,
    CLOSE_PUNCTUATION,
    INITIAL_QUOTE_PUNCTUATION,
    FINAL_QUOTE_PUNCTUATION,
    OTHER_PUNCTUATION,
    MATH_SYMBOL,
    CURRENCY_SYMBOL,
    MODIFIER_SYMBOL,
    OTHER_SYMBOL
};

/*
 * The following macros extract the fields of the character info.  The
 * GetDelta() macro is complicated because we can't rely on the C compiler
 * to do sign extension on right shifts.
 */

#define GetCaseType(info) (((info) & 0xE0) >> 5)
#define GetCategory(info) ((info) & 0x1F)
#define GetDelta(infO) (((info) > 0) ? ((info) >> 22) : (~(~((info)) >> 22)))

/*
 * This macro extracts the information about a character from the
 * Unicode character tables.
 */

#define GetUniCharInfo(ch) (groups[(int)groupMap[(int)((pageMap[(((int)(ch)) & 0xffff) >> OFFSET_BITS] << OFFSET_BITS) | ((ch) & ((1 << OFFSET_BITS)-1)))]])

