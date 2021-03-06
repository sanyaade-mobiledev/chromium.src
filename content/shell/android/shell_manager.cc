// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/android/shell_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/draw_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/shell.h"
#include "googleurl/src/gurl.h"
#include "jni/ShellManager_jni.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "ui/gfx/size.h"

#include <android/native_window_jni.h>

using base::android::ScopedJavaLocalRef;
using content::Compositor;
using content::DrawDelegate;

namespace {

class CompositorClient : public Compositor::Client {
 public:
  virtual void ScheduleComposite() OVERRIDE;
};

struct GlobalState {
  GlobalState()
      : g_scheduled_composite(false) {}
  base::android::ScopedJavaGlobalRef<jobject> j_obj;
  CompositorClient client;
  scoped_ptr<content::Compositor> compositor;
  scoped_ptr<WebKit::WebLayer> root_layer;
  bool g_scheduled_composite;
};

base::LazyInstance<GlobalState> g_global_state = LAZY_INSTANCE_INITIALIZER;

content::Compositor* GetCompositor() {
  return g_global_state.Get().compositor.get();
}

void Composite() {
  g_global_state.Get().g_scheduled_composite = false;
  if (GetCompositor()) {
    GetCompositor()->Composite();
  }
}

void CompositorClient::ScheduleComposite() {
  if (!g_global_state.Get().g_scheduled_composite) {
    g_global_state.Get().g_scheduled_composite = true;
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&Composite));
  }
}

} // anonymous namespace

namespace content {

jobject CreateShellView() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!GetCompositor()) {
    Compositor::Initialize();
    g_global_state.Get().compositor.reset(Compositor::Create(
        &g_global_state.Get().client));
    DCHECK(!g_global_state.Get().root_layer.get());
    g_global_state.Get().root_layer.reset(WebKit::WebLayer::create());
  }
  return Java_ShellManager_createShell(
      env, g_global_state.Get().j_obj.obj()).Release();
}

// Register native methods
bool RegisterShellManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void Init(JNIEnv* env, jclass clazz, jobject obj) {
  g_global_state.Get().j_obj.Reset(
      base::android::ScopedJavaLocalRef<jobject>(env, obj));
}

static void SurfaceCreated(
    JNIEnv* env, jclass clazz, jobject jsurface) {
  ANativeWindow* native_window = ANativeWindow_fromSurface(env, jsurface);
  if (native_window) {
    GetCompositor()->SetWindowSurface(native_window);
    ANativeWindow_release(native_window);
    GetCompositor()->SetRootLayer(g_global_state.Get().root_layer.get());
  }
}

static void SurfaceDestroyed(JNIEnv* env, jclass clazz) {
  GetCompositor()->SetWindowSurface(NULL);
}

static void SurfaceSetSize(
    JNIEnv* env, jclass clazz, jint width, jint height) {
  gfx::Size size = gfx::Size(width, height);
  DrawDelegate::GetInstance()->SetBounds(size);
  GetCompositor()->SetWindowBounds(size);
}

void LaunchShell(JNIEnv* env, jclass clazz, jstring jurl) {
  ShellBrowserContext* browserContext =
      static_cast<ShellContentBrowserClient*>(
          GetContentClient()->browser())->browser_context();
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  Shell::CreateNewWindow(browserContext,
                         url,
                         NULL,
                         MSG_ROUTING_NONE,
                         NULL);
}

void ShellAttachLayer(WebKit::WebLayer* layer) {
  g_global_state.Get().root_layer->addChild(layer);
}


void ShellRemoveLayer(WebKit::WebLayer* layer) {
  layer->removeFromParent();
}

}  // namespace content
