// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefsimple/simple_handler.h"

#include <sstream>
#include <string>

#include "include/base/cef_bind.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "DevtoolClient.h"

#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

enum {
	CONTEXT_MENU_ID_SHOW_DEVTOOLS = MENU_ID_USER_FIRST + 1
};

namespace {

SimpleHandler* g_instance = NULL;

}  // namespace

SimpleHandler::SimpleHandler()
    : is_closing_(false) {
  DCHECK(!g_instance);
  g_instance = this;
}

SimpleHandler::~SimpleHandler() {
  g_instance = NULL;
}

// static
SimpleHandler* SimpleHandler::GetInstance() {
  return g_instance;
}

void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  js_browser_delegate_ = new EasemobCefMessageRouterBrowserSideDelegate();

  // 创建了browser实例
  if (!GetBrowser())   {
	  base::AutoLock lock_scope(lock_);
	  // We need to keep the main child window, but not popup windows
	  browser_ = browser;
  }

}

bool SimpleHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  is_closing_ = true;
  return false;
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  CefQuitMessageLoop();
  js_browser_delegate_->OnBeforeClose(this, browser);
  browser_ = nullptr;
}

bool SimpleHandler::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message) {
	CEF_REQUIRE_UI_THREAD();

	if (js_browser_delegate_->OnProcessMessageReceived(this, browser, source_process,
		message)) {
		return true;
	}

	return false;
}

inline BYTE fromHex(const BYTE &x)
{
	return isdigit(x) ? x - '0' : x - 'A' + 10;
}

inline std::string URLDecode(const std::string &sIn)
{
	std::string sOut;
	for (size_t ix = 0; ix < sIn.size(); ix++)
	{
		BYTE ch = 0;
		if (sIn[ix] == '%')
		{
			ch = (fromHex(sIn[ix + 1]) << 4);
			ch |= fromHex(sIn[ix + 2]);
			ix += 2;
		}
		else if (sIn[ix] == '+')
		{
			ch = ' ';
		}
		else
		{
			ch = sIn[ix];
		}
		sOut += (char)ch;
	}
	return sOut;
}

bool SimpleHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	const CefString& target_url,
	const CefString& target_frame_name,
	CefLifeSpanHandler::WindowOpenDisposition target_disposition,
	bool user_gesture,
	const CefPopupFeatures& popupFeatures,
	CefWindowInfo& windowInfo,
	CefRefPtr<CefClient>& client,
	CefBrowserSettings& settings,
	bool* no_javascript_access)
{
	CefString dec = URLDecode(target_url);
	std::wstring url = dec.ToWString();
	ShellExecute(NULL, L"open", url.c_str(), NULL, NULL, 0);
	return true;
}

void SimpleHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  // Don't display an error for downloaded files.
  if (errorCode == ERR_ABORTED)
    return;

  // Display a load error message.
  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL " << std::string(failedUrl) <<
        " with error " << std::string(errorText) << " (" << errorCode <<
        ").</h2></body></html>";
  frame->LoadString(ss.str(), failedUrl);
}

CefRefPtr<CefBrowser> SimpleHandler::GetBrowser() const {
	base::AutoLock lock_scope(lock_);
	return browser_;
}


void SimpleHandler::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI,
        base::Bind(&SimpleHandler::CloseAllBrowsers, this, force_close));
    return;
  }

  if (browser_list_.empty())
    return;

  BrowserList::const_iterator it = browser_list_.begin();
  for (; it != browser_list_.end(); ++it)
    (*it)->GetHost()->CloseBrowser(force_close);
}

void SimpleHandler::OnBeforeContextMenu(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefContextMenuParams> params,
	CefRefPtr<CefMenuModel> model) {
	CEF_REQUIRE_UI_THREAD();

	model->InsertItemAt(0, MENU_ID_RELOAD_NOCACHE, CefString(L"刷新"));
	model->SetEnabled(MENU_ID_RELOAD_NOCACHE, !browser->IsLoading());

	model->SetLabel(MENU_ID_BACK, CefString(L"回退"));
	model->SetLabel(MENU_ID_CUT, CefString(L"剪切"));
	model->SetLabel(MENU_ID_COPY, CefString(L"复制"));
	model->SetLabel(MENU_ID_PASTE, CefString(L"粘贴"));

	model->Remove(MENU_ID_FORWARD);
	model->Remove(MENU_ID_PRINT);
	model->Remove(MENU_ID_VIEW_SOURCE);

	model->Remove(MENU_ID_UNDO);
	model->Remove(MENU_ID_REDO);
	model->Remove(MENU_ID_DELETE);
	model->Remove(MENU_ID_SELECT_ALL);

	model->AddCheckItem(CONTEXT_MENU_ID_SHOW_DEVTOOLS, CefString(L"显示开发者工具"));
}

bool SimpleHandler::OnContextMenuCommand(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefContextMenuParams> params,
	int command_id,
	EventFlags event_flags) {
	CEF_REQUIRE_UI_THREAD();

	switch (command_id) {
	case CONTEXT_MENU_ID_SHOW_DEVTOOLS:
		ShowDevTools(browser, CefPoint());
		return true;
	default:  // Allow default handling, if any.
		return false;
	}
	return true;
}

void SimpleHandler::ShowDevTools(CefRefPtr<CefBrowser> browser,
	const CefPoint& inspect_element_at) {
	CefWindowInfo windowInfo;
	CefBrowserSettings settings;
	CefRefPtr<CefClient> client(new CDevtoolClient(windowInfo, settings));

	browser->GetHost()->ShowDevTools(windowInfo, client, settings, inspect_element_at);
	int ia = 0;
	ia++;
}