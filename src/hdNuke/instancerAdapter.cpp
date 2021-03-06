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
#include <pxr/imaging/hd/tokens.h>

#include "instancerAdapter.h"


PXR_NAMESPACE_OPEN_SCOPE


void
HdNukeInstancerAdapter::Update(const GeoInfoVector& geoInfoPtrs)
{
    _instanceXforms.resize(geoInfoPtrs.size());
    for (size_t i = 0; i < geoInfoPtrs.size(); i++)
    {
        const float* matrixPtr = geoInfoPtrs[i]->matrix.array();
        std::copy(matrixPtr, matrixPtr + 16, _instanceXforms[i].data());
    }
}

VtValue
HdNukeInstancerAdapter::Get(const TfToken& key) const
{
    if (key == HdInstancerTokens->instanceTransform) {
        return VtValue(_instanceXforms);
    }
    return VtValue();
}


PXR_NAMESPACE_CLOSE_SCOPE
