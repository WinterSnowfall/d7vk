#pragma once

#ifndef _MSC_VER
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0A00
#endif

#include <stdint.h>
#include <d3d.h>

// Declare __uuidof for DDRAW/D3D7 interfaces
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IDirectDraw,             0x6c14db80,0xa733,0x11ce,0xa5,0x21,0x00,0x20,0xaf,0x0b,0xe5,0x60);
__CRT_UUID_DECL(IDirectDraw7,            0x15e65ec0,0x3b9c,0x11d2,0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b);
__CRT_UUID_DECL(IDirectDrawSurface,      0x6c14db81,0xa733,0x11ce,0xa5,0x21,0x00,0x20,0xaf,0x0b,0xe5,0x60);
__CRT_UUID_DECL(IDirectDrawSurface2,     0x57805885,0x6eec,0x11cf,0x94,0x41,0xa8,0x23,0x03,0xc1,0x0e,0x27);
__CRT_UUID_DECL(IDirectDrawSurface3,     0xda044e00,0x69b2,0x11d0,0xa1,0xd5,0x00,0xaa,0x00,0xb8,0xdf,0xbb);
__CRT_UUID_DECL(IDirectDrawSurface4,     0x0b2b8630,0xad35,0x11d0,0x8e,0xa6,0x00,0x60,0x97,0x97,0xea,0x5b);
__CRT_UUID_DECL(IDirectDrawSurface7,     0x06675a80,0x3b9b,0x11d2,0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b);
__CRT_UUID_DECL(IDirectDrawGammaControl, 0x69c11c3e,0xb46b,0x11d1,0xad,0x7a,0x00,0xc0,0x4f,0xc2,0x9b,0x4e);
__CRT_UUID_DECL(IDirectDrawColorControl, 0x4b9f0ee0,0x0d7e,0x11d0,0x9b,0x06,0x00,0xa0,0xc9,0x03,0xa3,0xb8);
__CRT_UUID_DECL(IDirect3D7,              0xf5049e77,0x4861,0x11d2,0xa4,0x07,0x00,0xa0,0xc9,0x06,0x29,0xa8);
__CRT_UUID_DECL(IDirect3DDevice7,        0xf5049e79,0x4861,0x11d2,0xa4,0x07,0x00,0xa0,0xc9,0x06,0x29,0xa8);
__CRT_UUID_DECL(IDirect3DVertexBuffer7,  0xf5049e7d,0x4861,0x11d2,0xa4,0x07,0x00,0xa0,0xc9,0x06,0x29,0xa8);
#elif defined(_MSC_VER)
interface DECLSPEC_UUID("6C14DB80-A733-11CE-A521-0020AF0BE560") IDirectDraw;
interface DECLSPEC_UUID("15E65EC0-3B9C-11D2-B92F-00609797EA5B") IDirectDraw7;
interface DECLSPEC_UUID("6C14DB81-A733-11CE-A521-0020AF0BE560") IDirectDrawSurface;
interface DECLSPEC_UUID("57805885-6EEC-11CF-9441-A82303C10E27") IDirectDrawSurface2;
interface DECLSPEC_UUID("DA044E00-69B2-11D0-A1D5-00AA00B8DFBB") IDirectDrawSurface3;
interface DECLSPEC_UUID("0B2B8630-AD35-11D0-8EA6-00609797EA5B") IDirectDrawSurface4;
interface DECLSPEC_UUID("06675A80-3B9B-11D2-B92F-00609797EA5B") IDirectDrawSurface7;
interface DECLSPEC_UUID("69C11C3E-B46B-11D1-AD7A-00C04FC29B4E") IDirectDrawGammaControl;
interface DECLSPEC_UUID("4B9F0EE0-0D7E-11D0-9B06-00A0C903A3B8") IDirectDrawColorControl;
interface DECLSPEC_UUID("F5049E77-4861-11D2-A407-00A0C90629A8") IDirect3D7;
interface DECLSPEC_UUID("F5049E79-4861-11D2-A407-00A0C90629A8") IDirect3DDevice7;
interface DECLSPEC_UUID("F5049E7D-4861-11D2-A407-00A0C90629A8") IDirect3DVertexBuffer7;
#endif


