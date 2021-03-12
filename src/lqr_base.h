/* LiquidRescaling Library
 * Copyright (C) 2007-2009 Carlo Baldassi (the "Author") <carlobaldassi@gmail.com>.
 * All Rights Reserved.
 *
 * This library implements the algorithm described in the paper
 * "Seam Carving for Content-Aware Image Resizing"
 * by Shai Avidan and Ariel Shamir
 * which can be found at http://www.faculty.idc.ac.il/arik/imret.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3 dated June, 2007.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>
 */

#ifndef __LQR_BASE_H__
#define __LQR_BASE_H__

#define LQR_MAX_NAME_LENGTH (1024)

#include <stdlib.h> /* calloc,free */
#include <stddef.h> /* NULL */
#include <stdbool.h> /* false,true */

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#if defined(G_OS_WIN32) && ! defined(LQR_DISABLE_DECLSPEC)
#  ifdef LQR_EXPORTS
#    define LQR_PUBLIC __declspec(dllexport)
#  else
#    define LQR_PUBLIC __declspec(dllimport)
#  endif /* LQR_EXPORTS */
#elif defined(__GNUC__) && ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
#  ifndef GCC_HASCLASSVISIBILITY
#    define GCC_HASCLASSVISIBILITY
#  endif /* !GCC_HASCLASSVISIBILITY */
#  ifdef GCC_HASCLASSVISIBILITY
#    define LQR_PUBLIC __attribute__((visibility("default")))
#  else
#    define LQR_PUBLIC
#  endif /* GCC_HASCLASSVISIBILITY */
#else
#  define LQR_PUBLIC
#endif /* G_OS_WIN32 */

#if 0
#define __LQR_DEBUG__
#endif

#if 0
#define __LQR_VERBOSE__
#endif

/**** RETURN VALUES (signals) ****/
enum _LqrRetVal {
    LQR_ERROR,                          /* generic error */
    LQR_OK,                             /* ok */
    LQR_NOMEM,                          /* not enough memory */
    LQR_USRCANCEL                       /* action cancelled by user */
};

typedef enum _LqrRetVal LqrRetVal;

#define LQR_TRY_N_N(assign) if ((assign) == NULL) { return NULL; }
/*
#define LQR_TRY_F_N(assign) if ((assign) == false) { return NULL; }
#define LQR_TRY_N_F(assign) if ((assign) == NULL) { return false; }
#define LQR_TRY_F_F(assign) if ((assign) == false) { return false; }
*/

#ifdef __GNUC__
#define LRQ_DEPRECATED __attribute__ ((deprecated))
#else
#define LQR_DEPRECATED
#endif
/* g_try_new and g_try_new0 replacement */
#define LRQ_CALLOC(type, count) calloc(count, sizeof(type))
/* g_free replacement */
#define LRQ_FREE(ptr) ((ptr) ? (void)0 : free(ptr))

/* generic signal processing macros */
#define LQR_CATCH(expr) do { \
  LqrRetVal ret_val; \
  if ((ret_val = (expr)) != LQR_OK) \
    { \
      return ret_val; \
    } \
} while(0)

/* convert a NULL assignment to an error signal */
#define LQR_CATCH_MEM(expr) do { \
  if ((expr) == NULL) \
    { \
      return LQR_NOMEM; \
    } \
} while(0)

/* convert a boolean value to an error signal */
#define LQR_CATCH_F(expr) do { \
  if ((expr) == false) \
    { \
      return LQR_ERROR; \
    } \
} while(0)

/* legacy */
#ifndef LQR_DISABLE_LEGACY_MACROS
#  define CATCH(expr) LQR_CATCH(expr)
#  define CATCH_MEM(expr) LQR_CATCH_MEM(expr)
#  define CATCH_F(expr) LQR_CATCH_F(expr)
#  define TRY_N_N(expr) LQR_TRY_N_N(expr)
#endif /* LQR_DISABLE_LEGACY_MACROS */

/**** IMAGE DEPTH ****/
enum _LqrColDepth {
    LQR_COLDEPTH_8I,
    LQR_COLDEPTH_16I,
    LQR_COLDEPTH_32F,
    LQR_COLDEPTH_64F
};

typedef enum _LqrColDepth LqrColDepth;

/**** IMAGE BASE TYPES ****/
typedef unsigned char lqr_t_8i;
typedef unsigned short lqr_t_16i;
typedef float lqr_t_32f;
typedef double lqr_t_64f;

/**** RESIZE ORDER ****/
enum _LqrResizeOrder {
    LQR_RES_ORDER_HOR,
    LQR_RES_ORDER_VERT
};

typedef enum _LqrResizeOrder LqrResizeOrder;

/**** IMAGE TYPE ****/
enum _LqrImageType {
    LQR_RGB_IMAGE,
    LQR_RGBA_IMAGE,
    LQR_GREY_IMAGE,
    LQR_GREYA_IMAGE,
    LQR_CMY_IMAGE,
    LQR_CMYK_IMAGE,
    LQR_CMYKA_IMAGE,
    LQR_CUSTOM_IMAGE
};

typedef enum _LqrImageType LqrImageType;

/**** CLASSES DECLARATIONS ****/

struct _LqrCarver;

typedef struct _LqrCarver LqrCarver;

#endif /* __LQR_BASE_H__ */
