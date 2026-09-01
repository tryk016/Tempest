#include "androidapi.h"

#include <Tempest/Event>
#include <Tempest/Log>
#include <Tempest/Platform>
#include <Tempest/Window>

#if defined(__ANDROID__)

#include <android/input.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <atomic>
#include <cstdint>
#include <exception>
#include <thread>

using namespace Tempest;

namespace {

android_app*     g_app   = nullptr;
Tempest::Window* g_owner = nullptr;

struct AndroidWindow final {
  // Borrowed from native_app_glue. VkSurfaceKHR takes its own reference.
  ANativeWindow* native = nullptr;
  };

AndroidWindow    g_window;
std::atomic_bool g_running {true};
bool             g_resumed = false;

bool canRender() noexcept {
  return g_running.load() && g_owner!=nullptr && g_resumed && g_window.native!=nullptr;
  }

void failLifecycle(const char* operation, const char* message) noexcept {
  g_running.store(false);
  try {
    Log::e("AndroidApi: ", operation, ": ", message);
    }
  catch(...) {
    }
  }

void dispatchResizeNoexcept(uint32_t width, uint32_t height) noexcept {
  if(g_owner==nullptr)
    return;
  try {
    SizeEvent e(width, height);
    AndroidApi::dispatchResize(*g_owner, e);
    }
  catch(const std::exception& e) {
    failLifecycle("surface resize", e.what());
    }
  catch(...) {
    failLifecycle("surface resize", "unknown exception");
    }
  }

bool g_touchActive = false;
int  g_touchId     = 0;
int  g_touchX      = 0;
int  g_touchY      = 0;

void cancelTouchNoexcept() noexcept {
  if(!g_touchActive)
    return;
  g_touchActive = false;
  if(g_owner==nullptr)
    return;
  try {
    MouseEvent e(g_touchX, g_touchY, Event::ButtonLeft, Event::M_NoModifier,
                 0, g_touchId, Event::MouseUp);
    AndroidApi::dispatchMouseUp(*g_owner, e);
    }
  catch(const std::exception& e) {
    failLifecycle("touch cancellation", e.what());
    }
  catch(...) {
    failLifecycle("touch cancellation", "unknown exception");
    }
  }

void waitForInitialWindow() {
  if(g_app==nullptr)
    return;
  while(g_app->window==nullptr && g_app->destroyRequested==0) {
    int                  events = 0;
    android_poll_source* source = nullptr;
    const int id = ALooper_pollAll(-1, nullptr, &events, reinterpret_cast<void**>(&source));
    if(id>=0 && source!=nullptr)
      source->process(g_app, source);
    }
  if(g_app->window!=nullptr && g_window.native==nullptr) {
    g_window.native = g_app->window;
    }
  g_resumed = g_app->activityState==APP_CMD_RESUME;
  }

// GameActivity releases the native window only after APP_CMD_TERM_WINDOW's
// callback returns. GPU work referencing that window must therefore be drained
// synchronously here, before returning control to native_app_glue.
void handleAppCmd(android_app* app, int32_t cmd) noexcept {
  try {
    switch(cmd) {
      case APP_CMD_INIT_WINDOW:
        g_window.native = app->window;
        if(g_window.native!=nullptr)
          dispatchResizeNoexcept(uint32_t(ANativeWindow_getWidth(g_window.native)),
                                 uint32_t(ANativeWindow_getHeight(g_window.native)));
        break;
      case APP_CMD_WINDOW_RESIZED:
      case APP_CMD_CONFIG_CHANGED:
        if(g_window.native!=nullptr)
          dispatchResizeNoexcept(uint32_t(ANativeWindow_getWidth(g_window.native)),
                                 uint32_t(ANativeWindow_getHeight(g_window.native)));
        break;
      case APP_CMD_TERM_WINDOW:
        cancelTouchNoexcept();
        g_window.native = nullptr;
        dispatchResizeNoexcept(0, 0);
        break;
      case APP_CMD_RESUME:
        g_resumed = true;
        break;
      case APP_CMD_PAUSE:
      case APP_CMD_STOP:
        g_resumed = false;
        cancelTouchNoexcept();
        break;
      case APP_CMD_DESTROY:
        g_resumed       = false;
        g_window.native = nullptr;
        cancelTouchNoexcept();
        g_running.store(false);
        break;
      default:
        break;
      }
    }
  catch(const std::exception& e) {
    failLifecycle("application command", e.what());
    }
  catch(...) {
    failLifecycle("application command", "unknown exception");
    }
  }

}

void AndroidApi::setAndroidApp(android_app* app) {
  // Native libraries can remain loaded across Activity recreation. Reset all
  // pointers and flags that may refer to the previous GameActivity instance
  // before installing the new glue object.
  g_app   = app;
  g_owner = nullptr;
  g_running.store(g_app!=nullptr);
  g_window      = {};
  g_resumed     = false;
  g_touchActive = false;
  if(g_app!=nullptr) {
    g_app->onAppCmd = &handleAppCmd;
    waitForInitialWindow();
    }
  }

