// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/android/content_view_util.h"
#include "chrome/browser/android/dev_tools_server.h"
#include "chrome/browser/android/google_location_settings_helper.h"
#include "chrome/browser/android/google_location_settings_helper_factory.h"
#include "chrome/browser/android/intent_helper.h"
#include "chrome/browser/android/process_utils.h"
#include "chrome/browser/android/provider/chrome_browser_provider.h"
#include "chrome/browser/component/navigation_interception/component_jni_registrar.h"
#include "chrome/browser/component/web_contents_delegate_android/component_jni_registrar.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/ui/android/autofill/autofill_external_delegate.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/android/javascript_app_modal_dialog_android.h"

namespace chrome {
namespace android {

static base::android::RegistrationMethod kChromeRegisteredMethods[] = {
  { "AutofillExternalDelegate",
      AutofillExternalDelegateAndroid::RegisterAutofillExternalDelegate},
  { "ChromeBrowserProvider",
      ChromeBrowserProvider::RegisterChromeBrowserProvider },
  { "ChromeHttpAuthHandler",
      ChromeHttpAuthHandler::RegisterChromeHttpAuthHandler },
  { "ChromeWebContentsDelegateAndroid",
      RegisterChromeWebContentsDelegateAndroid },
  { "ContentViewUtil", RegisterContentViewUtil },
  { "DevToolsServer", RegisterDevToolsServer },
  { "GoogleLocationSettingsHelper",
      GoogleLocationSettingsHelper::Register },
  { "GoogleLocationSettingsHelperFactory",
      GoogleLocationSettingsHelperFactory::Register },
  { "IntentHelper", RegisterIntentHelper },
  { "JavascriptAppModalDialog",
     JavascriptAppModalDialogAndroid::RegisterJavascriptAppModalDialog },
  { "ProcessUtils", RegisterProcessUtils },
  { "SqliteCursor", SQLiteCursor::RegisterSqliteCursor },
  { "navigation_interception", navigation_interception::RegisterJni },
};

bool RegisterJni(JNIEnv* env) {
  web_contents_delegate_android::RegisterJni(env);
  return RegisterNativeMethods(env, kChromeRegisteredMethods,
                               arraysize(kChromeRegisteredMethods));
}

} // namespace android
} // namespace chrome
