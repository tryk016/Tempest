#if defined(TEMPEST_BUILD_METAL)

#include "mtswapchain.h"

#include <Tempest/Application>
#include <Tempest/Except>

#include "mtdevice.h"

#ifdef __OSX__
#import <AppKit/AppKit.h>
#endif

#ifdef __IOS__
#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#endif

#import <QuartzCore/QuartzCore.hpp>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/MTLTexture.h>
#import <Metal/MTLCommandQueue.h>

using namespace Tempest;
using namespace Tempest::Detail;

#ifdef __OSX__
using SysView = NSView;
using SysWindow = NSWindow;
#endif

#ifdef __IOS__
using SysView = UIView;
using SysWindow = UIWindow;
#endif

@class MetalView;

@interface MetalView : SysView
@end

@implementation MetalView
+ (id)layerClass {
  return [CAMetalLayer class];
  }

- (CALayer *)makeBackingLayer {
  return [CAMetalLayer layer];
  }
@end

static PresentFailureKind presentFailureKind(MTL::CommandBufferStatus status,
                                             int64_t nativeCode) noexcept {
  if(status!=MTL::CommandBufferStatusError)
    return PresentFailureKind::UnexpectedStatus;

  switch(MTL::CommandBufferError(nativeCode)) {
    case MTL::CommandBufferErrorTimeout:
      return PresentFailureKind::Timeout;
    case MTL::CommandBufferErrorOutOfMemory:
      return PresentFailureKind::OutOfMemory;
    case MTL::CommandBufferErrorInvalidResource:
      return PresentFailureKind::InvalidResource;
    case MTL::CommandBufferErrorPageFault:
    case MTL::CommandBufferErrorBlacklisted:
    case MTL::CommandBufferErrorNotPermitted:
    case MTL::CommandBufferErrorDeviceRemoved:
      return PresentFailureKind::DeviceLost;
    case MTL::CommandBufferErrorInternal:
    case MTL::CommandBufferErrorMemoryless:
    case MTL::CommandBufferErrorStackOverflow:
      return PresentFailureKind::Internal;
    case MTL::CommandBufferErrorNone:
      return PresentFailureKind::Unknown;
    }
  return PresentFailureKind::Unknown;
  }

struct MtSwapchain::Impl {
  SysWindow* wnd  = nil;
  MetalView* view = nil;
  uint64_t nextPresentSerial = 1;
#if defined(TEMPEST_METAL_DIRECT_DRAWABLE)
  CA::MetalDrawable* drawable = nullptr;
#endif

  CAMetalLayer* metalLayer() {
#if defined(__OSX__)
    return reinterpret_cast<CAMetalLayer*>(wnd.contentView.layer);
#elif defined(__IOS__)
    return reinterpret_cast<CAMetalLayer*>(wnd.rootViewController.view.layer);
#endif
    }
  };

static float backingScaleFactor(SysWindow* w) {
#if defined(__OSX__)
  return [w screen].backingScaleFactor;
#elif defined(__IOS__)
  return [UIScreen mainScreen].scale;
#endif
  }

#if defined(__OSX__)
static NSRect windowRect(NSWindow* wnd) {
  NSRect fr = [wnd contentRectForFrameRect:[wnd frame]];
  fr = [wnd convertRectToBacking:fr];
  return fr;
  }
#elif defined(__IOS__)
static CGRect windowRect(UIWindow* wnd) {
  CGRect  fr    = wnd.rootViewController.view.frame;
  CGFloat scale = wnd.contentScaleFactor;
  // fr = [wnd convertRect:fr fromView:wnd.rootViewController.view];
  
  fr.origin.x    *= scale;
  fr.origin.y    *= scale;
  fr.size.width  *= scale;
  fr.size.height *= scale;
  return fr;
  }
#endif

