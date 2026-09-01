#include <Tempest/Pixmap>
#include <Tempest/MemWriter>
#include <Tempest/MemReader>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <limits>

using namespace testing;
using namespace Tempest;

TEST(main,PixmapIO_0) {
  Pixmap pm("assets/pixmap_io/rgba.png");
  EXPECT_EQ(pm.w(),     256);
  EXPECT_EQ(pm.h(),     256);
  EXPECT_EQ(pm.format(),TextureFormat::RGBA8);
  }

TEST(main,PixmapIO_1) {
  Pixmap pm("assets/pixmap_io/rgb.jpg");
  EXPECT_EQ(pm.w(),     852);
  EXPECT_EQ(pm.h(),     480);
  EXPECT_EQ(pm.format(),TextureFormat::RGB8);
  }

TEST(main,PixmapIO_SymetricIO) {
  Pixmap pm("assets/pixmap_io/rgba.png");

  static const char* frm[]={"png","jpg","tga","bmp"};
  for(auto f:frm) {
    std::vector<uint8_t> mem;
    MemWriter wr(mem);
    pm.save(wr,f);

    size_t realSz = mem.size();
    mem.push_back(0);
    MemReader rd(mem);
    pm = Pixmap(rd);

    EXPECT_EQ(realSz,rd.cursorPosition());
    }
  }

TEST(main,PixmapConv) {
  Pixmap pm("assets/pixmap_io/dxt5.dds");
  EXPECT_EQ(pm.w(),     512);
  EXPECT_EQ(pm.h(),     512);
  EXPECT_EQ(pm.format(),TextureFormat::DXT5);

  Pixmap px0(pm,TextureFormat::RGB8);
  EXPECT_EQ(px0.format(),TextureFormat::RGB8);
  px0.save("tst-dxt5.png");

  Pixmap px1(px0,TextureFormat::RGBA16);
  EXPECT_EQ(px1.format(),TextureFormat::RGBA16);
  px1.save("tst-dxt5.png");
  }

TEST(main,PixmapAstcMipChain) {
  // ASTC 4x4 stores one 16-byte block per rounded-up 4x4 tile.
  // 8x8 + 4x4 + 2x2 = 64 + 16 + 16 bytes.
  std::vector<uint8_t> data(96,0x5A);
  Pixmap pm(data.data(),data.size(),8,8,3,TextureFormat::ASTC4x4);

  EXPECT_EQ(pm.w(),8);
  EXPECT_EQ(pm.h(),8);
  EXPECT_EQ(pm.mipCount(),3);
  EXPECT_EQ(pm.dataSize(),data.size());
  EXPECT_EQ(pm.format(),TextureFormat::ASTC4x4);
  EXPECT_EQ(Pixmap::blockSizeForFormat(TextureFormat::ASTC4x4),16);
  EXPECT_EQ(Pixmap::blockCount(TextureFormat::ASTC4x4,5,7),Size(2,2));
  EXPECT_TRUE(isCompressedFormat(TextureFormat::ASTC4x4));

  data[0] = 0;
  EXPECT_EQ(reinterpret_cast<const uint8_t*>(pm.data())[0],0x5A);

  // 5x7 + 2x3 + 1x1 has the same 64 + 16 + 16 byte chain and verifies
  // rounded partial blocks across non-power-of-two mips.
  Pixmap odd(data.data(),data.size(),5,7,3,TextureFormat::ASTC4x4);
  EXPECT_EQ(odd.dataSize(),data.size());
  }

TEST(main,PixmapRejectsInvalidMipChain) {
  std::vector<uint8_t> data(96,0);
  EXPECT_THROW(Pixmap(data.data(),data.size()-1,8,8,3,TextureFormat::ASTC4x4),std::system_error);
  EXPECT_THROW(Pixmap(data.data(),data.size(),8,8,5,TextureFormat::ASTC4x4),std::system_error);
  EXPECT_THROW(Pixmap(nullptr,data.size(),8,8,3,TextureFormat::ASTC4x4),std::system_error);
  const auto maxDim = std::numeric_limits<uint32_t>::max();
  EXPECT_THROW(Pixmap(data.data(),data.size(),maxDim,maxDim,1,TextureFormat::RGBA32F),std::system_error);
  }
