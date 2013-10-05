#ifndef DCPLUSPLUS_DCPP_W_FLYLINKDC_H
#define DCPLUSPLUS_DCPP_W_FLYLINKDC_H

#pragma once

#include "version.h"

#ifdef _WIN32

#ifndef _WIN32_WINNT
# ifdef FLYLINKDC_SUPPORT_WIN_2000
#  define _WIN32_WINNT _WIN32_WINNT_WIN2K
# elif FLYLINKDC_SUPPORT_WIN_XP
#  define _WIN32_WINNT _WIN32_WINNT_WINXP
# elif FLYLINKDC_SUPPORT_WIN_VISTA
#  define _WIN32_WINNT _WIN32_WINNT_VISTA
# else // Win7+
#  define _WIN32_WINNT _WIN32_WINNT_WIN7
# endif
#endif // _WIN32_WINNT

#ifndef _WIN32_IE
# ifdef FLYLINKDC_SUPPORT_WIN_2000
#  define _WIN32_IE _WIN32_IE_IE60SP1
# elif FLYLINKDC_SUPPORT_WIN_XP
#  define _WIN32_IE _WIN32_IE_IE80
# endif
#endif // _WIN32_IE

#endif // _WIN32

#endif // DCPLUSPLUS_DCPP_COMPILER_FLYLINKDC_H
