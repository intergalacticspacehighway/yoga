/*
* Copyright (c) Meta Platforms, Inc. and affiliates.
*
* This source code is licensed under the MIT license found in the
* LICENSE file in the root directory of this source tree.
*/

#pragma once

#include <yoga/event/event.h>
#include <yoga/node/Node.h>

namespace facebook::yoga {
// Returns if some absolute descendant has new layout
bool layoutAbsoluteDescendantsGrid(
    yoga::Node* containingNode,
    yoga::Node* currentNode,
    SizingMode widthSizingMode,
    Direction currentNodeDirection,
    LayoutData& layoutMarkerData,
    uint32_t currentDepth,
    uint32_t generationCount,
    float currentNodeMainOffsetFromContainingBlock,
    float currentNodeCrossOffsetFromContainingBlock,
    float containingNodeAvailableInnerWidth,
    float containingNodeAvailableInnerHeight,
    const std::vector<GridTrackSize>& columnTracks,
    const std::vector<GridTrackSize>& rowTracks,
    int32_t minColumnStart,
    int32_t minRowStart);

} // namespace facebook::yoga
