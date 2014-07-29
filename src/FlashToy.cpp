#include "StdAfx.h"
#include "flashtoy.h"
#include "resource.h"
#include <atltypes.h>
#include <atlpath.h>

#include "flash_memory_stream.h"

#define CHECK_RET_HR(exp) \
  do{\
    hr = exp;\
    if (FAILED(hr)) \
      return hr;\
  }while(0)\

_ATL_FUNC_INFO FlashUI::s_OnReadyStateChangeInfo_ = {CC_STDCALL, VT_EMPTY, 1, {VT_I4}};
_ATL_FUNC_INFO FlashUI::s_OnProgressInfo_ = {CC_STDCALL, VT_EMPTY, 1, {VT_I4}}; 
_ATL_FUNC_INFO FlashUI::s_FSCommandInfo_ = {CC_STDCALL, VT_EMPTY, 2, {VT_BSTR, VT_BSTR}};
_ATL_FUNC_INFO FlashUI::s_FlashCallInfo_ = {CC_STDCALL, VT_EMPTY, 1, {VT_BSTR}}; 
LPWSTR FlashUI::WModeStrs[3] = {L"Window", L"Opaque", L"Transparent"};

namespace {
static const UINT kUpdateTimer = 1024;

bool GetResourceFromModule(HMODULE module,
  int resource_id,
  LPCTSTR resource_type,
  void** data,
  size_t* length) {
    if (!module)
      return false;

    if (!IS_INTRESOURCE(resource_id)) {
      return false;
    }

    HRSRC hres_info = FindResource(module, MAKEINTRESOURCE(resource_id),
      resource_type);
    if (NULL == hres_info)
      return false;

    DWORD data_size = SizeofResource(module, hres_info);
    HGLOBAL hres = LoadResource(module, hres_info);
    if (!hres)
      return false;

    void* resource = LockResource(hres);
    if (!resource)
      return false;

    *data = resource;
    *length = static_cast<size_t>(data_size);
    return true;
}

bool GetDataResourceFromModule(HMODULE module,
  int resource_id,
  void** data,
  size_t* length) {
    return GetResourceFromModule(module, resource_id, _T("BINDATA"), data, length);
}

HRESULT LoadFlashFromMemory(IShockwaveFlash* flash, void* data, size_t size) {
  FlashMemoryStream flash_stream(data, size);
  IPersistStreamInit *psi = 0;
  HRESULT hr = flash->QueryInterface(::IID_IPersistStreamInit,
                                      (LPVOID*)&psi);
  if (psi) {
    psi->InitNew();
    hr = psi->Load((LPSTREAM)&flash_stream);
    psi->Release();
  }
  
  return hr;
}

HRESULT LoadFlashFromFile(IShockwaveFlash* flash, const CString& swf_file) {
  return flash->LoadMovie(0, (_bstr_t)swf_file);
}

HRESULT LoadFlashFromResource(IShockwaveFlash* flash, int resource_id) {
  void* data = NULL;
  size_t length = 0;
  if (!GetDataResourceFromModule(::GetModuleHandle(NULL),
                                 resource_id, &data, &length)) {
      return ERROR_RESOURCE_NOT_FOUND;
  }
  return LoadFlashFromMemory(flash, data, length);
}

HRESULT LoadFlashMovie(IShockwaveFlash* flash, const FlashUI::Parameter& params) {
    HRESULT hr = ERROR_FILE_NOT_FOUND;
    if (!params.Movies.empty()) {
      hr = LoadFlashFromFile(flash, CString(params.Movies[0]));
    } else if (params.resource_id > 0) {
      hr = LoadFlashFromResource(flash, params.resource_id);
    }
    return hr;
}

}


FlashUI::Parameter::Parameter()
    : PlayMode(pm_alpha),
      MovieKeyFmt(_T("M%d")),
      MaxMovieCount(16),
      resource_id(0),
      UpdateInterval(100),
      PlayTime(0),
      Movies(),
      WinStyle(268435456),
      WinExStyle(128),
      WMode(2),
      BkColor(0),
      Quality(1),
      ScaleMode(3),
      AlignMode(0),
      Menu(true),
      PlayNow(true),
      DragMove(true),
      DisableRButtonDown(true) {
  memset(&WinRect, 0, sizeof(WinRect));
  WinRect.left  = 200;
  WinRect.right = 600;
  WinRect.top = 200;
  WinRect.bottom = 600;
}

