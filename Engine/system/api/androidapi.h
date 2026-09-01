#pragma once

#include <Tempest/SystemApi>

struct android_app;
struct ANativeWindow;

namespace Tempest {

class AndroidApi final: SystemApi {
  public:
    using SystemApi::dispatchRender;
    using SystemApi::dispatchMouseUp;
    using SystemApi::dispatchResize;

    static void setAndroidApp(android_app* app);
    static ANativeWindow* nativeWindow(SystemApi::Window* window) noexcept;

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
