// Copyright 2019-present Nathan Rusch
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>

#include "renderStack.h"


PXR_NAMESPACE_OPEN_SCOPE


HydraRenderStack::HydraRenderStack(HdRendererPlugin* pluginPtr)
        : rendererPlugin(pluginPtr),
          primCollection(HdTokens->geometry,
                         HdReprSelector(HdReprTokens->refined))
{
    HdRenderDelegate* renderDelegate = rendererPlugin->CreateRenderDelegate();
    renderIndex = HdRenderIndex::New(renderDelegate);

    nukeDelegate = new HdNukeSceneDelegate(renderIndex);

    static SdfPath taskControllerId("/HydraNuke_TaskController");
    taskController = new HdxTaskController(renderIndex, taskControllerId);

    taskController->SetCollection(primCollection);
}

HydraRenderStack::~HydraRenderStack()
{
    if (taskController != nullptr) {
        delete taskController;
    }

    if (usdDelegate != nullptr) {
        delete usdDelegate;
    }

    if (nukeDelegate != nullptr) {
        delete nukeDelegate;
    }

    HdRenderDelegate* renderDelegate = nullptr;
    if (renderIndex != nullptr) {
        renderDelegate = renderIndex->GetRenderDelegate();
        delete renderIndex;
    }

    if (rendererPlugin != nullptr) {
        if (renderDelegate != nullptr) {
            rendererPlugin->DeleteRenderDelegate(renderDelegate);
        }

        HdRendererPluginRegistry::GetInstance().ReleasePlugin(rendererPlugin);
    }
}

std::vector<HdRenderBuffer*>
HydraRenderStack::GetRenderBuffers() const
{
    std::vector<HdRenderBuffer*> buffers;
    if (renderIndex == nullptr or taskController == nullptr) {
        return buffers;
    }

    auto bprimIds = renderIndex->GetBprimSubtree(
        HdPrimTypeTokens->renderBuffer, taskController->GetControllerId());
    buffers.reserve(bprimIds.size());

    for (const auto& bprimId : bprimIds)
    {
        buffers.push_back(
            static_cast<HdRenderBuffer*>(renderIndex->GetBprim(
                HdPrimTypeTokens->renderBuffer, bprimId)));
    }
    return buffers;
}

/* static */
HydraRenderStack*
HydraRenderStack::Create(TfToken pluginId)
{
    auto& pluginRegistry = HdRendererPluginRegistry::GetInstance();
    if (not pluginRegistry.IsRegisteredPlugin(pluginId)) {
        return nullptr;
    }

    HdRendererPlugin* plugin = pluginRegistry.GetRendererPlugin(pluginId);
    if (not plugin->IsSupported()) {
        pluginRegistry.ReleasePlugin(plugin);
        return nullptr;
    }

    return new HydraRenderStack(plugin);
}

void
HydraRenderStack::loadUSDStage(const char* usdFilePath)
{
    static SdfPath usdDelegateId("/USD_Scene");

    if (usdDelegate != nullptr) {
        renderIndex->RemoveSubtree(usdDelegateId, usdDelegate);
        delete usdDelegate;
        usdDelegate = nullptr;
    }

    if (strlen(usdFilePath) == 0) {
        return;
    }

    if (auto stage = UsdStage::Open(usdFilePath)) {
        // XXX: Basic up-axis correction for sanity
        if (UsdGeomGetStageUpAxis(stage) == UsdGeomTokens->z) {
            if (auto defaultPrim = stage->GetDefaultPrim()) {
                if (defaultPrim.IsA<UsdGeomXformable>()) {
                    stage->SetEditTarget(stage->GetSessionLayer());
                    auto xform = UsdGeomXformable(defaultPrim);
                    auto rotXop = xform.AddRotateXOp(
                        UsdGeomXformOp::PrecisionDouble,
                        TfToken("upAxisCorrection"));
                    rotXop.Set<double>(-90);
                }
            }
        }

        TfTokenVector purposes;
        purposes.push_back(UsdGeomTokens->default_);
        purposes.push_back(UsdGeomTokens->render);

        UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), purposes, true);

        GfBBox3d bbox = bboxCache.ComputeWorldBound(stage->GetPseudoRoot());
        GfRange3d world = bbox.ComputeAlignedRange();

        GfVec3d worldCenter = (world.GetMin() + world.GetMax()) / 2.0;
        double worldSize = world.GetSize().GetLength();

        UsdPrim prim = stage->GetPseudoRoot();

        if (usdDelegate == nullptr) {
            usdDelegate = new UsdImagingDelegate(renderIndex, usdDelegateId);
        }
        usdDelegate->Populate(prim);
    }
}


PXR_NAMESPACE_CLOSE_SCOPE
