#include "d3d3_interface.h"

#include "d3d3_material.h"
#include "d3d3_viewport.h"

#include "../d3d_common_light.h"

#include "../ddraw/ddraw_interface.h"

namespace dxvk {

  uint32_t D3D3Interface::s_intfCount = 0;

  D3D3Interface::D3D3Interface(D3DCommonInterface* commonD3DIntf, Com<IDirect3D>&& d3d3IntfProxy, DDrawInterface* pParent)
    : DDrawWrappedObject<DDrawInterface, IDirect3D, d3d9::IDirect3D9>(pParent, std::move(d3d3IntfProxy), std::move(d3d9::Direct3DCreate9(D3D_SDK_VERSION)))
    , m_commonD3DIntf ( commonD3DIntf ) {
    // Get the bridge interface to D3D9.
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D3Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    if (m_commonD3DIntf == nullptr)
      m_commonD3DIntf = new D3DCommonInterface(D3DOptions(*m_bridge->GetConfig()));

    m_commonD3DIntf->SetD3D3Interface(this);

    m_bridge->EnableD3D3CompatibilityMode();

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D3Interface: Created a new interface nr. ((1-", m_intfCount, "))"));
  }

  D3D3Interface::~D3D3Interface() {
    if (m_commonD3DIntf->GetD3D3Interface() == this)
      m_commonD3DIntf->SetD3D3Interface(nullptr);

    // Clear the common interface pointer if it points to this interface
    if (m_parent != nullptr && m_parent->GetCommonInterface()->GetD3D3Interface() == this)
      m_parent->GetCommonInterface()->SetD3D3Interface(nullptr);

    Logger::debug(str::format("D3D3Interface: Interface nr. ((1-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D3Interface::AddRef() {
    if (likely(m_parent != nullptr)) {
      IUnknown* origin = m_parent->GetCommonInterface()->GetOrigin();
      if (likely(origin != nullptr))
        return origin->AddRef();
      else
        return m_parent->AddRef();
    } else {
      return ComObjectClamp::AddRef();
    }
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D3Interface::Release() {
    if (likely(m_parent != nullptr)) {
      IUnknown* origin = m_parent->GetCommonInterface()->GetOrigin();
      if (likely(origin != nullptr))
        return origin->Release();
      else
        return m_parent->Release();
    } else {
      return ComObjectClamp::Release();
    }
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirect3D, d3d9::IDirect3D9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3D))
      return this;

    Logger::debug("D3D3Interface::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Some games query for ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      Logger::debug("D3D3Interface::QueryInterface: Query for IDirectDraw");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    // Deathtrap Dungeon queries for IDirect3D2... not sure if this ever worked
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      Logger::warn("D3D3Interface::QueryInterface: Query for IDirect3D2");
      return m_parent->QueryInterface(riid, ppvObject);
    }

    try {
      *ppvObject = ref(this->GetInterface(riid));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::warn(e.message());
      Logger::warn(str::format(riid));
      return E_NOINTERFACE;
    }
  }

  // Docs state: "This method is provided for compliance with the COM protocol.
  // Returns DDERR_ALREADYINITIALIZED because the Direct3D object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3Interface::Initialize(REFIID riid) {
    Logger::debug(">>> D3D3Interface::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
    Logger::debug(">>> D3D3Interface::EnumDevices");

    if (unlikely(lpEnumDevicesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonD3DIntf->GetOptions();

    // D3D3 reports both HAL and HEL caps for any type of device,
    // with minor differences between the two. Note that the
    // device listing order matters, so list RAMP first, RGB second,
    // and HAL last. A RAMP device also needs to be advertised in D3D3,
    // since some games like Resident Evil expect it to be present.

    // RAMP device (monochrome), this is expected to be exposed
    GUID guidRAMP = IID_IDirect3DRampDevice;
    D3DDEVICEDESC3 desc3RAMP_HAL = GetD3D3Caps(d3dOptions->supportD16);
    D3DDEVICEDESC3 desc3RAMP_HEL = desc3RAMP_HAL;
    D3DDEVICEDESC descRAMP_HAL = { };
    D3DDEVICEDESC descRAMP_HEL = { };
    desc3RAMP_HAL.dwFlags = 0;
    desc3RAMP_HAL.dcmColorModel = 0;
    // RAMP devices use a monochrome color model
    desc3RAMP_HEL.dcmColorModel = D3DCOLOR_MONO;
    // Some applications apparently care about RGB texture caps
    desc3RAMP_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc3RAMP_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc3RAMP_HEL.dpcLineCaps.dwTextureCaps |= D3DPTEXTURECAPS_POW2;
    desc3RAMP_HEL.dpcTriCaps.dwTextureCaps  |= D3DPTEXTURECAPS_POW2;
    memcpy(&descRAMP_HAL, &desc3RAMP_HAL, sizeof(D3DDEVICEDESC3));
    memcpy(&descRAMP_HEL, &desc3RAMP_HEL, sizeof(D3DDEVICEDESC3));
    char deviceDescRAMP[100] = "D3VK RAMP";
    char deviceNameRAMP[100] = "D3VK RAMP";

    HRESULT hr = lpEnumDevicesCallback(&guidRAMP, &deviceDescRAMP[0], &deviceNameRAMP[0],
                                       &descRAMP_HAL, &descRAMP_HEL, lpUserArg);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    // Software emulation, this is expected to be exposed
    GUID guidRGB = IID_IDirect3DRGBDevice;
    D3DDEVICEDESC3 desc3RGB_HAL = GetD3D3Caps(d3dOptions->supportD16);
    D3DDEVICEDESC3 desc3RGB_HEL = desc3RGB_HAL;
    D3DDEVICEDESC descRGB_HAL = { };
    D3DDEVICEDESC descRGB_HEL = { };
    desc3RGB_HAL.dwFlags = 0;
    desc3RGB_HAL.dcmColorModel = 0;
    // Some applications apparently care about RGB texture caps
    desc3RGB_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc3RGB_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc3RGB_HEL.dpcLineCaps.dwTextureCaps |= D3DPTEXTURECAPS_POW2;
    desc3RGB_HEL.dpcTriCaps.dwTextureCaps  |= D3DPTEXTURECAPS_POW2;
    memcpy(&descRGB_HAL, &desc3RGB_HAL, sizeof(D3DDEVICEDESC3));
    memcpy(&descRGB_HEL, &desc3RGB_HEL, sizeof(D3DDEVICEDESC3));
    char deviceDescRGB[100] = "D3VK RGB";
    char deviceNameRGB[100] = "D3VK RGB";

    hr = lpEnumDevicesCallback(&guidRGB, &deviceDescRGB[0], &deviceNameRGB[0],
                               &descRGB_HAL, &descRGB_HEL, lpUserArg);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    // Hardware acceleration
    GUID guidHAL = IID_IDirect3DHALDevice;
    D3DDEVICEDESC3 desc3HAL_HAL = GetD3D3Caps(d3dOptions->supportD16);
    D3DDEVICEDESC3 desc3HAL_HEL = desc3HAL_HAL;
    D3DDEVICEDESC descHAL_HAL = { };
    D3DDEVICEDESC descHAL_HEL = { };
    desc3HAL_HEL.dcmColorModel = 0;
    // Some applications apparently care about RGB texture caps
    desc3HAL_HEL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc3HAL_HEL.dpcTriCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    memcpy(&descHAL_HAL, &desc3HAL_HAL, sizeof(D3DDEVICEDESC3));
    memcpy(&descHAL_HEL, &desc3HAL_HEL, sizeof(D3DDEVICEDESC3));
    char deviceDescHAL[100] = "D3VK HAL";
    char deviceNameHAL[100] = "D3VK HAL";

    hr = lpEnumDevicesCallback(&guidHAL, &deviceDescHAL[0], &deviceNameHAL[0],
                               &descHAL_HAL, &descHAL_HEL, lpUserArg);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D3Interface::CreateLight");

    if (unlikely(lplpDirect3DLight == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DLight);

    *lplpDirect3DLight = ref(new D3DLight(nullptr, this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateMaterial(LPDIRECT3DMATERIAL *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D3Interface::CreateMaterial");

    if (unlikely(lplpDirect3DMaterial == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DMaterial);

    D3DMATERIALHANDLE handle = m_commonD3DIntf->GetNextMaterialHandle();
    Com<D3D3Material> d3d3Material = new D3D3Material(nullptr, this, handle);

    m_materials.emplace(std::piecewise_construct,
                        std::forward_as_tuple(handle),
                        std::forward_as_tuple(d3d3Material.ptr()));

    *lplpDirect3DMaterial = d3d3Material.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateViewport(LPDIRECT3DVIEWPORT *lplpD3DViewport, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D3Interface::CreateViewport");

    Com<IDirect3DViewport> lplpD3DViewportProxy;
    HRESULT hr = m_proxy->CreateViewport(&lplpD3DViewportProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    InitReturnPtr(lplpD3DViewport);

    *lplpD3DViewport = ref(new D3D3Viewport(nullptr, std::move(lplpD3DViewportProxy), this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    Logger::debug(">>> D3D3Interface::FindDevice");

    if (unlikely(lpD3DFDS == nullptr || lpD3DFDR == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpD3DFDS->dwSize != sizeof(D3DFINDDEVICESEARCH)))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonD3DIntf->GetOptions();

    // Software emulation, this is expected to be exposed
    D3DDEVICEDESC3 descRGB_HAL = GetD3D3Caps(d3dOptions->supportD16);
    D3DDEVICEDESC3 descRGB_HEL = descRGB_HAL;
    descRGB_HAL.dwFlags = 0;
    descRGB_HAL.dcmColorModel = 0;
    // Some applications apparently care about RGB texture caps
    descRGB_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descRGB_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descRGB_HEL.dpcLineCaps.dwTextureCaps |= D3DPTEXTURECAPS_POW2;
    descRGB_HEL.dpcTriCaps.dwTextureCaps  |= D3DPTEXTURECAPS_POW2;

    // Hardware acceleration
    D3DDEVICEDESC3 descHAL_HAL = GetD3D3Caps(d3dOptions->supportD16);
    D3DDEVICEDESC3 descHAL_HEL = descHAL_HAL;
    descHAL_HEL.dcmColorModel = 0;
    // Some applications apparently care about RGB texture caps
    descHAL_HEL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descHAL_HEL.dpcTriCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descHAL_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;

    D3DFINDDEVICERESULT3 lpD3DFRD3 = { };
    lpD3DFRD3.dwSize = sizeof(D3DFINDDEVICERESULT3);

    if (lpD3DFDS->dwFlags & D3DFDS_GUID) {
      Logger::debug("D3D3Interface::FindDevice: Matching by device GUID");

      if (lpD3DFDS->guid == IID_IDirect3DRGBDevice ||
          lpD3DFDS->guid == IID_IDirect3DMMXDevice ||
          lpD3DFDS->guid == IID_IDirect3DRampDevice) {
        Logger::debug("D3D3Interface::FindDevice: Matched IID_IDirect3DRGBDevice");
        lpD3DFRD3.guid = IID_IDirect3DRGBDevice;
        lpD3DFRD3.ddHwDesc = descRGB_HAL;
        lpD3DFRD3.ddSwDesc = descRGB_HEL;
      } else if (lpD3DFDS->guid == IID_IDirect3DHALDevice) {
        Logger::debug("D3D3Interface::FindDevice: Matched IID_IDirect3DHALDevice");
        lpD3DFRD3.guid = IID_IDirect3DHALDevice;
        lpD3DFRD3.ddHwDesc = descHAL_HAL;
        lpD3DFRD3.ddSwDesc = descHAL_HEL;
      } else {
        Logger::err(str::format("D3D3Interface::FindDevice: Unknown device type: ", lpD3DFDS->guid));
        return DDERR_NOTFOUND;
      }

      memcpy(lpD3DFDR, &lpD3DFRD3, sizeof(D3DFINDDEVICERESULT3));
    } else if (lpD3DFDS->dwFlags & D3DFDS_HARDWARE) {
      Logger::debug("D3D3Interface::FindDevice: Matching by hardware flag");

      if (likely(lpD3DFDS->bHardware == TRUE)) {
        Logger::debug("D3D3Interface::FindDevice: Matched IID_IDirect3DHALDevice");
        lpD3DFRD3.guid = IID_IDirect3DHALDevice;
        lpD3DFRD3.ddHwDesc = descHAL_HAL;
        lpD3DFRD3.ddSwDesc = descHAL_HEL;
      } else {
        Logger::debug("D3D3Interface::FindDevice: Matched IID_IDirect3DRGBDevice");
        lpD3DFRD3.guid = IID_IDirect3DRGBDevice;
        lpD3DFRD3.ddHwDesc = descRGB_HAL;
        lpD3DFRD3.ddSwDesc = descRGB_HEL;
      }

      memcpy(lpD3DFDR, &lpD3DFRD3, sizeof(D3DFINDDEVICERESULT3));
    } else {
      Logger::err("D3D3Interface::FindDevice: Unhandled matching type");
      return DDERR_NOTFOUND;
    }

    return D3D_OK;
  }

  D3D3Material* D3D3Interface::GetMaterialFromHandle(D3DMATERIALHANDLE handle) {
    auto materialsIter = m_materials.find(handle);

    if (unlikely(materialsIter == m_materials.end()))
      return nullptr;

    return materialsIter->second;
  }

  void D3D3Interface::ReleaseMaterialHandle(D3DMATERIALHANDLE handle) {
    auto materialsIter = m_materials.find(handle);

    if (likely(materialsIter != m_materials.end()))
      m_materials.erase(materialsIter);
  }

}