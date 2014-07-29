// Copyright 2013 Shenma Co., Ltd.  // NOLINT

#ifndef WOW_CHROME_INSTALLER_LUXURY_INSTALLER_WOW_FLASH_MEMORY_STREAM_H_
#define WOW_CHROME_INSTALLER_LUXURY_INSTALLER_WOW_FLASH_MEMORY_STREAM_H_

#include "stdafx.h"

class FlashMemoryStream : IStream {
 public:
  FlashMemoryStream(void* data, ULONG size) {
    this->data_ = data;
    this->size_ = size;
    this->pos_ = 0;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
    return E_NOTIMPL;
  }

  ULONG STDMETHODCALLTYPE AddRef() {
    return E_NOTIMPL;
  }

  ULONG STDMETHODCALLTYPE Release() {
    return E_NOTIMPL;
  }

  // IStream methods
  STDMETHOD(Read) (void *pv, ULONG cb, ULONG *pcbRead) {
    if (pos_ == 0 && cb == 4) {
      memcpy(pv, "fUfU", 4);
      pos_ += 4;
      if (pcbRead)
        *pcbRead = cb;
      return S_OK;
    } else if (pos_ == 4 && cb == 4) {
      memcpy(pv, &size_, 4);
      size_ += 8;
      pos_ += 4;
      if (pcbRead)
        *pcbRead = cb;
      return S_OK;
    } else {
      if (pos_ + cb > size_)
        cb = size_ - pos_;
      if (cb == 0)
        return S_FALSE;
      memcpy(pv, (char*)data_ + pos_ - 8, cb);
      if (pcbRead)
        *pcbRead = cb;
      pos_ += cb;
      return S_OK;
    }
  }

  STDMETHOD(Write) (void const *pv, ULONG cb, ULONG *pcbWritten) {
      return E_NOTIMPL;
  }

  STDMETHOD(Seek) (LARGE_INTEGER dlibMove, DWORD dwOrigin,
      ULARGE_INTEGER *plibNewPosition) {
    return E_NOTIMPL;
  }

  STDMETHOD(SetSize) (ULARGE_INTEGER libNewSize) { return E_NOTIMPL; }

  STDMETHOD(CopyTo) (IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead,
      ULARGE_INTEGER *pcbWritten) {
    return E_NOTIMPL;
  }

  STDMETHOD(Commit) (DWORD grfCommitFlags) { return E_NOTIMPL; }

  STDMETHOD(Revert) (void) { return E_NOTIMPL; }

  STDMETHOD(LockRegion) (ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
      DWORD dwLockType) {
    return E_NOTIMPL;
  }

  STDMETHOD(UnlockRegion) (ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
      DWORD dwLockType) {
    return E_NOTIMPL;
  }

  STDMETHOD(Stat) (STATSTG *pstatstg, DWORD grfStatFlag) { return E_NOTIMPL; }

  STDMETHOD(Clone) (IStream **ppstm) { return E_NOTIMPL; }

  void* data_;
  ULONG size_;
  ULONG pos_;
};

#endif  // WOW_CHROME_INSTALLER_LUXURY_INSTALLER_WOW_FLASH_MEMORY_STREAM_H_
