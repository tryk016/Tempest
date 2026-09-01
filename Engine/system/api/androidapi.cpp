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
#include <exception>
#include <thread>
#include <utility>

using namespace Tempest;

namespace {

android_app*     g_app   = nullptr;
Tempest::Window* g_owner = nullptr;

std::atomic_bool g_running       {true};
std::atomic_bool g_windowChanged {false};

// Pointer identity is insufficient after APP_CMD_TERM_WINDOW: Android may
// allocate the replacement ANativeWindow at the same address. Remember the
// teardown independently so INIT_WINDOW still performs a full surface rebuild.
std::atomic_bool g_wasTerminated {false};

// Native window used by the caller's current VkSurfaceKHR. Same-window resize
// can use Swapchain::reset(); a different window needs a new surface.
ANativeWindow* g_lastWindow = nullptr;

std::function<void()>                   g_onSurfaceDestroyed;
std::function<void(SystemApi::Window*)> g_onSurfaceCreated;

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
  }

// GameActivity releases the native window only after APP_CMD_TERM_WINDOW's
// callback returns. GPU work referencing that window must therefore be drained
// synchronously here, before returning control to native_app_glue.
void handleAppCmd(android_app* app, int32_t cmd) {
  (void)app;
  switch(cmd) {
    case APP_CMD_INIT_WINDOW:
    case APP_CMD_WINDOW_RESIZED:
      g_windowChanged.store(true);
      break;
    case APP_CMD_TERM_WINDOW:
      if(g_onSurfaceDestroyed) {
        try {
          g_onSurfaceDestroyed();
          }
        catch(const std::exception& e) {
          Log::e("AndroidApi: onSurfaceDestroyed: ", e.what());
          }
        catch(...) {
          Log::e("AndroidApi: onSurfaceDestroyed: unknown exception");
          }
        }
      g_wasTerminated.store(true);
      g_windowChanged.store(false);
      break;
    case APP_CMD_DESTROY:
      g_running.store(false);
      break;
    default:
      break;
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
  g_windowChanged.store(false);
  g_wasTerminated.store(false);
  g_lastWindow = nullptr;
  g_onSurfaceDestroyed = {};
  g_onSurfaceCreated   = {};
  if(g_app!=nullptr) {
    g_app->onAppCmd = &handleAppCmd;
    waitForInitialWindow();
    }
  }

void AndroidApi::setSurfaceCallbacks(std::function<void()> onSurfaceDestroyed,
                                     std::function<void(SystemApi::Window*)> onSurfaceCreated) {
  g_onSurfaceDestroyed = std::move(onSurfaceDestroyed);
  g_onSurfaceCreated   = std::move(onSurfaceCreated);

  // Bootstrap already pumped the first INIT_WINDOW. Treat that window as the
  // initial surface and discard the bootstrap notification.
  g_lastWindow = g_app!=nullptr ? g_app->window : nullptr;
  g_windowChanged.store(false);
  }

AndroidApi::AndroidApi() {
  // The swapchain captures the native window at construction, so wait until
  // GameActivity supplies the initial ANativeWindow.
  waitForInitialWindow();
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner,
                                                uint32_t /*width*/, uint32_t /*height*/) {
  g_owner = owner;
  return reinterpret_cast<SystemApi::Window*>(g_app!=nullptr ? g_app->window : nullptr);
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, SystemApi::ShowMode /*sm*/) {
  g_owner = owner;
  return reinterpret_cast<SystemApi::Window*>(g_app!=nullptr ? g_app->window : nullptr);
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
  auto* wnd = reinterpret_cast<ANativeWindow*>(w);
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
    if(!cb.onTimer())
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
  const int timeoutMillis = g_owner!=nullptr && g_app->window!=nullptr ? 0 : -1;
  int id = ALooper_pollAll(timeoutMillis, nullptr, &events, reinterpret_cast<void**>(&source));
  while(id>=0) {
    if(source!=nullptr)
      source->process(g_app, source);
    if(g_app->destroyRequested!=0) {
      g_running.store(false);
      return;
      }
    id = ALooper_pollAll(0, nullptr, &events, reinterpret_cast<void**>(&source));
    }

  if(g_windowChanged.exchange(false) && g_owner!=nullptr && g_app->window!=nullptr) {
    // Consume this unconditionally. Short-circuiting it behind pointer
    // comparison would leave a stale true value for a later resize.
    const bool wasTerminated = g_wasTerminated.exchange(false);
    if(g_app->window!=g_lastWindow || wasTerminated) {
      if(g_onSurfaceCreated) {
        try {
          g_onSurfaceCreated(reinterpret_cast<SystemApi::Window*>(g_app->window));
          }
        catch(const std::exception& e) {
          Log::e("AndroidApi: onSurfaceCreated: ", e.what());
          }
        catch(...) {
          Log::e("AndroidApi: onSurfaceCreated: unknown exception");
          }
        }
      g_lastWindow = g_app->window;
      }
    else {
      SizeEvent e(ANativeWindow_getWidth(g_app->window), ANativeWindow_getHeight(g_app->window));
      dispatchResize(*g_owner, e);
      }
    }

  // GameActivity reports coordinates in native-window pixels, matching the
  // coordinate space used by Tempest's window and swapchain.
  android_input_buffer* ib = android_app_swap_input_buffers(g_app);
  if(ib!=nullptr) {
    for(uint64_t i=0; i<ib->motionEventsCount; ++i) {
      const GameActivityMotionEvent& m = ib->motionEvents[i];
      if(m.pointerCount==0 || g_owner==nullptr)
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

      MouseEvent e(int(x), int(y), Event::ButtonLeft, Event::M_NoModifier, 0, pid, type);
      if(type==Event::MouseDown)
        dispatchMouseDown(*g_owner, e);
      else if(type==Event::MouseUp)
        dispatchMouseUp(*g_owner, e);
      else if(type==Event::MouseMove)
        dispatchMouseMove(*g_owner, e);
      }
    android_app_clear_motion_events(ib);

    // Key mapping is outside first-light, but the fixed-size queue still must
    // be drained every frame to avoid stale/dropped hardware key events.
    android_app_clear_key_events(ib);
    }

  if(g_owner!=nullptr && g_app->window!=nullptr)
    dispatchRender(*g_owner);
  }

void AndroidApi::implSetWindowTitle(SystemApi::Window* /*w*/, const char* /*utf8*/) {
  }

#endif