FlashUI::Parameter::~Parameter() {
}

FlashUI::FlashUI()
    : deleted_(false),
      invalidate_(false),
      factory_(NULL),
      start_time_(0),
      play_time_(0),
      drag_move_(false),
      disable_RButton_down_(true),
      flash_version_(0),
      fix_flash_V8Transparent_bug_(false),
      bits_(NULL) {
}

FlashUI::~FlashUI() {
  deleted_ = true;
  if (IsWindow())
    DestroyWindow();
}

void FlashUI::Destroy() {
  if (factory_ == NULL)
    delete this;
  else
    factory_->DestoryFlashToy(this);
}

HRESULT FlashUI::InternalCreate(const FlashUI::Parameter& param, HWND hWndParent)
{
  DWORD dwStyle = param.WinStyle;
  DWORD dwExStyle = param.WinExStyle;
  CRect rcBound(param.WinRect);
  rcBound.NormalizeRect();
  COLORREF bkColor = param.BkColor;
  play_mode_ = param.PlayMode;
  start_time_ = GetTickCount();
  play_time_ = param.PlayTime;
  drag_move_ = param.DragMove;
  disable_RButton_down_ = param.DisableRButtonDown;
  caption_ = param.Caption.IsEmpty() ?
      (param.Movies.empty() ? _T("") : param.Movies[0]) : param.Caption;

  if (hWndParent != NULL) {
    dwStyle |= WS_CHILD;
  } else {
    if (rcBound.IsRectEmpty()) {
      full_screen_ = true;
      rcBound.SetRect(0, 0, GetSystemMetrics(SM_CXSCREEN),
                      GetSystemMetrics(SM_CYSCREEN));
    }
  }

  if (param.PlayMode == pm_transparent || param.PlayMode == pm_alpha) {
    dwExStyle |= WS_EX_LAYERED;
    dwExStyle |= WS_EX_APPWINDOW;
  }

  base::Create(NULL, _U_RECT(rcBound), caption_, dwStyle, dwExStyle);

  if (!IsWindow())
    return E_FAIL;

  SetWindowLong(GWL_STYLE, dwStyle);
  SetWindowLong(GWL_EXSTYLE, dwExStyle);
  CenterWindow();

  // register object for message filtering and idle updates
  CMessageLoop* pLoop = _Module.GetMessageLoop();
  ATLASSERT(pLoop != NULL);
  pLoop->AddMessageFilter(this);
  pLoop->AddIdleHandler(this);
  if (param.PlayMode == pm_transparent) {
    ::SetLayeredWindowAttributes(m_hWnd, bkColor, 255, LWA_COLORKEY);
  }

  SetTimer(kUpdateTimer, param.UpdateInterval, NULL);

  HRESULT hr;
  CHECK_RET_HR(flash_.CreateInstance(L"ShockwaveFlash.ShockwaveFlash"));
  flash_version_ = flash_->FlashVersion();
  if ((flash_version_ & 0x00FF0000) == 0x00080000)
    fix_flash_V8Transparent_bug_ = true;
  m_ViewObject = flash_;

  CHECK_RET_HR(flash_->put_WMode(getWModeStr(param.WMode)));
  CHECK_RET_HR(AttachControl(flash_, *this));
  CHECK_RET_HR(DispEventAdvise(flash_));
  CHECK_RET_HR(flash_->put_AlignMode(param.AlignMode));
  CHECK_RET_HR(flash_->put_BackgroundColor(bkColor));
  CHECK_RET_HR(flash_->put_Quality(param.Quality));
  CHECK_RET_HR(flash_->put_ScaleMode(param.ScaleMode));
  CHECK_RET_HR(flash_->put_Menu(param.Menu ? VARIANT_TRUE : VARIANT_FALSE));
  CHECK_RET_HR(flash_->put_Loop(param.Loop ? VARIANT_TRUE : VARIANT_FALSE));
  CHECK_RET_HR(LoadFlashMovie(flash_, param));
  CHECK_RET_HR(flash_->put_Playing(VARIANT_TRUE));

  return S_OK;
}

