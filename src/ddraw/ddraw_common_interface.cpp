#include "ddraw_common_interface.h"

#include <algorithm>

namespace dxvk {

  DDrawCommonInterface::DDrawCommonInterface(const D3DOptions& options)
    : m_options ( options ) {
  }

  DDrawCommonInterface::~DDrawCommonInterface() {
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    auto it = std::find(m_surfaces.begin(), m_surfaces.end(), surface);
    if (likely(it != m_surfaces.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), surface);
      if (unlikely(it != m_surfaces.end())) {
        Logger::warn("DDrawCommonInterface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces.push_back(surface);
      }
    }
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), surface);
      if (likely(it != m_surfaces.end())) {
        m_surfaces.erase(it);
      } else {
        Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
      }
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface4* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    auto it = std::find(m_surfaces4.begin(), m_surfaces4.end(), surface);
    if (likely(it != m_surfaces4.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface4* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces4.begin(), m_surfaces4.end(), surface);
      if (unlikely(it != m_surfaces4.end())) {
        Logger::warn("DDrawCommonInterface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces4.push_back(surface);
      }
    }
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface4* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces4.begin(), m_surfaces4.end(), surface);
      if (likely(it != m_surfaces4.end())) {
        m_surfaces4.erase(it);
      } else {
        Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
      }
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface7* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    auto it = std::find(m_surfaces7.begin(), m_surfaces7.end(), surface);
    if (likely(it != m_surfaces7.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface7* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces7.begin(), m_surfaces7.end(), surface);
      if (unlikely(it != m_surfaces7.end())) {
        Logger::warn("DDrawCommonInterface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces7.push_back(surface);
      }
    }
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface7* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces7.begin(), m_surfaces7.end(), surface);
      if (likely(it != m_surfaces7.end())) {
        m_surfaces7.erase(it);
      } else {
        Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
      }
    }
  }

}