#ifndef PANIVIEW_PRECOMP_H
#define PANIVIEW_PRECOMP_H

#ifndef _UNICODE
#define _UNICODE
#endif  /* _UNICODE */

#ifndef UNICODE
#define UNICODE
#endif  /* UNICODE */

#include <assert.h>

/* fix clang linter jokes
 *
 * I don't know WHY actually it swears about unknown typenames `interface` and
 * `BOOL` in sholbjidl.h and d2d1*.h files. Therefore it defined and it pops a
 * warning about redefinition.
 *
 * __OBJC__ macro huh?
 * */
#ifdef __OBJC__
#undef __OBJC__
#endif

#ifdef __OBJC_BOOL
#undef __OBJC_BOOL
#endif

#ifdef __objc_INCLUDE_GNU
#undef __objc_INCLUDE_GNU
#endif

#ifdef _NO_BOOL_TYPEDEF
#undef _NO_BOOL_TYPEDEF
#endif

#include <math.h>

/* WinAPI headers */
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <richedit.h>

#include <shlwapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <strsafe.h>
#include <pathcch.h>

/* Direct2D headers */
#include "d2dwrapper.h"
#include <wincodec.h>

#endif  /* PANIVIEW_PRECOMP_H */
