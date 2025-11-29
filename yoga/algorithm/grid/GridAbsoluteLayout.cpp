/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <yoga/algorithm/Align.h>
#include <yoga/algorithm/BoundAxis.h>
#include <yoga/algorithm/CalculateLayout.h>
#include <yoga/algorithm/TrailingPosition.h>
#include <yoga/algorithm/grid/AutoPlacement.h>

namespace facebook::yoga {

static inline void setFlexStartLayoutPosition(
    const yoga::Node* const parent,
    yoga::Node* child,
    const Direction direction,
    const FlexDirection axis,
    const float containingBlockWidth, 
    const float additionalStartOffset) {
  float position = child->style().computeFlexStartMargin(
                      axis, direction, containingBlockWidth) +
      parent->getLayout().border(flexStartEdge(axis)) + additionalStartOffset;

  child->setLayoutPosition(position, flexStartEdge(axis));
}

static inline void setFlexEndLayoutPosition(
    const yoga::Node* const parent,
    yoga::Node* child,
    const Direction direction,
    const FlexDirection axis,
    const float containingBlockWidth,
    const float additionalStartOffset) {
  float flexEndPosition = parent->getLayout().border(flexEndEdge(axis)) +
      child->style().computeFlexEndMargin(
          axis, direction, containingBlockWidth);

  child->setLayoutPosition(
      getPositionOfOppositeEdge(flexEndPosition, axis, parent, child),
      flexStartEdge(axis));
}

static inline void setCenterLayoutPosition(
    const yoga::Node* const parent,
    yoga::Node* child,
    const Direction direction,
    const FlexDirection axis,
    const float containingBlockWidth,
    const float containingBlockSize,
    const float additionalStartOffset) {

  const float childOuterSize =
      child->getLayout().measuredDimension(dimension(axis)) +
      child->style().computeMarginForAxis(axis, containingBlockWidth);

  float position = (containingBlockSize - childOuterSize) / 2.0f +
      parent->getLayout().border(flexStartEdge(axis)) +
      child->style().computeFlexStartMargin(
          axis, direction, containingBlockWidth) 
          + additionalStartOffset;

  child->setLayoutPosition(position, flexStartEdge(axis));
}

static void justifyAbsoluteChild(
    const yoga::Node* const parent,
    yoga::Node* child,
    const Direction direction,
    const FlexDirection mainAxis,
    const float containingBlockWidth,
    const float containingBlockSize,
    const float additionalStartOffset) {
  const Justify parentJustifyContent = parent->style().justifyContent();
  switch (parentJustifyContent) {
    case Justify::Start:
    case Justify::End:
    case Justify::Auto:
    break;
    case Justify::Stretch:
    // No-Op
    break;
    case Justify::FlexStart:
    case Justify::SpaceBetween:
      setFlexStartLayoutPosition(
          parent, child, direction, mainAxis, containingBlockWidth, additionalStartOffset);
      break;
    case Justify::FlexEnd:
      setFlexEndLayoutPosition(
          parent, child, direction, mainAxis, containingBlockWidth, additionalStartOffset);
      break;
    case Justify::Center:
    case Justify::SpaceAround:
    case Justify::SpaceEvenly:
      setCenterLayoutPosition(
          parent, child, direction, mainAxis, containingBlockWidth, containingBlockSize, additionalStartOffset);
      break;
  }
}

static void alignAbsoluteChild(
    const yoga::Node* const parent,
    yoga::Node* child,
    const Direction direction,
    const FlexDirection crossAxis,
    const float containingBlockWidth,
    const float containingBlockSize,
    const float additionalStartOffset) {
  Align itemAlign = resolveChildAlignment(parent, child);
  const Wrap parentWrap = parent->style().flexWrap();
  if (parentWrap == Wrap::WrapReverse) {
    if (itemAlign == Align::FlexEnd) {
      itemAlign = Align::FlexStart;
    } else if (itemAlign != Align::Center) {
      itemAlign = Align::FlexEnd;
    }
  }

  switch (itemAlign) {
    case Align::Start:
    case Align::End:
    case Align::Auto:
    case Align::FlexStart:
    case Align::Baseline:
    case Align::SpaceAround:
    case Align::SpaceBetween:
    case Align::Stretch:
    case Align::SpaceEvenly:
      setFlexStartLayoutPosition(
          parent, child, direction, crossAxis, containingBlockWidth, additionalStartOffset);
      break;
    case Align::FlexEnd:
      setFlexEndLayoutPosition(
          parent, child, direction, crossAxis, containingBlockWidth, additionalStartOffset);
      break;
    case Align::Center:
      setCenterLayoutPosition(
          parent, child, direction, crossAxis, containingBlockWidth, containingBlockSize, additionalStartOffset);
      break;
  }
}

/*
* Absolutely positioned nodes do not participate in flex layout and thus their
* positions can be determined independently from the rest of their siblings.
* For each axis there are essentially two cases:
*
* 1) The node has insets defined. In this case we can just use these to
*    determine the position of the node.
* 2) The node does not have insets defined. In this case we look at the style
*    of the parent to position the node. Things like justify content and
*    align content will move absolute children around. If none of these
*    special properties are defined, the child is positioned at the start
*    (defined by flex direction) of the leading flex line.
*
* This function does that positioning for the given axis. The spec has more
* information on this topic: https://www.w3.org/TR/css-flexbox-1/#abspos-items
*/
static void positionAbsoluteChild(
    const yoga::Node* const containingNode,
    const yoga::Node* const parent,
    yoga::Node* child,
    const Direction direction,
    const Dimension dimension,
    const float containingBlockWidth,
    const float containingBlockHeight,
    const float additionalStartOffset) {
  const float containingBlockSize = dimension == Dimension::Width ? containingBlockWidth : containingBlockHeight;

  // The inline-start position takes priority over the end position in the case
  // that they are both set and the node has a fixed width. Thus we only have 2
  // cases here: if inline-start is defined and if inline-end is defined.
  //
  // Despite checking inline-start to honor prioritization of insets, we write
  // to the flex-start edge because this algorithm works by positioning on the
  // flex-start edge and then filling in the flex-end direction at the end if
  // necessary.
  auto axis = dimension == Dimension::Width ? FlexDirection::Row : FlexDirection::Column;
  if (child->style().isInlineStartPositionDefined(axis, direction) &&
      !child->style().isInlineStartPositionAuto(axis, direction)) {
    const float positionRelativeToInlineStart =
        child->style().computeInlineStartPosition(
            axis, direction, containingBlockSize) + additionalStartOffset +
        containingNode->style().computeInlineStartBorder(axis, direction) +
        child->style().computeInlineStartMargin(
            axis, direction, containingBlockSize);
    const float positionRelativeToFlexStart =
        inlineStartEdge(axis, direction) != flexStartEdge(axis)
        ? getPositionOfOppositeEdge(
              positionRelativeToInlineStart, axis, containingNode, child)
        : positionRelativeToInlineStart;
    child->setLayoutPosition(positionRelativeToFlexStart, flexStartEdge(axis));
  } else if (
      child->style().isInlineEndPositionDefined(axis, direction) &&
      !child->style().isInlineEndPositionAuto(axis, direction)) {
    const float positionRelativeToInlineStart =
        containingNode->getLayout().measuredDimension(dimension) -
        additionalStartOffset -
        child->getLayout().measuredDimension(dimension) -
        containingNode->style().computeInlineEndBorder(axis, direction) -
        child->style().computeInlineEndMargin(
            axis, direction, containingBlockSize) -
        child->style().computeInlineEndPosition(
            axis, direction, containingBlockSize);
    const float positionRelativeToFlexStart =
        inlineStartEdge(axis, direction) != flexStartEdge(axis)
        ? getPositionOfOppositeEdge(
              positionRelativeToInlineStart, axis, containingNode, child)
        : positionRelativeToInlineStart;

    child->setLayoutPosition(positionRelativeToFlexStart, flexStartEdge(axis));
  } else {
    axis == FlexDirection::Row ? justifyAbsoluteChild(
                    parent, child, direction, axis, containingBlockWidth, containingBlockWidth, additionalStartOffset)
              : alignAbsoluteChild(
                    parent, child, direction, axis, containingBlockWidth, containingBlockHeight, additionalStartOffset);
  }
}

static std::pair<float, float> calculateGridItemArea(
  Dimension dimension,
  const yoga::Node* const containingNode,
  const yoga::Node* const child,
  int32_t minTrackStart,
  const std::vector<GridTrackSize>& gridTracks) {
    auto explicitLineCount = dimension == Dimension::Width ? 
      containingNode->style().gridTemplateColumns().size() + 1 
      : containingNode->style().gridTemplateRows().size() + 1;
    auto containingNodeSize = dimension == Dimension::Width ? 
      containingNode->getLayout().measuredDimension(Dimension::Width) : containingNode->getLayout().measuredDimension(Dimension::Height);
    auto itemTrackStart = dimension == Dimension::Width ? 
      child->style().gridColumnStart() : child->style().gridRowStart();
    auto itemTrackEnd = dimension == Dimension::Width ? 
      child->style().gridColumnEnd() : child->style().gridRowEnd();
    auto gap = dimension == Dimension::Width ? 
      containingNode->style().computeGapForDimension(Dimension::Width, containingNodeSize)
      : containingNode->style().computeGapForDimension(Dimension::Height, containingNodeSize);
    
    auto itemTrackPlacement = GridItemTrackPlacement::resolveLinePlacement(itemTrackStart, itemTrackEnd, explicitLineCount);
    itemTrackPlacement.start -= minTrackStart;
    itemTrackPlacement.end -= minTrackStart;
    
    if (itemTrackStart.type == GridLineType::Integer || itemTrackEnd.type == GridLineType::Integer) {
      auto itemStartOffset = 0.0f;
      auto itemAreaSize = 0.0f;
      for (size_t i = 0; i < itemTrackPlacement.start && i < gridTracks.size(); i++) {
        itemStartOffset += gridTracks[i].baseSize;
        if (i < itemTrackPlacement.start - 1) {
          itemStartOffset += gap;
        }
      }

      for (size_t i = itemTrackPlacement.start; i < itemTrackPlacement.end && i < gridTracks.size(); i++) {
        itemAreaSize += gridTracks[i].baseSize;
        if (i < itemTrackPlacement.end - 1) {
          itemAreaSize += gap;
        }
      }
      return {itemStartOffset, itemAreaSize};
    }

    return {YGUndefined, YGUndefined};
  }

static void layoutAbsoluteChild(
    const yoga::Node* const containingNode,
    const yoga::Node* const node,
    yoga::Node* const child,
    const float containingBlockWidth,
    const float containingBlockHeight,
    const SizingMode widthMode,
    const Direction direction,
    LayoutData& layoutMarkerData,
    const uint32_t depth,
    const uint32_t generationCount,
    const std::vector<GridTrackSize>& columnTracks,
    const std::vector<GridTrackSize>& rowTracks,
    int32_t minColumnStart,
    int32_t minRowStart) {

  // If the child is grid item of display:grid containing node, then containing block could be the area defined by the grid-[column|row]-[start|end] properties.
  // In such cases, the containing block width and height will be the area defined by the grid-[column|row]-[start|end] properties.
  auto finalContainingBlockWidth = containingBlockWidth;
  auto finalContainingBlockHeight = containingBlockHeight;
  auto additionalLeftOffset = 0.0f;
  auto additionalTopOffset = 0.0f;

  if (containingNode->style().display() == Display::Grid) {
      auto [itemAreaLeftOffset, itemAreaWidth] = calculateGridItemArea(Dimension::Width, containingNode, child, minColumnStart, columnTracks);
      auto [itemAreaTopOffset, itemAreaHeight] = calculateGridItemArea(Dimension::Height, containingNode, child, minRowStart, rowTracks);
      
      if (yoga::isDefined(itemAreaLeftOffset) && yoga::isDefined(itemAreaWidth)) {
        finalContainingBlockWidth = itemAreaWidth;
        additionalLeftOffset = itemAreaLeftOffset + containingNode->getLayout().padding(PhysicalEdge::Left);
      }
      if (yoga::isDefined(itemAreaTopOffset) && yoga::isDefined(itemAreaHeight)) {
        finalContainingBlockHeight = itemAreaHeight;
        additionalTopOffset = itemAreaTopOffset + containingNode->getLayout().padding(PhysicalEdge::Top);
      }
  }

  float childWidth = YGUndefined;
  float childHeight = YGUndefined;
  SizingMode childWidthSizingMode = SizingMode::MaxContent;
  SizingMode childHeightSizingMode = SizingMode::MaxContent;

  auto marginRow = child->style().computeMarginForAxis(
      FlexDirection::Row, finalContainingBlockWidth);
  auto marginColumn = child->style().computeMarginForAxis(
      FlexDirection::Column, finalContainingBlockWidth);

  if (child->hasDefiniteLength(Dimension::Width, finalContainingBlockWidth)) {
    childWidth = child
                    ->getResolvedDimension(
                        direction,
                        Dimension::Width,
                        finalContainingBlockWidth,
                        finalContainingBlockWidth)
                    .unwrap() +
        marginRow;
  } else {
    // If the child doesn't have a specified width, compute the width based on
    // the left/right offsets if they're defined.
    if (child->style().isFlexStartPositionDefined(
            FlexDirection::Row, direction) &&
        child->style().isFlexEndPositionDefined(
            FlexDirection::Row, direction) &&
        !child->style().isFlexStartPositionAuto(
            FlexDirection::Row, direction) &&
        !child->style().isFlexEndPositionAuto(FlexDirection::Row, direction)) {
      childWidth =
          finalContainingBlockWidth -
          (child->style().computeFlexStartPosition(
              FlexDirection::Row, direction, finalContainingBlockWidth) +
          child->style().computeFlexEndPosition(
              FlexDirection::Row, direction, finalContainingBlockWidth));
      childWidth = boundAxis(
          child,
          FlexDirection::Row,
          direction,
          childWidth,
          finalContainingBlockWidth,
          finalContainingBlockWidth);
    }
  }

  if (child->hasDefiniteLength(Dimension::Height, finalContainingBlockHeight)) {
    childHeight = child
                      ->getResolvedDimension(
                          direction,
                          Dimension::Height,
                          finalContainingBlockHeight,
                          finalContainingBlockWidth)
                      .unwrap() +
        marginColumn;
  } else {
    // If the child doesn't have a specified height, compute the height based
    // on the top/bottom offsets if they're defined.
    if (child->style().isFlexStartPositionDefined(
            FlexDirection::Column, direction) &&
        child->style().isFlexEndPositionDefined(
            FlexDirection::Column, direction) &&
        !child->style().isFlexStartPositionAuto(
            FlexDirection::Column, direction) &&
        !child->style().isFlexEndPositionAuto(
            FlexDirection::Column, direction)) {
      childHeight =
          finalContainingBlockHeight -
          (child->style().computeFlexStartPosition(
              FlexDirection::Column, direction, finalContainingBlockHeight) +
          child->style().computeFlexEndPosition(
              FlexDirection::Column, direction, finalContainingBlockHeight));
      childHeight = boundAxis(
          child,
          FlexDirection::Column,
          direction,
          childHeight,
          finalContainingBlockHeight,
          finalContainingBlockWidth);
    }
  }

  // Exactly one dimension needs to be defined for us to be able to do aspect
  // ratio calculation. One dimension being the anchor and the other being
  // flexible.
  const auto& childStyle = child->style();
  if (yoga::isUndefined(childWidth) ^ yoga::isUndefined(childHeight)) {
    if (childStyle.aspectRatio().isDefined()) {
      if (yoga::isUndefined(childWidth)) {
        childWidth = marginRow +
            (childHeight - marginColumn) * childStyle.aspectRatio().unwrap();
      } else if (yoga::isUndefined(childHeight)) {
        childHeight = marginColumn +
            (childWidth - marginRow) / childStyle.aspectRatio().unwrap();
      }
    }
  }

  // If we're still missing one or the other dimension, measure the content.
  if (yoga::isUndefined(childWidth) || yoga::isUndefined(childHeight)) {
    childWidthSizingMode = yoga::isUndefined(childWidth)
        ? SizingMode::MaxContent
        : SizingMode::StretchFit;
    childHeightSizingMode = yoga::isUndefined(childHeight)
        ? SizingMode::MaxContent
        : SizingMode::StretchFit;

    // If the size of the owner is defined then try to constrain the absolute
    // child to that size as well. This allows text within the absolute child
    // to wrap to the size of its owner. This is the same behavior as many
    // browsers implement.
    if (yoga::isUndefined(childWidth) &&
        widthMode != SizingMode::MaxContent &&
        yoga::isDefined(finalContainingBlockWidth) && finalContainingBlockWidth > 0) {
      childWidth = finalContainingBlockWidth;
      childWidthSizingMode = SizingMode::FitContent;
    }

    calculateLayoutInternal(
        child,
        childWidth,
        childHeight,
        direction,
        childWidthSizingMode,
        childHeightSizingMode,
        finalContainingBlockWidth,
        finalContainingBlockHeight,
        false,
        LayoutPassReason::kAbsMeasureChild,
        layoutMarkerData,
        depth,
        generationCount);
    childWidth = child->getLayout().measuredDimension(Dimension::Width) +
        child->style().computeMarginForAxis(
            FlexDirection::Row, finalContainingBlockWidth);
    childHeight = child->getLayout().measuredDimension(Dimension::Height) +
        child->style().computeMarginForAxis(
            FlexDirection::Column, finalContainingBlockWidth);
  }

  calculateLayoutInternal(
      child,
      childWidth,
      childHeight,
      direction,
      SizingMode::StretchFit,
      SizingMode::StretchFit,
      finalContainingBlockWidth,
      finalContainingBlockHeight,
      true,
      LayoutPassReason::kAbsLayout,
      layoutMarkerData,
      depth,
      generationCount);

  positionAbsoluteChild(
      containingNode,
      node,
      child,
      direction,
      Dimension::Width,
      finalContainingBlockWidth,
      finalContainingBlockHeight,
      additionalLeftOffset);
  positionAbsoluteChild(
      containingNode,
      node,
      child,
      direction,
      Dimension::Height,
      finalContainingBlockWidth,
      finalContainingBlockHeight,
      additionalTopOffset);
}

bool layoutAbsoluteDescendantsGrid(
    yoga::Node* containingNode,
    yoga::Node* currentNode,
    SizingMode widthSizingMode,
    Direction currentNodeDirection,
    LayoutData& layoutMarkerData,
    uint32_t currentDepth,
    uint32_t generationCount,
    float currentNodeLeftOffsetFromContainingBlock,
    float currentNodeTopOffsetFromContainingBlock,
    float containingNodeAvailableInnerWidth,
    float containingNodeAvailableInnerHeight,
    const std::vector<GridTrackSize>& columnTracks,
    const std::vector<GridTrackSize>& rowTracks,
    int32_t minColumnStart,
    int32_t minRowStart) {
  bool hasNewLayout = false;
  for (auto child : currentNode->getLayoutChildren()) {
    
    if (child->style().display() == Display::None) {
      continue;
    } else if (child->style().positionType() == PositionType::Absolute) {
      const bool absoluteErrata =
          currentNode->hasErrata(Errata::AbsolutePercentAgainstInnerSize);
      const float containingBlockWidth = absoluteErrata
          ? containingNodeAvailableInnerWidth
          : containingNode->getLayout().measuredDimension(Dimension::Width) -
              containingNode->style().computeBorderForAxis(FlexDirection::Row);
      const float containingBlockHeight = absoluteErrata
          ? containingNodeAvailableInnerHeight
          : containingNode->getLayout().measuredDimension(Dimension::Height) -
              containingNode->style().computeBorderForAxis(
                  FlexDirection::Column);

      layoutAbsoluteChild(
          containingNode,
          currentNode,
          child,
          containingBlockWidth,
          containingBlockHeight,
          widthSizingMode,
          currentNodeDirection,
          layoutMarkerData,
          currentDepth,
          generationCount,
          columnTracks,
          rowTracks,
          minColumnStart,
          minRowStart);

      hasNewLayout = hasNewLayout || child->getHasNewLayout();

      /*
      * At this point we know the left and top physical edges of the child are
      * set with positions that are relative to the containing block if insets
      * are defined
      */
      const float childLeftPosition =
          child->getLayout().position(PhysicalEdge::Left);
      const float childTopPosition =
          child->getLayout().position(PhysicalEdge::Top);

      const float childLeftOffsetFromParent =
          child->style().horizontalInsetsDefined()
          ? (childLeftPosition - currentNodeLeftOffsetFromContainingBlock)
          : childLeftPosition;
      const float childTopOffsetFromParent =
          child->style().verticalInsetsDefined()
          ? (childTopPosition - currentNodeTopOffsetFromContainingBlock)
          : childTopPosition;

      child->setLayoutPosition(childLeftOffsetFromParent, PhysicalEdge::Left);
      child->setLayoutPosition(childTopOffsetFromParent, PhysicalEdge::Top);
    } else if (
        child->style().positionType() == PositionType::Static &&
        !child->alwaysFormsContainingBlock()) {
      // We may write new layout results for absolute descendants of "child"
      // which are positioned relative to the current containing block instead
      // of their parent. "child" may not be dirty, or have new constraints, so
      // absolute positioning may be the first time during this layout pass that
      // we need to mutate these descendents. Make sure the path of
      // nodes to them is mutable before positioning.
      child->cloneChildrenIfNeeded();
      const Direction childDirection =
          child->resolveDirection(currentNodeDirection);
      // By now all descendants of the containing block that are not absolute
      // will have their positions set for left and top.
      const float childLeftOffsetFromContainingBlock =
          currentNodeLeftOffsetFromContainingBlock +
          child->getLayout().position(PhysicalEdge::Left);
      const float childTopOffsetFromContainingBlock =
          currentNodeTopOffsetFromContainingBlock +
          child->getLayout().position(PhysicalEdge::Top);

      hasNewLayout = layoutAbsoluteDescendantsGrid(
                        containingNode,
                        child,
                        widthSizingMode,
                        childDirection,
                        layoutMarkerData,
                        currentDepth + 1,
                        generationCount,
                        childLeftOffsetFromContainingBlock,
                        childTopOffsetFromContainingBlock,
                        containingNodeAvailableInnerWidth,
                        containingNodeAvailableInnerHeight,
                        columnTracks,
                        rowTracks,
                        minColumnStart,
                        minRowStart) ||
          hasNewLayout;

      if (hasNewLayout) {
        child->setHasNewLayout(hasNewLayout);
      }
    }
  }
  return hasNewLayout;
}
} // namespace facebook::yoga
