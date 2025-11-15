#include "d3d7_include.h"

#include "ddraw7_interface.h"

namespace dxvk {

  //Logger Logger::s_instance("d3d7.log");

  // TODO: This is somewhat janky, but works for now
  HMODULE GetProxiedDDrawModule() {
    static HMODULE hDDraw = nullptr;

    if (unlikely(hDDraw == nullptr))
      hDDraw = LoadLibraryA("C:\\windows\\system32\\ddraw.dll");

    return hDDraw;
  }

  HRESULT CreateDirectDrawEx(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter) {
    Logger::debug(">>> DirectDrawCreateEx");

    typedef HRESULT (__stdcall *DirectDrawCreateEx_t)(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter);
    static DirectDrawCreateEx_t ProxiedDirectDrawCreateEx = nullptr;

    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDD);

    if (unlikely(iid != __uuidof(IDirectDraw7)))
      return DDERR_INVALIDPARAMS;

    try {
      if (unlikely(ProxiedDirectDrawCreateEx == nullptr)) {
        HMODULE hDDraw = GetProxiedDDrawModule();

        if (unlikely(!hDDraw)) {
          Logger::err("CreateDirectDrawEx: Failed to load proxied ddraw.dll");
          return DDERR_GENERIC;
        }

        ProxiedDirectDrawCreateEx = reinterpret_cast<DirectDrawCreateEx_t>(GetProcAddress(hDDraw, "DirectDrawCreateEx"));

        if (unlikely(ProxiedDirectDrawCreateEx == nullptr)) {
          Logger::err("CreateDirectDrawEx: Failed GetProcAddress");
          return DDERR_GENERIC;
        }
      }

      LPVOID lplpDDProxied = nullptr;
      HRESULT hr = ProxiedDirectDrawCreateEx(lpGUID, &lplpDDProxied, iid, pUnkOuter);

      if (unlikely(FAILED(hr))) {
        Logger::err("CreateDirectDrawEx: Failed call to proxied interface");
        return hr;
      }

      Com<IDirectDraw7> DDraw7IntfProxied = static_cast<IDirectDraw7*>(lplpDDProxied);
      *lplpDD = ref(new DDraw7Interface(std::move(DDraw7IntfProxied)));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

}

extern "C" {

  DLLEXPORT HRESULT __stdcall AcquireDDThreadLock() {
    dxvk::Logger::debug("<<< AcquireDDThreadLock: Proxy");

    typedef HRESULT (__stdcall *AcquireDDThreadLock_t)();
    static AcquireDDThreadLock_t ProxiedAcquireDDThreadLock = nullptr;

    if (unlikely(ProxiedAcquireDDThreadLock == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("AcquireDDThreadLock: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedAcquireDDThreadLock = reinterpret_cast<AcquireDDThreadLock_t>(GetProcAddress(hDDraw, "AcquireDDThreadLock"));

      if (unlikely(ProxiedAcquireDDThreadLock == nullptr)) {
        dxvk::Logger::err("AcquireDDThreadLock: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedAcquireDDThreadLock();

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::warn("AcquireDDThreadLock: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall CompleteCreateSysmemSurface(DWORD arg) {
    dxvk::Logger::warn("!!! CompleteCreateSysmemSurface: Stub");
    return DD_OK;
  }

  DLLEXPORT HRESULT __stdcall D3DParseUnknownCommand(LPVOID lpCmd, LPVOID *lpRetCmd) {
    dxvk::Logger::warn("!!! D3DParseUnknownCommand: Stub");
    return DD_OK;
  }

  DLLEXPORT HRESULT __stdcall DDGetAttachedSurfaceLcl(DWORD arg1, DWORD arg2, DWORD arg3) {
    dxvk::Logger::warn("!!! DDGetAttachedSurfaceLcl: Stub");
    return DD_OK;
  }

  DLLEXPORT HRESULT __stdcall DDInternalLock(DWORD arg1, DWORD arg2) {
    dxvk::Logger::warn("!!! DDInternalLock: Stub");
    return DD_OK;
  }

  DLLEXPORT HRESULT __stdcall DDInternalUnlock(DWORD arg) {
    dxvk::Logger::warn("!!! DDInternalUnlock: Stub");
    return DD_OK;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawCreate(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter) {
    dxvk::Logger::debug("<<< DirectDrawCreate: Proxy");

    typedef HRESULT (__stdcall *DirectDrawCreate_t)(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter);
    static DirectDrawCreate_t ProxiedDirectDrawCreate = nullptr;

    if (unlikely(ProxiedDirectDrawCreate == nullptr)) {
      dxvk::Logger::warn("DirectDrawCreate is a forwarded interface only, expect breakage");

      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawCreate: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawCreate = reinterpret_cast<DirectDrawCreate_t>(GetProcAddress(hDDraw, "DirectDrawCreate"));

      if (unlikely(ProxiedDirectDrawCreate == nullptr)) {
        dxvk::Logger::err("DirectDrawCreate: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedDirectDrawCreate(lpGUID, lplpDD, pUnkOuter);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::err("DirectDrawCreate: Failed call to proxied interface");
    }

    return hr;
  }

  HRESULT WINAPI DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    dxvk::Logger::debug("<<< DirectDrawCreateClipper: Proxy");

    typedef HRESULT (__stdcall *DirectDrawCreateClipper_t)(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter);
    static DirectDrawCreateClipper_t ProxiedDirectDrawCreateClipper = nullptr;

    if (unlikely(ProxiedDirectDrawCreateClipper == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawCreateClipper: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawCreateClipper = reinterpret_cast<DirectDrawCreateClipper_t>(GetProcAddress(hDDraw, "DirectDrawCreateClipper"));

      if (unlikely(!ProxiedDirectDrawCreateClipper)) {
        dxvk::Logger::err("DirectDrawCreateClipper: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedDirectDrawCreateClipper(dwFlags, lplpDDClipper, pUnkOuter);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::err("DirectDrawCreateClipper: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawCreateEx(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter) {
    return dxvk::CreateDirectDrawEx(lpGUID, lplpDD, iid, pUnkOuter);
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateA(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext) {
    dxvk::Logger::debug("<<< DirectDrawEnumerateA: Proxy");

    typedef HRESULT (__stdcall *DirectDrawEnumerateA_t)(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext);
    static DirectDrawEnumerateA_t ProxiedDirectDrawEnumerateA = nullptr;

    if (unlikely(ProxiedDirectDrawEnumerateA == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateA: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawEnumerateA = reinterpret_cast<DirectDrawEnumerateA_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateA"));

      if (unlikely(ProxiedDirectDrawEnumerateA == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateA: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedDirectDrawEnumerateA(lpCallback, lpContext);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::err("DirectDrawEnumerateA: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags) {
    dxvk::Logger::debug("<<< DirectDrawEnumerateExA: Proxy");

    typedef HRESULT (__stdcall *DirectDrawEnumerateA_t)(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags);
    static DirectDrawEnumerateA_t ProxiedDirectDrawEnumerateExA = nullptr;

    if (unlikely(ProxiedDirectDrawEnumerateExA == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExA: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }


      ProxiedDirectDrawEnumerateExA = reinterpret_cast<DirectDrawEnumerateA_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateExA"));

      if (unlikely(ProxiedDirectDrawEnumerateExA == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExA: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedDirectDrawEnumerateExA(lpCallback, lpContext, dwFlags);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::err("DirectDrawEnumerateExA: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateExW(LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags) {
    dxvk::Logger::warn("!!! DirectDrawEnumerateExW: Stub");
    return DD_OK;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateW(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext) {
    dxvk::Logger::warn("!!! DirectDrawEnumerateW: Stub");
    return DD_OK;
  }

  DLLEXPORT HRESULT __stdcall DllCanUnloadNow() {
    dxvk::Logger::warn("!!! DllCanUnloadNow: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
    dxvk::Logger::warn("!!! DllGetClassObject: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall GetDDSurfaceLocal(DWORD arg1, DWORD arg2, DWORD arg3) {
    dxvk::Logger::warn("!!! GetDDSurfaceLocal: Stub");
    return DD_OK;
  }

  // Note: will always return unwrapped surfaces, so we need to account for that
  DLLEXPORT HRESULT __stdcall GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE7 *lpDDS, DWORD arg) {
    dxvk::Logger::debug("<<< GetSurfaceFromDC: Proxy");

    typedef HRESULT (__stdcall *GetSurfaceFromDC_t)(HDC hdc, LPDIRECTDRAWSURFACE7 *lpDDS, DWORD arg);
    static GetSurfaceFromDC_t ProxiedGetSurfaceFromDC = nullptr;

    if (unlikely(ProxiedGetSurfaceFromDC == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("GetSurfaceFromDC: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedGetSurfaceFromDC = reinterpret_cast<GetSurfaceFromDC_t>(GetProcAddress(hDDraw, "GetSurfaceFromDC"));

      if (unlikely(ProxiedGetSurfaceFromDC == nullptr)) {
        dxvk::Logger::err("GetSurfaceFromDC: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedGetSurfaceFromDC(hdc, lpDDS, arg);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::warn("GetSurfaceFromDC: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall RegisterSpecialCase(DWORD arg1, DWORD arg2, DWORD arg3, DWORD arg4) {
    dxvk::Logger::warn("!!! RegisterSpecialCase: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall ReleaseDDThreadLock() {
    dxvk::Logger::debug("<<< ReleaseDDThreadLock: Proxy");

    typedef HRESULT (__stdcall *ReleaseDDThreadLock_t)();
    static ReleaseDDThreadLock_t ProxiedReleaseDDThreadLock = nullptr;

    if (unlikely(ProxiedReleaseDDThreadLock == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("ReleaseDDThreadLock: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedReleaseDDThreadLock = reinterpret_cast<ReleaseDDThreadLock_t>(GetProcAddress(hDDraw, "AcquireDDThreadLock"));

      if (unlikely(ProxiedReleaseDDThreadLock == nullptr)) {
        dxvk::Logger::err("ReleaseDDThreadLock: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedReleaseDDThreadLock();

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::warn("ReleaseDDThreadLock: Failed call to proxied interface");
    }

    return hr;
  }

}
