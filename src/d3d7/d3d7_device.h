#pragma once

#include "d3d7_include.h"
#include "d3d7_singlethread.h"
#include "d3d7_caps.h"
#include "ddraw7_wrapped_object.h"
#include "../d3d9/d3d9_bridge.h"

#include <array>
#include <unordered_map>

namespace dxvk {

  class D3D7Interface;
  class D3D7StateBlock;
  class DDraw7Surface;
  class DDraw7Interface;

  /**
  * \brief D3D7 device implementation
  *
  * Implements the IDirect3DDevice7 interfaces
  */
  class D3D7Device final : public DDrawWrappedObject<d3d9::IDirect3DDevice9, IDirect3DDevice7> {

    friend class D3D7StateBlock;

  public:
    D3D7Device(
        Com<IDirect3DDevice7>&& d3d7DeviceProxy,
        D3D7Interface* pParent,
        DDraw7Interface* pDD7Parent,
        Com<d3d9::IDirect3DDevice9>&& pDevice,
        DDraw7Surface* pSurface);

    ~D3D7Device();

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC7 *desc);

    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D7 **d3d);

    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirectDrawSurface7 *surface, DWORD flags);

    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirectDrawSurface7 **surface);

    HRESULT STDMETHODCALLTYPE Clear(DWORD count, D3DRECT *rects, DWORD flags, D3DCOLOR color, D3DVALUE z, DWORD stencil);

    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE SetViewport(D3DVIEWPORT7 *data);

    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT7 *data);

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL7 *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL7 *data);

    HRESULT STDMETHODCALLTYPE SetLight(DWORD idx, D3DLIGHT7 *data);

    HRESULT STDMETHODCALLTYPE GetLight(DWORD idx, D3DLIGHT7 *data);

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState);

    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState);

    HRESULT STDMETHODCALLTYPE BeginStateBlock();

    HRESULT STDMETHODCALLTYPE EndStateBlock(LPDWORD lpdwBlockHandle);

    HRESULT STDMETHODCALLTYPE PreLoad(IDirectDrawSurface7 *surface);

    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *pVertex, DWORD vertexCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *pVertex, DWORD vertexCount, WORD *pIndex, DWORD indexCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE SetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *pStridedData, DWORD stridedDataCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD  dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD  dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer7 *vb, DWORD startVertex, DWORD primitiveCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer7 *vb, DWORD startVertex, DWORD primitiveCount, WORD *pIndex, DWORD indexCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE ComputeSphereVisibility(D3DVECTOR *centers, D3DVALUE *radii, DWORD sphere_count, DWORD sphereCount, DWORD *visibility);

    HRESULT STDMETHODCALLTYPE GetTexture(DWORD stage, IDirectDrawSurface7 **surface);

    HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirectDrawSurface7 *surface);

    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState);

    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState);

    HRESULT STDMETHODCALLTYPE ValidateDevice(LPDWORD lpdwPasses);

    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD dwBlockHandle);

    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD dwBlockHandle);

    HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD dwBlockHandle);

    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE d3dsbType, LPDWORD lpdwBlockHandle);

    HRESULT STDMETHODCALLTYPE Load(IDirectDrawSurface7 *dst_surface, POINT *dst_point, IDirectDrawSurface7 *src_surface, RECT *src_rect, DWORD flags);

    HRESULT STDMETHODCALLTYPE LightEnable(DWORD dwLightIndex, BOOL bEnable);

    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD dwLightIndex, BOOL *pbEnable);

    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation);

    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation);

    HRESULT STDMETHODCALLTYPE GetInfo(DWORD info_id, void *info, DWORD info_size);

    DDraw7Surface* GetRenderTarget() const {
      return m_rt.ptr();
    }

    D3D7DeviceLock LockDevice() {
      return m_singlethread.AcquireLock();
    }

  private:

    void UploadIndices(void* indices, DWORD indexCount);

    inline HRESULT IntializeSurface9(DDraw7Surface* surface, bool renderTarget);

    inline bool ShouldRecord() { return m_recorder != nullptr; }

    D3D7Interface*                   m_parent;
    DDraw7Interface*                 m_DD7Parent;

    D3D7Singlethread                 m_singlethread;

    Com<IDxvkD3D8InterfaceBridge>    m_bridge;

    Com<DDraw7Surface, false>        m_rt;
    DDraw7Surface*                   m_rtOrig = nullptr;

    std::array<Com<DDraw7Surface, false>, caps7::TextureStageCount> m_textures;

    D3D7StateBlock* m_recorder       = nullptr;
    DWORD           m_recorderHandle = 0;
    DWORD           m_handle         = 0;
    std::unordered_map<DWORD, D3D7StateBlock> m_stateBlocks;

    // Value of D3DRS_LINEPATTERN
    D3DLINEPATTERN  m_linePattern   = {};
    // Value of D3DRS_ZVISIBLE (although the RS is not supported, its value is stored)
    DWORD           m_zVisible      = 0;

    Com<d3d9::IDirect3DSurface9>     m_rt9;
    Com<d3d9::IDirect3DSurface9>     m_ds9;

  };

}