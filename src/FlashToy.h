// Copyright (c) 2013 Shenma Co., Ltd. . All rights reserved.  // NOLINT
#pragma once

#include "flash.tlh"
#include "AxHostBase.h"
#include <atltypes.h>
#include <atlstr.h>
#include <set>
#include <vector>

using namespace ShockwaveFlashObjects;

class FlashUIFactory;
class FlashUI : public CAxHostWindow,
                public CMessageFilter,
                public CIdleHandler,
                public IDispEventSimpleImpl<1, FlashUI, &__uuidof(_IShockwaveFlashEvents)> {
 public:
  typedef CAxHostWindow base;
  enum PlayModes {
    pm_normal                 = 0,
    pm_simulate_transparent   = 1,
    pm_simulate_alpha         = 2,
    pm_transparent            = 3,
    pm_alpha                  = 4
  };

  enum {WM_APP_UPDATE = WM_APP + 100};

  static LPWSTR WModeStrs[3];

  struct Parameter {
     Parameter();
     ~Parameter();

    std::vector<CString> Movies;
    int resource_id;
    CString MovieKeyFmt;
    int MaxMovieCount;
    CString Caption;
    PlayModes PlayMode;
    unsigned int UpdateInterval;
    unsigned int PlayTime;
    RECT WinRect;
    DWORD WinStyle;
    DWORD WinExStyle;
    int WMode;
    long BkColor;
    int Quality;
    int ScaleMode;
    int AlignMode;
    bool Menu;
    bool PlayNow;
    bool Loop;
    bool DragMove;
    bool DisableRButtonDown;
  };

  void Destroy();
  virtual void OnFinalMessage(HWND hWnd);

  DECLARE_WND_CLASS(_T("FlashUIWindow"))
  BEGIN_MSG_MAP(FlashUI)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_CLOSE, OnWindowsClose)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
    MESSAGE_HANDLER(WM_PAINT, OnPaint)
    MESSAGE_HANDLER(WM_DISPLAYCHANGE, OnDisplayChange)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
    MESSAGE_HANDLER(WM_RBUTTONDOWN, OnRButtonDown)
    CHAIN_MSG_MAP(base)
  END_MSG_MAP()

  BEGIN_SINK_MAP(FlashUI)
    SINK_ENTRY_INFO(1, __uuidof(_IShockwaveFlashEvents), 0xfffffd9f, OnReadyStateChange, &s_OnReadyStateChangeInfo_)
    SINK_ENTRY_INFO(1, __uuidof(_IShockwaveFlashEvents), 0x000007a6, OnProgress, &s_OnProgressInfo_)
    SINK_ENTRY_INFO(1, __uuidof(_IShockwaveFlashEvents), 0x00000096, FSCommand, &s_FSCommandInfo_)
    SINK_ENTRY_INFO(1, __uuidof(_IShockwaveFlashEvents), 0x000000c5, FlashCall, &s_FlashCallInfo_)
  END_SINK_MAP()

  STDMETHOD(InvalidateRect)(LPCRECT pRect, BOOL fErase);
  STDMETHOD_(ULONG, AddRef)() { return 1; }
  STDMETHOD_(ULONG, Release)() { return 1; }
  STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject) throw() {
    return _InternalQueryInterface(iid, ppvObject); 
  }

 protected:
  FlashUI(void);
  virtual ~FlashUI(void);

 private:
  HRESULT InternalCreate(const Parameter& param, HWND hWndParent);
  LRESULT OnWindowsClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnDisplayChange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnRButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
  HRESULT __stdcall OnReadyStateChange(long newState);
  HRESULT __stdcall OnProgress(long percentDone);
  HRESULT __stdcall FSCommand(BSTR command, BSTR args);
  HRESULT __stdcall FlashCall(BSTR request);

  LPWSTR getWModeStr(int WMode) const;
  void UpdateWindowFromFlash();

  // Override CIdleHandler
  virtual BOOL OnIdle() { return FALSE; }

  // Override CMessageFilter
  virtual BOOL PreTranslateMessage(MSG* pMsg) {return FALSE;}


 private:
  CBitmapT<true> bmp_;
  BYTE* bits_;
  FlashUIFactory* factory_;
  CString caption_;
  IShockwaveFlashPtr flash_;
  IViewObjectPtr m_ViewObject;
  long flash_version_;
  PlayModes play_mode_;
  DWORD start_time_;
  DWORD play_time_;
  bool full_screen_;
  bool deleted_;
  bool invalidate_;
  bool drag_move_;
  bool disable_RButton_down_;
  bool fix_flash_V8Transparent_bug_;

 private:
  static _ATL_FUNC_INFO s_OnReadyStateChangeInfo_;
  static _ATL_FUNC_INFO s_OnProgressInfo_;
  static _ATL_FUNC_INFO s_FSCommandInfo_;
  static _ATL_FUNC_INFO s_FlashCallInfo_;

  friend class FlashUIFactory;
};

class FlashUIFactory {
 public:
  typedef std::set<FlashUI*> FlashToySet;
  typedef void (*LifeCallback)(FlashUI* pInstance, bool bCreate, void* pUserData);
  
  FlashUIFactory();
  ~FlashUIFactory();
  FlashUI* CreateFlashToy(const FlashUI::Parameter& param, HWND hWndParent);
  bool DestoryFlashToy(FlashUI* pFlashToy);
  void DestoryAllFlashToy();
  const FlashToySet& GetFlashToySet() const;
  void SetCallback(LifeCallback cbLife, void* pUserData);

 private:
  FlashToySet m_Instances;
  LifeCallback m_cbLife;
  void* m_pUserData;
};