// Undefine D3D7 macros
#undef DIRECT3D_VERSION
#undef D3D_SDK_VERSION

#undef D3DCS_ALL            // parentheses added in D3D9
#undef D3DFVF_POSITION_MASK // changed from 0x00E to 0x400E in D3D9
#undef D3DFVF_RESERVED2     // reduced from 4 to 2 in DX9

#undef D3DSP_REGNUM_MASK    // changed from 0x00000FFF to 0x000007FF in D3D9

#if defined(__MINGW32__) || defined(__GNUC__)

// Avoid redundant definitions (add D3D*_DEFINED macros here)
#define D3DRECT_DEFINED
#define D3DMATRIX_DEFINED

// Temporarily override __CRT_UUID_DECL to allow usage in d3d9 namespace
#pragma push_macro("__CRT_UUID_DECL")
#ifdef __CRT_UUID_DECL
#undef __CRT_UUID_DECL
#endif

#ifdef __MINGW32__
#define __CRT_UUID_DECL(type,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)                                                                               \
}                                                                                                                                           \
  extern "C++" template<> struct __mingw_uuidof_s<d3d9::type> { static constexpr IID __uuid_inst = {l,w1,w2, {b1,b2,b3,b4,b5,b6,b7,b8}}; }; \
  extern "C++" template<> constexpr const GUID &__mingw_uuidof<d3d9::type>() { return __mingw_uuidof_s<d3d9::type>::__uuid_inst; }          \
  extern "C++" template<> constexpr const GUID &__mingw_uuidof<d3d9::type*>() { return __mingw_uuidof_s<d3d9::type>::__uuid_inst; }         \
namespace d3d9 {

#elif defined(__GNUC__)
#define __CRT_UUID_DECL(type, a, b, c, d, e, f, g, h, i, j, k)                                                               \
}                                                                                                                            \
  extern "C++" { template <> constexpr GUID __uuidof_helper<d3d9::type>() { return GUID{a,b,c,{d,e,f,g,h,i,j,k}}; } }        \
  extern "C++" { template <> constexpr GUID __uuidof_helper<d3d9::type*>() { return __uuidof_helper<d3d9::type>(); } }       \
  extern "C++" { template <> constexpr GUID __uuidof_helper<const d3d9::type*>() { return __uuidof_helper<d3d9::type>(); } } \
  extern "C++" { template <> constexpr GUID __uuidof_helper<d3d9::type&>() { return __uuidof_helper<d3d9::type>(); } }       \
  extern "C++" { template <> constexpr GUID __uuidof_helper<const d3d9::type&>() { return __uuidof_helper<d3d9::type>(); } } \
namespace d3d9 {
#endif

#endif // defined(__MINGW32__) || defined(__GNUC__)


/**
* \brief Direct3D 9
*
* All D3D9 interfaces are included within
* a namespace, so as not to collide with
* D3D8 interfaces.
*/
namespace d3d9 {
#include <d3d9.h>
}

// Indicates d3d9:: namespace is in-use.
#define DXVK_D3D9_NAMESPACE

#if defined(__MINGW32__) || defined(__GNUC__)
#pragma pop_macro("__CRT_UUID_DECL")
#endif

// for some reason we need to specify __declspec(dllexport) for MinGW
#if defined(__WINE__) || !defined(_WIN32)
  #define DLLEXPORT __attribute__((visibility("default")))
#else
  #define DLLEXPORT
#endif


#include "../util/com/com_guid.h"
#include "../util/com/com_object.h"
#include "../util/com/com_pointer.h"

#include "../util/log/log.h"
#include "../util/log/log_debug.h"

#include "../util/sync/sync_recursive.h"

#include "../util/util_error.h"
#include "../util/util_likely.h"
#include "../util/util_string.h"

