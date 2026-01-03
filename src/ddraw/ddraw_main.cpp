#include "ddraw_include.h"

#include "ddraw_clipper.h"
#include "ddraw_interface.h"
#include "ddraw7/ddraw7_interface.h"

namespace dxvk {

  //Logger Logger::s_instance("d3d7.log");

  HMODULE GetProxiedDDrawModule() {
    static HMODULE hDDraw = nullptr;

    if (unlikely(hDDraw == nullptr)) {
      // Try to load ddraw_.dll from the current path first
      hDDraw = ::LoadLibraryA("ddraw_.dll");

      if (hDDraw == nullptr) {
        char loadPath[MAX_PATH] = { };
        UINT returnLength = ::GetSystemDirectoryA(loadPath, MAX_PATH);
        if (unlikely(!returnLength))
          return nullptr;

        strcat(loadPath, "\\ddraw.dll");
        hDDraw = ::LoadLibraryA(loadPath);

        if (likely(hDDraw != nullptr))
          Logger::debug(">>> GetProxiedDDrawModule: Loaded ddraw.dll from system path");
      } else {
        Logger::debug(">>> GetProxiedDDrawModule: Loaded ddraw_.dll");
      }
    }

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

        ProxiedDirectDrawCreateEx = reinterpret_cast<DirectDrawCreateEx_t>(::GetProcAddress(hDDraw, "DirectDrawCreateEx"));

        if (unlikely(ProxiedDirectDrawCreateEx == nullptr)) {
          Logger::err("CreateDirectDrawEx: Failed GetProcAddress");
          return DDERR_GENERIC;
        }
      }

      LPVOID lplpDDProxied = nullptr;
      HRESULT hr = ProxiedDirectDrawCreateEx(lpGUID, &lplpDDProxied, iid, pUnkOuter);

      if (unlikely(FAILED(hr))) {
        Logger::warn("CreateDirectDrawEx: Failed call to proxied interface");
        return hr;
      }

