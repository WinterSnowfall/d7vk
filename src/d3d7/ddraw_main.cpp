#include "d3d7_include.h"

#include "ddraw7_interface.h"

namespace dxvk {

  //Logger Logger::s_instance("d3d7.log");

  HMODULE GetProxiedDDrawModule() {
    // TODO: This is very janky, but works for now
    static HMODULE hDDraw = LoadLibraryA("C:\\windows\\system32\\ddraw.dll");
    return hDDraw;
  }

  HRESULT CreateDirectDrawEx(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter) {
    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(iid != __uuidof(IDirectDraw7)))
      return DDERR_INVALIDPARAMS;

    try {
      HMODULE hDDraw = GetProxiedDDrawModule();

      if (unlikely(!hDDraw)) {
        Logger::err("CreateDirectDrawEx: Failed to load proxied ddraw.dll");
        *lplpDD = nullptr;
        return DDERR_GENERIC;
      }

      typedef HRESULT (__stdcall *DirectDrawCreateEx_t)(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter);
      DirectDrawCreateEx_t ProxiedDirectDrawCreateEx = reinterpret_cast<DirectDrawCreateEx_t>(GetProcAddress(hDDraw, "DirectDrawCreateEx"));

      if (unlikely(!ProxiedDirectDrawCreateEx)) {
        Logger::err("CreateDirectDrawEx: Failed GetProcAddress");
        *lplpDD = nullptr;
        return DDERR_GENERIC;
      }

      LPVOID lplpDDProxied = nullptr;
      HRESULT hr = ProxiedDirectDrawCreateEx(lpGUID, &lplpDDProxied, iid, pUnkOuter);

      if (unlikely(FAILED(hr))) {
        Logger::err("CreateDirectDrawEx: Failed call to proxied interface");
        *lplpDD = nullptr;
        return hr;
      }

      Com<IDirectDraw7> DDraw7IntfProxied = static_cast<IDirectDraw7*>(lplpDDProxied);
      *lplpDD = ref(new DDraw7Interface(std::move(DDraw7IntfProxied)));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      *lplpDD = nullptr;
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

}

extern "C" {

  DLLEXPORT HRESULT __stdcall AcquireDDThreadLock() {
    dxvk::Logger::warn("!!! AcquireDDThreadLock: Stub");
    return DD_OK;
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

    dxvk::Logger::warn("DirectDrawCreate is a forwarded interface only, expect breakage");

    HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

    if (unlikely(!hDDraw)) {
      dxvk::Logger::err("DirectDrawCreate: Failed to load proxied ddraw.dll");
      return DDERR_GENERIC;
    }

    typedef HRESULT (__stdcall *DirectDrawCreate_t)(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter);
    DirectDrawCreate_t ProxiedDirectDrawCreate = nullptr;
    ProxiedDirectDrawCreate = reinterpret_cast<DirectDrawCreate_t>(GetProcAddress(hDDraw, "DirectDrawCreate"));

    if (unlikely(!ProxiedDirectDrawCreate)) {
      dxvk::Logger::err("DirectDrawCreate: Failed GetProcAddress");
      return DDERR_GENERIC;
    }

    HRESULT hr = ProxiedDirectDrawCreate(lpGUID, lplpDD, pUnkOuter);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::err("DirectDrawCreate: Failed call to proxied interface");
    }

    return hr;
  }

  HRESULT WINAPI DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    dxvk::Logger::debug("<<< DirectDrawCreateClipper: Proxy");

    HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

    if (unlikely(!hDDraw)) {
      dxvk::Logger::err("DirectDrawCreateClipper: Failed to load proxied ddraw.dll");
      return DDERR_GENERIC;
    }

    typedef HRESULT (__stdcall *DirectDrawCreateClipper_t)(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter);
    DirectDrawCreateClipper_t ProxiedDirectDrawCreateClipper = nullptr;
    ProxiedDirectDrawCreateClipper = reinterpret_cast<DirectDrawCreateClipper_t>(GetProcAddress(hDDraw, "DirectDrawCreateClipper"));

    if (unlikely(!ProxiedDirectDrawCreateClipper)) {
      dxvk::Logger::err("DirectDrawCreateClipper: Failed GetProcAddress");
      return DDERR_GENERIC;
    }

    HRESULT hr = ProxiedDirectDrawCreateClipper(dwFlags, lplpDDClipper, pUnkOuter);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::err("DirectDrawCreateClipper: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawCreateEx(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter) {
    dxvk::Logger::debug(">>> DirectDrawCreateEx");
    return dxvk::CreateDirectDrawEx(lpGUID, lplpDD, iid, pUnkOuter);
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateA(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext) {
    dxvk::Logger::debug("<<< DirectDrawEnumerateA: Proxy");

    HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

    if (unlikely(!hDDraw)) {
      dxvk::Logger::err("DirectDrawEnumerateA: Failed to load proxied ddraw.dll");
      return DDERR_GENERIC;
    }

    typedef HRESULT (__stdcall *DirectDrawEnumerateA_t)(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext);
    DirectDrawEnumerateA_t ProxiedDirectDrawEnumerateA = nullptr;
    ProxiedDirectDrawEnumerateA = reinterpret_cast<DirectDrawEnumerateA_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateA"));

    if (unlikely(!ProxiedDirectDrawEnumerateA)) {
      dxvk::Logger::err("DirectDrawEnumerateA: Failed GetProcAddress");
      return DDERR_GENERIC;
    }

    HRESULT hr = ProxiedDirectDrawEnumerateA(lpCallback, lpContext);

    if (unlikely(FAILED(hr))) {
      dxvk::Logger::err("DirectDrawEnumerateA: Failed call to proxied interface");
    }

    return hr;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags) {
    dxvk::Logger::debug("<<< DirectDrawEnumerateExA: Proxy");

    HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

    if (unlikely(!hDDraw)) {
      dxvk::Logger::err("DirectDrawEnumerateExA: Failed to load proxied ddraw.dll");
      return DDERR_GENERIC;
    }

    typedef HRESULT (__stdcall *DirectDrawEnumerateA_t)(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags);
    DirectDrawEnumerateA_t ProxiedDirectDrawEnumerateExA = nullptr;
    ProxiedDirectDrawEnumerateExA = reinterpret_cast<DirectDrawEnumerateA_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateExA"));

    if (unlikely(!ProxiedDirectDrawEnumerateExA)) {
      dxvk::Logger::err("DirectDrawEnumerateExA: Failed GetProcAddress");
      return DDERR_GENERIC;
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

  DLLEXPORT HRESULT __stdcall GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE7 *lpDDS, DWORD arg) {
    dxvk::Logger::warn("!!! GetSurfaceFromDC: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall RegisterSpecialCase(DWORD arg1, DWORD arg2, DWORD arg3, DWORD arg4) {
    dxvk::Logger::warn("!!! RegisterSpecialCase: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall ReleaseDDThreadLock() {
    dxvk::Logger::warn("!!! ReleaseDDThreadLock: Stub");
    return DD_OK;
  }

}
