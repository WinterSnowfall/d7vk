#include "d3d_common_device.h"

#include "d3d7/d3d7_device.h"
#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

namespace dxvk {

  D3DCommonDevice::D3DCommonDevice(
        DDrawCommonInterface* commonIntf,
        DWORD creationFlags9,
        uint32_t totalMemory)
    : m_totalMemory    ( totalMemory )
    , m_creationFlags9 ( creationFlags9 ) {
  }

  D3DCommonDevice::~D3DCommonDevice() {
  }

}