// note : MoltenVK supports NSView, UIView, CAMetalLayer, so we should align to it
MtSwapchain::MtSwapchain(MtDevice& dev, SystemApi::Window *w)
  :dev(dev), pimpl(new Impl()) {
  NSObject* obj = reinterpret_cast<NSObject*>(w);
  if([obj isKindOfClass : [SysWindow class]])
    pimpl->wnd = reinterpret_cast<SysWindow*>(w);

  const CGRect rect = windowRect(pimpl->wnd);
  sz = {int(rect.size.width), int(rect.size.height)};

  pimpl->view = [[MetalView alloc] initWithFrame:rect];
#if defined(__OSX__)
  pimpl->view.wantsLayer = YES;
  pimpl->wnd.contentView = pimpl->view;
#elif defined(__IOS__)
  pimpl->wnd.rootViewController.view = pimpl->view;
#endif

  CAMetalLayer* lay = pimpl->metalLayer();
  const float dpi = backingScaleFactor(pimpl->wnd);
    
  lay.device = id<MTLDevice>(dev.impl.get());
    
  [lay setContentsScale:dpi];
#if defined(__IOS__)
  // Swapchain takes too much memory on 2GB iPhone
  lay.maximumDrawableCount      = 3;
#endif
  lay.pixelFormat               = MTLPixelFormatBGRA8Unorm;
  lay.allowsNextDrawableTimeout = NO;
#if defined(TEMPEST_METAL_DIRECT_DRAWABLE)
  lay.framebufferOnly           = YES;
#else
  lay.framebufferOnly           = NO;
#endif

  reset();
  }

MtSwapchain::~MtSwapchain() {
#if defined(TEMPEST_METAL_DIRECT_DRAWABLE)
  if(pimpl->drawable!=nullptr)
    pimpl->drawable->release();
#endif
  if(pimpl->view!=nil)
    [pimpl->view release];
  }

void MtSwapchain::reset() {
  dev.waitIdle(); // pending commands
  std::lock_guard<SpinLock> guard(sync);

#if defined(TEMPEST_METAL_DIRECT_DRAWABLE)
  if(pimpl->drawable!=nullptr) {
    pimpl->drawable->release();
    pimpl->drawable = nullptr;
    }
#endif

  // https://developer.apple.com/documentation/quartzcore/cametallayer?language=objc
  CAMetalLayer* lay = pimpl->metalLayer();
  auto wrect = windowRect(pimpl->wnd);
  // auto lrect = lay.frame;
  lay.drawableSize = wrect.size;
  sz       = {int(wrect.size.width), int(wrect.size.height)};
  imgCount = uint32_t(lay.maximumDrawableCount);

  img.resize(imgCount);
  for(size_t i=0; i<imgCount; ++i)
    img[i].tex = nullptr;
#if !defined(TEMPEST_METAL_DIRECT_DRAWABLE)
  for(size_t i=0; i<imgCount; ++i)
    img[i].tex = mkTexture();
#endif

  currentImg = 0;
  }

uint32_t MtSwapchain::currentBackBufferIndex() {
#if defined(TEMPEST_METAL_DIRECT_DRAWABLE)
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  std::lock_guard<SpinLock> guard(sync);

  // Direct-drawable v2 experiment: acquire before encoding so the main render
  // pass targets CAMetalLayer storage instead of a private copy texture.
  if(pimpl->drawable==nullptr) {
    auto* lay = reinterpret_cast<CA::MetalLayer*>(pimpl->metalLayer());
    auto* drawable = lay->nextDrawable();
    if(drawable==nullptr)
      throw SwapchainSuboptimal();

    auto* texture = drawable->texture();
    if(texture->width()!=size_t(sz.w) || texture->height()!=size_t(sz.h))
      throw SwapchainSuboptimal();

    drawable->retain();
    texture->retain();
    pimpl->drawable = drawable;
    img[currentImg].tex = NsPtr<MTL::Texture>(texture);
    }
#endif
  return currentImg;
  }