      Com<IDirectDraw7> DDraw7IntfProxied = static_cast<IDirectDraw7*>(lplpDDProxied);
      *lplpDD = ref(new DDraw7Interface(std::move(DDraw7IntfProxied)));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return S_OK;
  }

  HRESULT CreateDirectDraw(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter) {
    Logger::debug(">>> DirectDrawCreate");

    typedef HRESULT (__stdcall *DirectDrawCreate_t)(GUID *lpGUID, LPVOID *lplpDD, IUnknown *pUnkOuter);
    static DirectDrawCreate_t ProxiedDirectDrawCreate = nullptr;

    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDD);

    try {
      if (unlikely(ProxiedDirectDrawCreate == nullptr)) {
        HMODULE hDDraw = GetProxiedDDrawModule();

        if (unlikely(!hDDraw)) {
          Logger::err("CreateDirectDrawEx: Failed to load proxied ddraw.dll");
          return DDERR_GENERIC;
        }

        ProxiedDirectDrawCreate = reinterpret_cast<DirectDrawCreate_t>(::GetProcAddress(hDDraw, "DirectDrawCreate"));

        if (unlikely(ProxiedDirectDrawCreate == nullptr)) {
          Logger::err("CreateDirectDraw: Failed GetProcAddress");
          return DDERR_GENERIC;
        }
      }

      LPVOID lplpDDProxied = nullptr;
      HRESULT hr = ProxiedDirectDrawCreate(lpGUID, &lplpDDProxied, pUnkOuter);

      if (unlikely(FAILED(hr))) {
        Logger::warn("CreateDirectDraw: Failed call to proxied interface");
        return hr;
      }

      Com<IDirectDraw> DDrawIntfProxied = static_cast<IDirectDraw*>(lplpDDProxied);
      *lplpDD = ref(new DDrawInterface(std::move(DDrawIntfProxied), nullptr));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return S_OK;
  }

  HRESULT CreateDirectDrawClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    Logger::debug(">>> DirectDrawCreateClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    typedef HRESULT (__stdcall *DirectDrawCreateClipper_t)(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter);
    static DirectDrawCreateClipper_t ProxiedDirectDrawCreateClipper = nullptr;

    if (unlikely(ProxiedDirectDrawCreateClipper == nullptr)) {
      HMODULE hDDraw = GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        Logger::err("DirectDrawCreateClipper: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawCreateClipper = reinterpret_cast<DirectDrawCreateClipper_t>(GetProcAddress(hDDraw, "DirectDrawCreateClipper"));

      if (unlikely(ProxiedDirectDrawCreateClipper == nullptr)) {
        Logger::err("DirectDrawCreateClipper: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    Com<IDirectDrawClipper> lplpDDClipperProxy;
    HRESULT hr = ProxiedDirectDrawCreateClipper(dwFlags, &lplpDDClipperProxy, pUnkOuter);

    if (unlikely(FAILED(hr))) {
      Logger::warn("DirectDrawCreateClipper: Failed call to proxied interface");
      return hr;
    }

    *lplpDDClipper = ref(new DDrawClipper(std::move(lplpDDClipperProxy), nullptr));

    return S_OK;
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
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall D3DParseUnknownCommand(LPVOID lpCmd, LPVOID *lpRetCmd) {
    dxvk::Logger::warn("!!! D3DParseUnknownCommand: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DDGetAttachedSurfaceLcl(DWORD arg1, DWORD arg2, DWORD arg3) {
    dxvk::Logger::warn("!!! DDGetAttachedSurfaceLcl: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DDInternalLock(DWORD arg1, DWORD arg2) {
    dxvk::Logger::warn("!!! DDInternalLock: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DDInternalUnlock(DWORD arg) {
    dxvk::Logger::warn("!!! DDInternalUnlock: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawCreate(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter) {
    return dxvk::CreateDirectDraw(lpGUID, lplpDD, pUnkOuter);
  }

  // Mostly unused, except for Sea Dogs (D3D6)
  DLLEXPORT HRESULT __stdcall DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    return dxvk::CreateDirectDrawClipper(dwFlags, lplpDDClipper, pUnkOuter);
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
      dxvk::Logger::warn("DirectDrawEnumerateA: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags) {
    dxvk::Logger::debug("<<< DirectDrawEnumerateExA: Proxy");

    typedef HRESULT (__stdcall *DirectDrawEnumerateExA_t)(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags);
    static DirectDrawEnumerateExA_t ProxiedDirectDrawEnumerateExA = nullptr;

    if (unlikely(ProxiedDirectDrawEnumerateExA == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExA: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawEnumerateExA = reinterpret_cast<DirectDrawEnumerateExA_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateExA"));

      if (unlikely(ProxiedDirectDrawEnumerateExA == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExA: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedDirectDrawEnumerateExA(lpCallback, lpContext, dwFlags);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::warn("DirectDrawEnumerateExA: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateExW(LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags) {
    dxvk::Logger::debug("<<< DirectDrawEnumerateExW: Proxy");

    typedef HRESULT (__stdcall *DirectDrawEnumerateExW_t)(LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags);
    static DirectDrawEnumerateExW_t ProxiedDirectDrawEnumerateExW = nullptr;

    if (unlikely(ProxiedDirectDrawEnumerateExW == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExW: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawEnumerateExW = reinterpret_cast<DirectDrawEnumerateExW_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateExW"));

      if (unlikely(ProxiedDirectDrawEnumerateExW == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExW: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedDirectDrawEnumerateExW(lpCallback, lpContext, dwFlags);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::warn("DirectDrawEnumerateExW: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateW(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext) {
    dxvk::Logger::debug("<<< DirectDrawEnumerateW: Proxy");

    typedef HRESULT (__stdcall *DirectDrawEnumerateW_t)(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext);
    static DirectDrawEnumerateW_t ProxiedDirectDrawEnumerateW = nullptr;

    if (unlikely(ProxiedDirectDrawEnumerateW == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateW: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawEnumerateW = reinterpret_cast<DirectDrawEnumerateW_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateW"));

      if (unlikely(ProxiedDirectDrawEnumerateW == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateW: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedDirectDrawEnumerateW(lpCallback, lpContext);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::warn("DirectDrawEnumerateW: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall DllCanUnloadNow() {
    dxvk::Logger::warn("!!! DllCanUnloadNow: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
    dxvk::Logger::debug("<<< DllGetClassObject: Proxy");

    typedef HRESULT (__stdcall *DllGetClassObject_t)(REFCLSID rclsid, REFIID riid, LPVOID *ppv);
    static DllGetClassObject_t ProxiedDllGetClassObject = nullptr;

    dxvk::Logger::debug(dxvk::str::format("DllGetClassObject: Call for rclsid: ", rclsid));

    // TODO: Figure out a way to get Total Annihilation: Kingdoms to like what we return,
    // because simply calling CreateDirectDraw here does not work for some reason

    if (unlikely(ProxiedDllGetClassObject == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DllGetClassObject: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDllGetClassObject = reinterpret_cast<DllGetClassObject_t>(GetProcAddress(hDDraw, "DllGetClassObject"));

      if (unlikely(ProxiedDllGetClassObject == nullptr)) {
        dxvk::Logger::err("DllGetClassObject: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    HRESULT hr = ProxiedDllGetClassObject(rclsid, riid, ppv);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::warn("DllGetClassObject: Failed call to proxied interface");
    }

    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall GetDDSurfaceLocal(DWORD arg1, DWORD arg2, DWORD arg3) {
    dxvk::Logger::warn("!!! GetDDSurfaceLocal: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE7 *lpDDS, DWORD arg) {
    dxvk::Logger::warn("!!! GetSurfaceFromDC: Stub");
    return S_OK;
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
