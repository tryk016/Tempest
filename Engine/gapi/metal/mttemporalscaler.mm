#if defined(TEMPEST_BUILD_METAL) && defined(TEMPEST_METALFX_TEMPORAL)

#include "mttemporalscaler.h"

#include <Tempest/MetalApi>

#include "mtdevice.h"
#include "mttexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtTemporalScaler::MtTemporalScaler(MtDevice& device, const TemporalScalerDesc& cfg) {
  if(cfg.inputWidth==0 || cfg.inputHeight==0 || cfg.outputWidth==0 || cfg.outputHeight==0)
    return;
  if(!MTLFX::TemporalScalerDescriptor::supportsDevice(device.impl.get()))
    return;

  auto pool = NsPtr<NS::AutoreleasePool>::init();
  auto raw  = MTLFX::TemporalScalerDescriptor::alloc();
  if(raw==nullptr)
    return;
  auto desc = NsPtr<MTLFX::TemporalScalerDescriptor>(raw->init());
  if(desc==nullptr)
    return;

  desc->setColorTextureFormat(nativeFormat(cfg.inputFormat));
  desc->setDepthTextureFormat(nativeFormat(cfg.depthFormat));
  desc->setMotionTextureFormat(nativeFormat(cfg.motionFormat));
  desc->setOutputTextureFormat(nativeFormat(cfg.outputFormat));
  desc->setInputWidth(cfg.inputWidth);
  desc->setInputHeight(cfg.inputHeight);
  desc->setOutputWidth(cfg.outputWidth);
  desc->setOutputHeight(cfg.outputHeight);
  desc->setAutoExposureEnabled(cfg.autoExposure);

  impl = NsPtr<MTLFX::TemporalScaler>(desc->newTemporalScaler(device.impl.get()));
  }

bool MtTemporalScaler::encode(MTL::CommandBuffer& cmd, MtTexture& color, MtTexture& depth,
                              MtTexture& motion, MtTexture& output, const TemporalScalerArgs& args) {
  if(impl==nullptr || color.impl==nullptr || depth.impl==nullptr ||
     motion.impl==nullptr || output.impl==nullptr)
    return false;
  if(color.impl->pixelFormat()!=impl->colorTextureFormat() ||
     depth.impl->pixelFormat()!=impl->depthTextureFormat() ||
     motion.impl->pixelFormat()!=impl->motionTextureFormat() ||
     output.impl->pixelFormat()!=impl->outputTextureFormat())
    return false;
  if(color.impl->width()!=impl->inputWidth() || color.impl->height()!=impl->inputHeight() ||
     depth.impl->width()!=impl->inputWidth() || depth.impl->height()!=impl->inputHeight() ||
     motion.impl->width()!=impl->inputWidth() || motion.impl->height()!=impl->inputHeight() ||
     output.impl->width()!=impl->outputWidth() || output.impl->height()!=impl->outputHeight())
    return false;

  const auto colorUsage  = impl->colorTextureUsage();
  const auto depthUsage  = impl->depthTextureUsage();
  const auto motionUsage = impl->motionTextureUsage();
  const auto outputUsage = impl->outputTextureUsage();
  if((color.impl->usage()&colorUsage)!=colorUsage ||
     (depth.impl->usage()&depthUsage)!=depthUsage ||
     (motion.impl->usage()&motionUsage)!=motionUsage ||
     (output.impl->usage()&outputUsage)!=outputUsage)
    return false;

  impl->setInputContentWidth(color.impl->width());
  impl->setInputContentHeight(color.impl->height());
  impl->setColorTexture(color.impl.get());
  impl->setDepthTexture(depth.impl.get());
  impl->setMotionTexture(motion.impl.get());
  impl->setOutputTexture(output.impl.get());
  impl->setPreExposure(1.f);
  impl->setJitterOffsetX(args.jitterOffsetX);
  impl->setJitterOffsetY(args.jitterOffsetY);
  impl->setMotionVectorScaleX(args.motionVectorScaleX);
  impl->setMotionVectorScaleY(args.motionVectorScaleY);
  impl->setReset(args.resetHistory);
  impl->setDepthReversed(args.depthReversed);
  impl->encodeToCommandBuffer(&cmd);
  return true;
  }

AbstractGraphicsApi::TemporalScaler*
  MetalApi::createTemporalScaler(AbstractGraphicsApi::Device* device, const TemporalScalerDesc& desc) {
  auto& dev = *reinterpret_cast<MtDevice*>(device);
  auto* scaler = new MtTemporalScaler(dev,desc);
  if(!scaler->isValid()) {
    delete scaler;
    return nullptr;
    }
  return scaler;
  }

#endif
