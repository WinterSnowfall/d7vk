#pragma once

#include "ddraw_include.h"

#include "ddraw/ddraw_clipper.h"
#include "ddraw/ddraw_palette.h"

namespace dxvk {

  class DDrawCommonSurface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonSurface();

    ~DDrawCommonSurface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
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

  private:

    bool    m_dirtyMipMaps = false;

    uint8_t m_mipCount     = 1;

    Com<DDrawClipper> m_clipper;
    Com<DDrawPalette> m_palette;

  };

}