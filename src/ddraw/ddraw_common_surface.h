#pragma once

#include "ddraw_include.h"

#include "ddraw_common_interface.h"

#include "ddraw/ddraw_clipper.h"
#include "ddraw/ddraw_palette.h"

namespace dxvk {

  class DDraw7Surface;
  class DDraw4Surface;
  class DDraw3Surface;
  class DDraw2Surface;
  class DDrawSurface;

  class DDrawCommonSurface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonSurface(DDrawCommonInterface* commonIntf);

    ~DDrawCommonSurface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf.ptr();
    }

    uint8_t GetMipCount() const {
      return m_mipCount;
    }

    void SetMipCount(uint8_t mipCount) {
      m_mipCount = mipCount;
    }

    bool HasDirtyMipMaps() const {
      return m_dirtyMipMaps;
    }

    void DirtyMipMaps() {
      m_dirtyMipMaps = true;
    }

    void UnDirtyMipMaps() {
      m_dirtyMipMaps = false;
    }

    void SetClipper(DDrawClipper* clipper) {
      m_clipper = clipper;
    }

    DDrawClipper* GetClipper() const {
      return m_clipper.ptr();
    }

    void SetPalette(DDrawPalette* palette) {
      m_palette = palette;
    }

    DDrawPalette* GetPalette() const {
      return m_palette.ptr();
    }

    void SetDD7Surface(DDraw7Surface* surf7) {
      m_surf7 = surf7;
    }

    DDraw7Surface* GetDD7Surface() const {
      return m_surf7;
    }

    void SetDD4Surface(DDraw4Surface* surf4) {
      m_surf4 = surf4;
    }

    DDraw4Surface* GetDD4Surface() const {
      return m_surf4;
    }

    void SetDD3Surface(DDraw3Surface* surf3) {
      m_surf3 = surf3;
    }

    DDraw3Surface* GetDD3Surface() const {
      return m_surf3;
    }

    void SetDD2Surface(DDraw2Surface* surf2) {
      m_surf2 = surf2;
    }

    DDraw2Surface* GetDD2Surface() const {
      return m_surf2;
    }

    void SetDDSurface(DDrawSurface* surf) {
      m_surf = surf;
    }

    DDrawSurface* GetDDSurface() const {
      return m_surf;
    }

  private:

    bool                      m_dirtyMipMaps = false;

    uint8_t                   m_mipCount     = 1;

    Com<DDrawClipper>         m_clipper;
    Com<DDrawPalette>         m_palette;

    Com<DDrawCommonInterface> m_commonIntf;

    // Track all possible surface versions of the same object
    DDraw7Surface*            m_surf7        = nullptr;
    DDraw4Surface*            m_surf4        = nullptr;
    DDraw3Surface*            m_surf3        = nullptr;
    DDraw2Surface*            m_surf2        = nullptr;
    DDrawSurface*             m_surf         = nullptr;

  };

}