#include <Tempest/Event>
#include <Tempest/Window>

#include <type_traits>

namespace {

class AppStateWindow final : public Tempest::Window {
  protected:
    void appStateEvent(Tempest::AppStateEvent&) override {
      }
  };

static_assert(std::is_base_of_v<Tempest::Event, Tempest::AppStateEvent>);
static_assert(std::is_base_of_v<Tempest::Window, AppStateWindow>);
static_assert(std::is_constructible_v<Tempest::AppStateEvent,
                                      Tempest::AppStateEvent::State>);
static_assert(std::is_same_v<decltype(Tempest::AppStateEvent::state),
                             const Tempest::AppStateEvent::State>);

}
