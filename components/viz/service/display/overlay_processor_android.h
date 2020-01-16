// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ANDROID_H_

#include "components/viz/service/display/overlay_processor_using_strategy.h"

namespace base {
class WaitableEvent;
}

namespace viz {
class OverlayProcessorOnGpu;

// This class is used on Android for the pre-SurfaceControl case.
// This is an overlay processor for supporting fullscreen video underlays on
// Android. Things are a bit different on Android compared with other platforms.
// By the time a video frame is marked as overlayable it means the video decoder
// was outputting to a Surface that we can't read back from. As a result, the
// overlay must always succeed, or the video won't be visible. This is one of
// the reasons that only fullscreen is supported: we have to be sure that
// nothing will cause the overlay to be rejected, because there's no fallback to
// gl compositing.
class VIZ_SERVICE_EXPORT OverlayProcessorAndroid
    : public OverlayProcessorUsingStrategy {
 public:
  OverlayProcessorAndroid(
      SkiaOutputSurface* skia_output_surface,
      scoped_refptr<gpu::GpuTaskSchedulerHelper> gpu_task_scheduler,
      bool enable_overlay);
  ~OverlayProcessorAndroid() override;

  bool IsOverlaySupported() const override;

  bool NeedsSurfaceOccludingDamageRect() const override;

  // Override OverlayProcessorUsingStrategy.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  void SetViewportSize(const gfx::Size& size) override {}

  void CheckOverlaySupport(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates) override;
  gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const override;

 private:
  // OverlayProcessor needs to send overlay candidate information to the gpu
  // thread. These two methods are scheduled on the gpu thread to setup and
  // teardown the gpu side receiver.
  void InitializeOverlayProcessorOnGpu();
  void DestroyOverlayProcessorOnGpu(base::WaitableEvent* event);
  void NotifyOverlayPromotion(DisplayResourceProvider* resource_provider,
                              const OverlayCandidateList& candidate_list,
                              const QuadList& quad_list) override;

  // [id] == candidate's |display_rect| for all promotable resources.
  using PromotionHintInfoMap = std::map<ResourceId, gfx::RectF>;

  // For android, this provides a set of resources that could be promoted to
  // overlay, if one backs them with a SurfaceView.
  PromotionHintInfoMap promotion_hint_info_map_;

  // Set of resources that have requested a promotion hint that also have quads
  // that use them.
  ResourceIdSet promotion_hint_requestor_set_;

  void NotifyOverlayPromotionUsingSkiaOutputSurface(
      DisplayResourceProvider* resource_provider,
      const OverlayCandidateList& candidate_list);

  SkiaOutputSurface* const skia_output_surface_;
  scoped_refptr<gpu::GpuTaskSchedulerHelper> gpu_task_scheduler_;
  const bool overlay_enabled_;
  // This class is created, accessed, and destroyed on the gpu thread.
  std::unique_ptr<OverlayProcessorOnGpu> processor_on_gpu_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ANDROID_H_
