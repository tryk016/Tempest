#include <Tempest/Widget>
#include <Tempest/Button>

#include <Tempest/Window>
#include <Tempest/EventDispatcher>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

MouseEvent mkMEvent(Event::Type t, int x,int y){
  return MouseEvent(x,y,Event::ButtonLeft,Event::M_NoModifier,0,0,t);
  }

KeyEvent mkKEvent(Event::KeyType kt, Event::Type t){
  return KeyEvent(kt,t);
  }

struct TstButton:Button {
  int down=0;
  int up  =0;
  int move=0;

  void mouseDownEvent(Tempest::MouseEvent&) override {
    down++;
    }
  void mouseUpEvent(Tempest::MouseEvent&) override {
    up++;
    }
  void mouseMoveEvent(Tempest::MouseEvent&) override {
    move++;
    }

  void clear() {
    down=0;
    up  =0;
    move=0;
    }
  };

TEST(main,EventDispatcher_MouseEvent) {
  Widget wx;
  wx.resize(1000,50);
  wx.setLayout(Vertical);

  EventDispatcher dis(wx);

  TstButton& b0=wx.addWidget(new TstButton());
  TstButton& b1=wx.addWidget(new TstButton());

  auto evt0 = mkMEvent(Event::MouseDown,11,22);
  dis.dispatchMouseDown(wx,evt0);

  auto evt1 = mkMEvent(Event::MouseUp,11,22);
  dis.dispatchMouseUp(wx,evt1);

  EXPECT_EQ(b0.down,1);
  EXPECT_EQ(b0.up,  1);

  EXPECT_EQ(b1.down,0);
  EXPECT_EQ(b1.up,  0);

  b0.clear();
  b1.clear();

  auto evt3 = mkMEvent(Event::MouseDown,11,22);
  dis.dispatchMouseDown(wx,evt0);

  auto evt4 = mkMEvent(Event::MouseMove,999,22);
  dis.dispatchMouseMove(wx,evt4);
  EXPECT_EQ(b0.down,1);
  EXPECT_EQ(b0.up,  0);
  EXPECT_EQ(b0.move,1);

  auto evt5 = mkMEvent(Event::MouseUp,11,22);
  dis.dispatchMouseUp(wx,evt5);
  EXPECT_EQ(b0.down,1);
  EXPECT_EQ(b0.up,  1);
  EXPECT_EQ(b0.move,1);
  }

TEST(main,EventDispatcher_IndependentTouches) {
  struct TouchWidget final:Widget {
    std::vector<int> drags;
    std::vector<int> releases;
    int doubleClicks = 0;

    void mouseDownEvent(MouseEvent&) override {}
    void mouseDoubleClickEvent(MouseEvent& e) override {
      ++doubleClicks;
      Widget::mouseDoubleClickEvent(e);
      }
    void mouseDragEvent(MouseEvent& e) override { drags.push_back(e.mouseID); }
    void mouseUpEvent(MouseEvent& e) override { releases.push_back(e.mouseID); }
    };

  for(int released:{0,1}) {
    TouchWidget widget;
    widget.resize(100,100);
    EventDispatcher dispatcher(widget);
    auto event = [](Event::Type type,int id,int x=20) {
      return MouseEvent(x,20,Event::ButtonLeft,Event::M_NoModifier,0,id,type);
      };
    auto first = event(Event::MouseDown,0);
    auto second = event(Event::MouseDown,1);
    dispatcher.dispatchMouseDown(widget,first);
    dispatcher.dispatchMouseDown(widget,second);
    EXPECT_EQ(widget.doubleClicks,0);

    auto up = event(Event::MouseUp,released);
    dispatcher.dispatchMouseUp(widget,up);
    auto move = event(Event::MouseMove,1-released,200);
    dispatcher.dispatchMouseMove(widget,move);
    auto last = event(Event::MouseUp,1-released,200);
    dispatcher.dispatchMouseUp(widget,last);
    EXPECT_EQ(widget.drags,std::vector<int>({1-released}));
    EXPECT_EQ(widget.releases,std::vector<int>({released,1-released}));
    }
  }
