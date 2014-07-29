// testFlashToy.cpp : main source file for testFlashToy.exe
//

#include "stdafx.h"
#include "resource.h"
#include "FlashToy.h"
#include "flash_memory_stream.h"

CAppModule _Module;

int Run(LPTSTR lpstrCmdLine = NULL, int nCmdShow = SW_SHOWDEFAULT) {
  CMessageLoop theLoop;
  _Module.AddMessageLoop(&theLoop);

  FlashUIFactory m_FlashToyFactory;
  FlashUI::Parameter param;
  param.resource_id = IDR_FLASH_TEST;
  m_FlashToyFactory.CreateFlashToy(param, NULL);

  int nRet = theLoop.Run();
  _Module.RemoveMessageLoop();
  return nRet;
}


int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                     LPTSTR lpstrCmdLine, int nCmdShow) {
  HRESULT hRes = ::CoInitialize(NULL);
  // If you are running on NT 4.0 or higher you can use the following
  // call instead to make the EXE free threaded. This means that calls
  // come in on a random RPC thread.
  // HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ATLASSERT(SUCCEEDED(hRes));

  // this resolves ATL window thunking problem when Microsoft Layer
  // for Unicode (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  // add flags to support other controls
  AtlInitCommonControls(ICC_BAR_CLASSES);

  hRes = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(hRes));

  int nRet = Run(lpstrCmdLine, nCmdShow);

  _Module.Term();
  ::CoUninitialize();

  return nRet;
}
