#include <Tempest/TextModel>

#include <gtest/gtest.h>

#include <array>
#include <string_view>

using namespace Tempest;

TEST(main,TextModelBoundedStringView) {
  std::array<char,5> raw = {{'x','a','b','c','y'}};
  TextModel text(std::string_view(raw.data()+1,3));
  EXPECT_EQ(text.size(),3u);
  EXPECT_STREQ(text.c_str(),"abc");

  text.setText(std::string_view(raw.data()+2,1));
  EXPECT_EQ(text.size(),1u);
  EXPECT_STREQ(text.c_str(),"b");

  text.setText(std::string_view(raw.data()+1,0));
  EXPECT_TRUE(text.isEmpty());
  EXPECT_STREQ(text.c_str(),"");
  }

TEST(main,TextModelBoundedShortCommands) {
  std::array<char,4> raw = {{'x','a','b','y'}};
  TextModel text("z");

  TextModel::CommandInsert insertOne(
      std::string_view(raw.data()+1,1),text.charAt(0));
  insertOne.redo(text);
  EXPECT_STREQ(text.c_str(),"az");
  insertOne.undo(text);
  EXPECT_STREQ(text.c_str(),"z");

  TextModel::CommandInsert insertTwo(
      std::string_view(raw.data()+1,2),text.charAt(1));
  insertTwo.redo(text);
  EXPECT_STREQ(text.c_str(),"zab");
  insertTwo.undo(text);
  EXPECT_STREQ(text.c_str(),"z");

  TextModel::CommandReplace replaceEmpty(
      std::string_view(raw.data()+1,0),text.charAt(0),text.charAt(1));
  replaceEmpty.redo(text);
  EXPECT_STREQ(text.c_str(),"");
  replaceEmpty.undo(text);
  EXPECT_STREQ(text.c_str(),"z");

  TextModel::CommandReplace replaceOne(
      std::string_view(raw.data()+1,1),text.charAt(0),text.charAt(1));
  replaceOne.redo(text);
  EXPECT_STREQ(text.c_str(),"a");
  replaceOne.undo(text);
  EXPECT_STREQ(text.c_str(),"z");

  TextModel::CommandReplace replaceTwo(
      std::string_view(raw.data()+1,2),text.charAt(0),text.charAt(1));
  replaceTwo.redo(text);
  EXPECT_STREQ(text.c_str(),"ab");
  replaceTwo.undo(text);
  EXPECT_STREQ(text.c_str(),"z");

  TextModel::CommandInsert insertEmpty(
      std::string_view(raw.data()+1,0),text.charAt(0));
  insertEmpty.redo(text);
  EXPECT_STREQ(text.c_str(),"z");
  insertEmpty.undo(text);
  EXPECT_STREQ(text.c_str(),"z");
  }
