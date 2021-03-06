// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/browser/root_window_manager.h"

#include "include/base/cef_bind.h"
#include "include/wrapper/cef_helpers.h"
#include "cefclient/browser/main_context.h"
#include "cefclient/browser/test_runner.h"

namespace client {

RootWindowManager::RootWindowManager(bool terminate_when_all_windows_closed)
    : terminate_when_all_windows_closed_(terminate_when_all_windows_closed) {
}

RootWindowManager::~RootWindowManager() {
  // All root windows should already have been destroyed.
  DCHECK(root_windows_.empty());
}

scoped_refptr<RootWindow> RootWindowManager::CreateRootWindow(
    bool with_controls,
    bool with_osr,
    const CefRect& bounds,
    const std::string& url) {
  CefBrowserSettings settings;
  MainContext::Get()->PopulateBrowserSettings(&settings);

  scoped_refptr<RootWindow> root_window = RootWindow::Create();
  root_window->Init(this, with_controls, with_osr, bounds, settings,
                    url.empty() ? MainContext::Get()->GetMainURL() : url);

  // Store a reference to the root window on the main thread.
  OnRootWindowCreated(root_window);

  return root_window;
}

scoped_refptr<RootWindow> RootWindowManager::CreateRootWindowAsPopup(
    bool with_controls,
    bool with_osr,
    const CefPopupFeatures& popupFeatures,
    CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client,
    CefBrowserSettings& settings) {
  MainContext::Get()->PopulateBrowserSettings(&settings);

  scoped_refptr<RootWindow> root_window = RootWindow::Create();
  root_window->InitAsPopup(this, with_controls, with_osr,
                           popupFeatures, windowInfo, client, settings);

  // Store a reference to the root window on the main thread.
  OnRootWindowCreated(root_window);

  return root_window;
}

scoped_refptr<RootWindow> RootWindowManager::GetWindowForBrowser(
    int browser_id) {
  REQUIRE_MAIN_THREAD();

  RootWindowSet::const_iterator it = root_windows_.begin();
  for (; it != root_windows_.end(); ++it) {
    CefRefPtr<CefBrowser> browser = (*it)->GetBrowser();
    if (browser.get() && browser->GetIdentifier() == browser_id)
      return *it;
  }
  return NULL;
}

void RootWindowManager::CloseAllWindows(bool force) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(
        base::Bind(&RootWindowManager::CloseAllWindows, base::Unretained(this),
                  force));
    return;
  }

  if (root_windows_.empty())
    return;

  RootWindowSet::const_iterator it = root_windows_.begin();
  for (; it != root_windows_.end(); ++it)
    (*it)->Close(force);
}

void RootWindowManager::OnRootWindowCreated(
    scoped_refptr<RootWindow> root_window) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(
        base::Bind(&RootWindowManager::OnRootWindowCreated,
                   base::Unretained(this), root_window));
    return;
  }

  root_windows_.insert(root_window);
}

void RootWindowManager::OnTest(RootWindow* root_window, int test_id) {
  REQUIRE_MAIN_THREAD();

  test_runner::RunTest(root_window->GetBrowser(), test_id);
}

void RootWindowManager::OnExit(RootWindow* root_window) {
  REQUIRE_MAIN_THREAD();

  CloseAllWindows(false);
}

void RootWindowManager::OnRootWindowDestroyed(RootWindow* root_window) {
  REQUIRE_MAIN_THREAD();

  RootWindowSet::iterator it = root_windows_.find(root_window);
  DCHECK(it != root_windows_.end());
  if (it != root_windows_.end())
    root_windows_.erase(it);

  if (terminate_when_all_windows_closed_ && root_windows_.empty()) {
    // Quit the main message loop after all windows have closed.
    MainMessageLoop::Get()->Quit();
  }
}

}  // namespace client
