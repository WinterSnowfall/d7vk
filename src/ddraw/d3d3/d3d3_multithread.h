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
  class D3D3DeviceLock {

  public:

    D3D3DeviceLock()
      : m_mutex(nullptr) { }

    D3D3DeviceLock(sync::RecursiveSpinlock& mutex)
      : m_mutex(&mutex) {
      mutex.lock();
    }

    D3D3DeviceLock(D3D3DeviceLock&& other)
      : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    D3D3DeviceLock& operator = (D3D3DeviceLock&& other) {
      if (m_mutex)
        m_mutex->unlock();

      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~D3D3DeviceLock() {
      if (m_mutex != nullptr)
        m_mutex->unlock();
    }

  private:

    sync::RecursiveSpinlock* m_mutex;

  };


  /**
   * \brief D3D3 context lock
   */
  class D3D3Multithread {

  public:

    D3D3Multithread(
      BOOL                  Protected);

    D3D3DeviceLock AcquireLock() {
      return m_protected
        ? D3D3DeviceLock(m_mutex)
        : D3D3DeviceLock();
    }

  private:

    BOOL            m_protected;

    sync::RecursiveSpinlock m_mutex;

  };

}