LRESULT FlashUI::OnWindowsClose(UINT uMsg, WPARAM wParam,
                                LPARAM lParam, BOOL& bHandled) {
  DestroyWindow();
  ::PostQuitMessage(0);
  return 0;
}

LPWSTR FlashUI::getWModeStr(int WMode) const {
  if (WMode >= 0 && WMode < sizeof(WModeStrs) / sizeof(WModeStrs[1]))
    return WModeStrs[WMode];
  else
    return WModeStrs[0];
}

LRESULT FlashUI::OnTimer(UINT uMsg, WPARAM wParam,
                         LPARAM lParam, BOOL& bHandled) {
  if (kUpdateTimer == wParam) {
    if (play_time_ > 0 && GetTickCount() - start_time_ > play_time_) {
      DestroyWindow();
      return 0;
    }

    if (invalidate_ && play_mode_ == pm_alpha) {
      UpdateWindowFromFlash();
    }
  }
  return 0;
}

LRESULT FlashUI::OnPaint(UINT uMsg, WPARAM wParam,
                         LPARAM lParam, BOOL& bHandled)
{
  if (play_mode_ == pm_alpha) {
    ValidateRect(NULL);
  } else {
    bHandled = FALSE;
  }
  return 0;
}

LRESULT FlashUI::OnLButtonDown(UINT uMsg, WPARAM wParam,
                               LPARAM lParam, BOOL& bHandled) {
  if (drag_move_) {
    PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, lParam);
  }

  bHandled = TRUE;
  return 0;
}

LRESULT FlashUI::OnRButtonDown(UINT uMsg, WPARAM wParam,
                               LPARAM lParam, BOOL& bHandled) {
  if (!disable_RButton_down_)
    bHandled = FALSE;
  return 0;
}

LRESULT FlashUI::OnDisplayChange(UINT uMsg, WPARAM wParam,
                                 LPARAM lParam, BOOL& bHandled) {
  if (!full_screen_)
    return 0;

  int cx = LOWORD(lParam);
  int cy = HIWORD(lParam);
  CRect rcWindow;
  GetWindowRect(rcWindow);
  if (cx != rcWindow.Width() || cy != rcWindow.Height()) {
    MoveWindow(0, 0, cx, cy, TRUE);
  }
  return 0;
}

void FlashUI::UpdateWindowFromFlash() {
  CWindowDC dcWindow(NULL);
  CDCT<true> dc;
  dc.CreateCompatibleDC(dcWindow);
  CBitmapT<true> bmp;
  CRect rcWindow;
  GetWindowRect(rcWindow);

  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biHeight = rcWindow.Height();
  bmi.bmiHeader.biWidth = rcWindow.Width();
  bmi.bmiHeader.biPlanes = 1;
  BYTE* pBits;
  bmp.CreateDIBSection(dcWindow, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, NULL);
  UINT nBitsSize =
      ((bmi.bmiHeader.biWidth + 3) & (~3)) * bmi.bmiHeader.biHeight * 4;
  CBitmapT<false> oldbmp = dc.SelectBitmap(bmp);
  RECTL rcDraw = {0, 0, rcWindow.Width(), rcWindow.Height()};
  bool bNeedRedraw = true;

  if (!fix_flash_V8Transparent_bug_) {
    m_ViewObject->Draw(DVASPECT_CONTENT, -1, 0, 0, 0, dc, &rcDraw, 0, 0, 0);
    bNeedRedraw = 1 || bits_ == NULL || memcmp(pBits, bits_, nBitsSize) != 0;
    if (bNeedRedraw) {
      bits_ = pBits;
      HBITMAP hbmp = bmp_.Detach();
      bmp_.Attach(bmp.Detach());
      bmp.Attach(hbmp);
    }
  } else {
    memset(pBits, 255, nBitsSize);
    m_ViewObject->Draw(DVASPECT_CONTENT, -1, 0, 0, 0, dc, &rcDraw, 0, 0, 0);
    bNeedRedraw = bits_ == NULL || memcmp(pBits, bits_, nBitsSize) != 0;
    if (bNeedRedraw) {
      BYTE* pOldMem = new BYTE[nBitsSize];
      memcpy(pOldMem, pBits, nBitsSize);
      memset(pBits, 0, nBitsSize);
      m_ViewObject->Draw(DVASPECT_CONTENT, -1, 0, 0, 0, dc, &rcDraw, 0, 0, 0);
      for (UINT i = 0; i < nBitsSize; i += 4) {
        pBits[i + 3] = 255 - pOldMem[i] + pBits[i];
      }
      delete[] bits_;
      bits_ = pOldMem;
    }
  }

  if (bNeedRedraw) {
    BLENDFUNCTION bf;
    bf.AlphaFormat = AC_SRC_ALPHA;
    bf.BlendFlags = 0; 
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    CPoint pt(0, 0);
    ::UpdateLayeredWindow(m_hWnd, dcWindow, NULL, &(rcWindow.Size()), dc, &pt, 0, &bf, ULW_ALPHA);
  }
  dc.SelectBitmap(oldbmp);
  invalidate_ = false;
}

