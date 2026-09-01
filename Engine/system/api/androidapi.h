#pragma once

#include <Tempest/SystemApi>

#include <functional>

struct android_app;
struct ANativeWindow;

namespace Tempest {

class AndroidApi final: SystemApi {
  public:
    using SystemApi::dispatchRender;

    static void setAndroidApp(android_app* app);

    // Registered after the initial swapchain exists. The destroy callback is
    // synchronous: it runs before GameActivity releases the ANativeWindow.
    // The create callback receives the new native window so the caller can
    // rebuild the VkSurfaceKHR as well as the swapchain images.
    static void setSurfaceCallbacks(std::function<void()> onSurfaceDestroyed,
                                    std::function<void(SystemApi::Window*)> onSurfaceCreated);

  private:
    AndroidApi();

    Window* implCreateWindow(Tempest::Window* owner, uint32_t width, uint32_t height) override;
    Window* implCreateWindow(Tempest::Window* owner, ShowMode sm) override;
    void    implDestroyWindow(Window* w) override;
    void    implExit() override;

    Rect implWindowClientRect(SystemApi::Window* w) override;
    bool implSetAsFullscreen(SystemApi::Window* w, bool fullScreen) override;
    bool implIsFullscreen(SystemApi::Window* w) override;

    void implSetCursorPosition(SystemApi::Window* w, int x, int y) override;
    void implShowCursor(SystemApi::Window* w, CursorShape cursor) override;

    bool implIsRunning() override;
    int  implExec(AppCallBack& cb) override;
    void implProcessEvents(AppCallBack& cb) override;

    void implSetWindowTitle(SystemApi::Window* w, const char* utf8) override;

  friend class SystemApi;
  };

}