ANativeWindow* AndroidApi::nativeWindow(SystemApi::Window* window) noexcept {
  if(window!=reinterpret_cast<SystemApi::Window*>(&g_window))
    return nullptr;
  return g_window.native;
  }

AndroidApi::AndroidApi() {
  // The swapchain captures the native window at construction, so wait until
  // GameActivity supplies the initial ANativeWindow.
  waitForInitialWindow();
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner,
                                                uint32_t /*width*/, uint32_t /*height*/) {
  if(owner==nullptr || g_owner!=nullptr || g_window.native==nullptr)
    return nullptr;
  g_owner = owner;
  return reinterpret_cast<SystemApi::Window*>(&g_window);
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, SystemApi::ShowMode /*sm*/) {
  return implCreateWindow(owner, 0, 0);
  }

void AndroidApi::implDestroyWindow(SystemApi::Window* /*w*/) {
  g_owner = nullptr;
  }

void AndroidApi::implExit() {
  g_running.store(false);
  if(g_app!=nullptr)
    g_app->destroyRequested = 1;
  }

Tempest::Rect AndroidApi::implWindowClientRect(SystemApi::Window* w) {
  auto* wnd = nativeWindow(w);
  if(wnd==nullptr)
    return Rect(0, 0, 0, 0);
  return Rect(0, 0, ANativeWindow_getWidth(wnd), ANativeWindow_getHeight(wnd));
  }

bool AndroidApi::implSetAsFullscreen(SystemApi::Window* /*w*/, bool /*fullScreen*/) {
  return true;
  }

bool AndroidApi::implIsFullscreen(SystemApi::Window* /*w*/) {
  return true;
  }

void AndroidApi::implSetCursorPosition(SystemApi::Window* /*w*/, int /*x*/, int /*y*/) {
  }

void AndroidApi::implShowCursor(SystemApi::Window* /*w*/, CursorShape /*cursor*/) {
  }

bool AndroidApi::implIsRunning() {
  return g_running.load();
  }

int AndroidApi::implExec(AppCallBack& cb) {
  while(implIsRunning()) {
    implProcessEvents(cb);
    if(canRender() && !cb.onTimer())
      std::this_thread::yield();
    }
  return 0;
  }

void AndroidApi::implProcessEvents(AppCallBack& /*cb*/) {
  if(g_app==nullptr)
    return;

  // Poll without blocking while rendering; wait for lifecycle work when no
  // renderable window exists.
  int                  events = 0;
  android_poll_source* source = nullptr;
  const int timeoutMillis = canRender() ? 0 : -1;
  int id = ALooper_pollAll(timeoutMillis, nullptr, &events, reinterpret_cast<void**>(&source));
  while(id>=0) {
    if(source!=nullptr)
      source->process(g_app, source);
    if(!g_running.load())
      return;
    if(g_app->destroyRequested!=0) {
      g_running.store(false);
      return;
      }
    id = ALooper_pollAll(0, nullptr, &events, reinterpret_cast<void**>(&source));
    }

  // GameActivity reports coordinates in native-window pixels, matching the
  // coordinate space used by Tempest's window and swapchain.
  android_input_buffer* ib = android_app_swap_input_buffers(g_app);
  if(ib!=nullptr) {
    for(uint64_t i=0; i<ib->motionEventsCount; ++i) {
      const GameActivityMotionEvent& m = ib->motionEvents[i];
      if(m.pointerCount==0 || !canRender())
        continue;

      const int32_t action = m.action & AMOTION_EVENT_ACTION_MASK;
      Event::Type type = Event::NoEvent;
      if(action==AMOTION_EVENT_ACTION_DOWN)
        type = Event::MouseDown;
      else if(action==AMOTION_EVENT_ACTION_UP || action==AMOTION_EVENT_ACTION_CANCEL)
        type = Event::MouseUp;
      else if(action==AMOTION_EVENT_ACTION_MOVE)
        type = Event::MouseMove;
      else
        continue;

      const float x   = GameActivityPointerAxes_getX(&m.pointers[0]);
      const float y   = GameActivityPointerAxes_getY(&m.pointers[0]);
      const int   pid = m.pointers[0].id;

      g_touchX  = int(x);
      g_touchY  = int(y);
      g_touchId = pid;
      if(type==Event::MouseDown)
        g_touchActive = true;

      MouseEvent e(int(x), int(y), Event::ButtonLeft, Event::M_NoModifier, 0, pid, type);
      if(type==Event::MouseDown)
        dispatchMouseDown(*g_owner, e);
      else if(type==Event::MouseUp)
        dispatchMouseUp(*g_owner, e);
      else if(type==Event::MouseMove)
        dispatchMouseMove(*g_owner, e);
      if(type==Event::MouseUp)
        g_touchActive = false;
      }
    android_app_clear_motion_events(ib);

    // Key mapping is outside first-light, but the fixed-size queue still must
    // be drained every frame to avoid stale/dropped hardware key events.
    android_app_clear_key_events(ib);
    }

  if(canRender())
    dispatchRender(*g_owner);
  }

void AndroidApi::implSetWindowTitle(SystemApi::Window* /*w*/, const char* /*utf8*/) {
  }

#endif
