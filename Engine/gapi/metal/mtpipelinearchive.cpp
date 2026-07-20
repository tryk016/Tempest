#include "mtpipelinearchive.h"

#include <stdexcept>

using namespace Tempest;
using namespace Tempest::Detail;

namespace {

bool isValidUtf8(const char* value) noexcept {
  if(value==nullptr || value[0]=='\0')
    return false;

  const auto* p = reinterpret_cast<const unsigned char*>(value);
  while(*p!=0) {
    if(*p<=0x7f) {
      ++p;
      continue;
      }

    unsigned continuation = 0;
    uint32_t codepoint = 0;
    if((*p&0xe0)==0xc0) {
      continuation = 1;
      codepoint = *p&0x1f;
      if(codepoint<2)
        return false;
      }
    else if((*p&0xf0)==0xe0) {
      continuation = 2;
      codepoint = *p&0x0f;
      }
    else if((*p&0xf8)==0xf0) {
      continuation = 3;
      codepoint = *p&0x07;
      if(codepoint>4)
        return false;
      }
    else {
      return false;
      }
    ++p;
    for(unsigned i=0; i<continuation; ++i,++p) {
      if((*p&0xc0)!=0x80)
        return false;
      codepoint = (codepoint<<6)|(*p&0x3f);
      }
    if((continuation==2 && codepoint<0x800) ||
       (continuation==3 && codepoint<0x10000) ||
       codepoint>0x10ffff ||
       (codepoint>=0xd800 && codepoint<=0xdfff))
      return false;
    }
  return true;
  }

}

std::shared_ptr<const MetalPipelineArchiveConfigOwned>
Tempest::Detail::makeMetalPipelineArchiveConfig(
    const MetalPipelineArchiveConfig& config) {
  if(config.abiVersion!=MetalPipelineArchiveConfig::AbiVersion ||
     config.structSize!=MetalPipelineArchiveConfig::StructSize)
    throw std::invalid_argument("Metal pipeline archive ABI mismatch");
  if(!isValidUtf8(config.archivePath) || config.archivePath[0]!='/')
    throw std::invalid_argument(
        "Metal pipeline archive path must be absolute UTF-8");

  auto owned = std::make_shared<MetalPipelineArchiveConfigOwned>();
  owned->archivePath = config.archivePath;
  return owned;
  }
