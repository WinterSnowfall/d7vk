#pragma once

#include "../ddraw_include.h"

namespace dxvk {

  /**
   * \brief Device lock
   *
   * Lightweight RAII wrapper that implements
   * a subset of the functionality provided by
   * \c std::unique_lock, with the goal of being
   * cheaper to construct and destroy.
   */
  class D3D6DeviceLock {

  public:

    D3D6DeviceLock()
      : m_mutex(nullptr) { }

    D3D6DeviceLock(sync::RecursiveSpinlock& mutex)
      : m_mutex(&mutex) {
      mutex.lock();
    }

    D3D6DeviceLock(D3D6DeviceLock&& other)
      : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    D3D6DeviceLock& operator = (D3D6DeviceLock&& other) {
      if (m_mutex)
        m_mutex->unlock();

      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~D3D6DeviceLock() {
      if (m_mutex != nullptr)
        m_mutex->unlock();
    }

  private:

    sync::RecursiveSpinlock* m_mutex;

  };


  /**
   * \brief D3D6 context lock
   */
  class D3D6Multithread {

  public:

    D3D6Multithread(
      BOOL                  Protected);

    D3D6DeviceLock AcquireLock() {
      return m_protected
        ? D3D6DeviceLock(m_mutex)
        : D3D6DeviceLock();
    }

  private:

    BOOL            m_protected;

    sync::RecursiveSpinlock m_mutex;

  };

}