void MtSwapchain::present() {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  
  uint32_t        i        = currentImg;
#if defined(TEMPEST_METAL_DIRECT_DRAWABLE)
  auto            drawable = pimpl->drawable;
  if(drawable==nullptr || img[i].tex==nullptr)
    throw SwapchainSuboptimal();
#else
  CA::MetalLayer* lay      = reinterpret_cast<CA::MetalLayer*>(pimpl->metalLayer());
  auto            drawable = lay->nextDrawable();
  if(drawable==nullptr)
    throw SwapchainSuboptimal();
#endif
  
  std::lock_guard<SpinLock> guard(sync);
  auto dr = drawable->texture();
  if(dr->width()!=img[i].tex->width() || dr->height()!=img[i].tex->height()) {
    throw SwapchainSuboptimal();
    }
  
  auto desc = NsPtr<MTL::CommandBufferDescriptor>::init();
  //desc->setRetainedReferences(true);
  desc->setErrorOptions(MTL::CommandBufferErrorOptionEncoderExecutionStatus);
  
  auto cmd = dev.queue->commandBuffer(desc.get());
#if !defined(TEMPEST_METAL_DIRECT_DRAWABLE)
  auto enc = cmd->blitCommandEncoder();
  
  enc->copyFromTexture(img[i].tex.get(), 0, 0,
                       dr, 0, 0,
                       1, 1);
  enc->endEncoding();
#endif
  cmd->presentDrawable(drawable);

  const uint64_t presentSerial = pimpl->nextPresentSerial++;
  auto async = dev.asyncState();
  MtAsyncState::SubmissionToken token;
  try {
    token = async->onSubmit();
    cmd->addCompletedHandler(^(MTL::CommandBuffer* c){
      if(!async->beginCompletion(token))
        return;
      PresentFailure failure;
      failure.serial     = presentSerial;
      try {
        const MTL::CommandBufferStatus status = c->status();
        NS::Error* const error = c->error();
        failure.statusCode = int32_t(status);
        failure.nativeCode = error!=nullptr ? int64_t(error->code()) : 0;
        if(status!=MTL::CommandBufferStatusCompleted)
          failure.kind = presentFailureKind(status,failure.nativeCode);
        }
      catch(...) {
        failure.kind = PresentFailureKind::Internal;
        }
#if defined(TEMPEST_METAL_FAULT_ASYNC_PRESENT_AFTER_TERMINAL)
      constexpr bool injectFault = true;
#else
      constexpr bool injectFault = false;
#endif
      async->finishCompletion(token,failure,injectFault);
      });
    cmd->commit();
    }
  catch(...) {
    if(token && async->beginCompletion(token)) {
      PresentFailure failure;
      failure.kind       = PresentFailureKind::Internal;
      failure.serial     = presentSerial;
      async->finishCompletion(token,failure);
      }
    throw;
    }

#if defined(TEMPEST_METAL_DIRECT_DRAWABLE)
  // presentDrawable retains the drawable until presentation completes. Drop
  // the temporary CPU-side references now so CAMetalLayer can recycle it.
  img[i].tex = nullptr;
  pimpl->drawable = nullptr;
  drawable->release();
#endif

  nextDrawable();
  }

NsPtr<MTL::Texture> MtSwapchain::mkTexture() {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  auto desc = NsPtr<MTL::TextureDescriptor>::init();
  if(desc==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  desc->setTextureType(MTL::TextureType2D);
  desc->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  desc->setWidth(sz.w);
  desc->setHeight(sz.h);
  desc->setMipmapLevelCount(1);
  desc->setCpuCacheMode(MTL::CPUCacheModeDefaultCache);
  desc->setStorageMode(MTL::StorageModePrivate);
  desc->setUsage(MTL::TextureUsageRenderTarget);
  desc->setAllowGPUOptimizedContents(true);

  auto impl = NsPtr<MTL::Texture>(dev.impl->newTexture(desc.get()));
  if(impl==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  return impl;
  }

void MtSwapchain::nextDrawable() {
  currentImg = (currentImg+1) % img.size();
  }

uint32_t MtSwapchain::imageCount() const {
  return imgCount;
  }

uint32_t MtSwapchain::w() const {
  return sz.w;
  }

uint32_t MtSwapchain::h() const {
  return sz.h;
  }

MTL::PixelFormat MtSwapchain::format() const {
  CAMetalLayer* lay = pimpl->metalLayer();
  return MTL::PixelFormat(lay.pixelFormat);
  }

#endif
