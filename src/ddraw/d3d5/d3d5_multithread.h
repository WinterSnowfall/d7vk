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
  class D3D5DeviceLock {

  public:

    D3D5DeviceLock()
      : m_mutex(nullptr) { }

    D3D5DeviceLock(sync::RecursiveSpinlock& mutex)
      : m_mutex(&mutex) {
      mutex.lock();
    }

    D3D5DeviceLock(D3D5DeviceLock&& other)
      : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    D3D5DeviceLock& operator = (D3D5DeviceLock&& other) {
      if (m_mutex)
        m_mutex->unlock();

      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~D3D5DeviceLock() {
      if (m_mutex != nullptr)
        m_mutex->unlock();
    }

  private:

    sync::RecursiveSpinlock* m_mutex;

  };


  /**
   * \brief D3D5 context lock
   */
  class D3D5Multithread {

  public:

    D3D5Multithread(
      BOOL                  Protected);

    D3D5DeviceLock AcquireLock() {
      return m_protected
        ? D3D5DeviceLock(m_mutex)
        : D3D5DeviceLock();
    }

  private:

    BOOL            m_protected;

    sync::RecursiveSpinlock m_mutex;

  };

}