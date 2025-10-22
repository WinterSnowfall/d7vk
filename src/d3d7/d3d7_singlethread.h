#pragma once

#include "d3d7_include.h"

namespace dxvk {

  /**
   * \brief Device lock
   *
   * Lightweight RAII wrapper that implements
   * a subset of the functionality provided by
   * \c std::unique_lock, with the goal of being
   * cheaper to construct and destroy.
   */
  class D3D7DeviceLock {

  public:

    D3D7DeviceLock()
      : m_mutex(nullptr) { }

    D3D7DeviceLock(sync::RecursiveSpinlock& mutex)
      : m_mutex(&mutex) {
      mutex.lock();
    }

    D3D7DeviceLock(D3D7DeviceLock&& other)
      : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    D3D7DeviceLock& operator = (D3D7DeviceLock&& other) {
      if (m_mutex)
        m_mutex->unlock();

      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~D3D7DeviceLock() {
      if (m_mutex != nullptr)
        m_mutex->unlock();
    }

  private:

    sync::RecursiveSpinlock* m_mutex;

  };


  /**
   * \brief D3D7 context lock
   */
  class D3D7Singlethread {

  public:

    D3D7Singlethread(
      BOOL                  Protected);

    D3D7DeviceLock AcquireLock() {
      return m_protected
        ? D3D7DeviceLock(m_mutex)
        : D3D7DeviceLock();
    }

  private:

    BOOL            m_protected;

    sync::RecursiveSpinlock m_mutex;

  };

}