void FlashUI::OnFinalMessage(HWND hWnd) {
  base::OnFinalMessage(hWnd);
  if (!deleted_)
    Destroy();
}

HRESULT __stdcall FlashUI::InvalidateRect(LPCRECT pRect, BOOL fErase) {
  if (play_mode_ == pm_alpha)
    invalidate_ = true;
  return base::InvalidateRect(pRect, fErase);
}

HRESULT __stdcall FlashUI::OnReadyStateChange(long newState) {
  ATLTRACE(_T("OnReadyStateChange(%d)\n"), newState);
  return S_OK;
}

HRESULT __stdcall FlashUI::OnProgress(long percentDone) {
  ATLTRACE(_T("OnProgress(%d)\n"), percentDone);
  return S_OK;
}

HRESULT __stdcall FlashUI::FSCommand(BSTR command, BSTR args) {
  ATLTRACE(_T("FSCommand(%s, %s)\n"), (LPCTSTR)CW2T(command), (LPCTSTR)CW2T(args));
  return S_OK;
}

HRESULT __stdcall FlashUI::FlashCall(BSTR request) {
  ATLTRACE(_T("FlashCall(%s)\n"), (LPCTSTR)CW2T(request));
  return S_OK;
}

LRESULT FlashUI::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/,
                              LPARAM /*lParam*/, BOOL& /*bHandled*/) {
  // center the dialog on the screen
  //UIAddChildWindowContainer(m_hWnd);
  return TRUE;
}

//////////////////////////////////////////////////////////////////////////
// class CFlashToyFactory
FlashUIFactory::FlashUIFactory()
    : m_cbLife(NULL),
      m_pUserData(NULL) {
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
};

FlashUIFactory::~FlashUIFactory() {
  DestoryAllFlashToy();
  CoUninitialize();
}

FlashUI* FlashUIFactory::CreateFlashToy(
    const FlashUI::Parameter& param, HWND hWndParent) {
  FlashUI* pInstance = NULL;
  try {
    pInstance = new FlashUI();
  } catch (...) {
    return NULL;
  }

  HRESULT hr = pInstance->InternalCreate(param, hWndParent);
  if (FAILED(hr)) {
    delete pInstance;
    return NULL;
  }

  pInstance->factory_ = this;
  m_Instances.insert(pInstance);
  if (m_cbLife)
    m_cbLife(pInstance, true, m_pUserData);

  return pInstance;
}

bool FlashUIFactory::DestoryFlashToy(FlashUI* pFlashToy) {
  FlashToySet::iterator found = m_Instances.find(pFlashToy);
  if (found == m_Instances.end())
    return false;

  if (m_cbLife)
    m_cbLife(pFlashToy, false, m_pUserData);

  m_Instances.erase(found);
  delete pFlashToy;
  return true;
}

void FlashUIFactory::DestoryAllFlashToy() {
  FlashToySet::iterator iter = m_Instances.begin();
  FlashToySet::iterator end = m_Instances.end();
  for (; iter != end; ++ iter) {
    if (m_cbLife)
      m_cbLife(*iter, false, m_pUserData);
    delete *iter;
  }
  m_Instances.clear();
}

const FlashUIFactory::FlashToySet& FlashUIFactory::GetFlashToySet() const {
  return m_Instances;
}

void FlashUIFactory::SetCallback(
    FlashUIFactory::LifeCallback cbLife, void* pUserData) {
  m_cbLife = cbLife;
  m_pUserData = pUserData;
}
