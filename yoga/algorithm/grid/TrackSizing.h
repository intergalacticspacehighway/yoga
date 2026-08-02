/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <yoga/algorithm/Align.h>
#include <yoga/algorithm/Baseline.h>
#include <yoga/algorithm/BoundAxis.h>
#include <yoga/algorithm/CalculateLayout.h>
#include <yoga/algorithm/grid/GridLayout.h>
#include <yoga/numeric/Comparison.h>
#include <yoga/style/StyleSizeLength.h>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace facebook::yoga {
struct TrackSizing {
  enum class AffectedSize { BaseSize, GrowthLimit };

  struct ContentDistribution {
    float startOffset = 0.0f;
    float betweenTracksOffset = 0.0f;
    float effectiveGap = 0.0f;
  };

  struct ItemConstraint {
    float width;
    float height;
    SizingMode widthSizingMode;
    SizingMode heightSizingMode;
    float containingBlockWidth;
    float containingBlockHeight;
  };

  using CrossDimensionEstimator = std::function<float(const GridItem&)>;

  struct ItemSizeContribution {
    const GridItem* item;
    std::vector<GridTrack*> affectedTracks;
    float sizeContribution;

    ItemSizeContribution(
        const GridItem* item,
        const std::vector<GridTrack*>& affectedTracks,
        float sizeContribution)
        : item(item),
          affectedTracks(affectedTracks),
          sizeContribution(sizeContribution) {}
  };

  TrackSizing(
      yoga::Node* node,
      std::vector<GridTrack>& columnTracks,
      std::vector<GridTrack>& rowTracks,
      float containerInnerWidth,
      float containerInnerHeight,
      std::vector<GridItem>& gridItems,
      SizingMode widthSizingMode,
      SizingMode heightSizingMode,
      Direction direction,
      float ownerWidth,
      float ownerHeight,
      LayoutData& layoutMarkerData,
      uint32_t depth,
      uint32_t generationCount,
      BaselineItemGroups& baselineItemGroups);

  // 11.1. Grid Sizing Algorithm
  // https://www.w3.org/TR/css-grid-1/#algo-grid-sizing
  void runGridSizingAlgorithm();

  // Results consumed by GridLayout once sizing has run.
  float getTotalBaseSize(Dimension dimension);
  bool hasPercentageTracks(Dimension dimension) const;
  ContentDistribution calculateContentDistribution(
      Dimension dimension,
      float freeSpace);
  ItemConstraint calculateItemConstraints(
      const GridItem& item,
      float containingBlockWidth,
      float containingBlockHeight);

 private:
  Node* node;
  std::vector<GridTrack>&
      columnTracks; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  std::vector<GridTrack>&
      rowTracks; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  float containerInnerWidth;
  float containerInnerHeight;
  std::vector<GridItem>&
      gridItems; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  SizingMode widthSizingMode;
  SizingMode heightSizingMode;
  Direction direction;
  float ownerWidth;
  float ownerHeight;
  LayoutData&
      layoutMarkerData; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  uint32_t depth;
  uint32_t generationCount;
  CrossDimensionEstimator crossDimensionEstimator;

  // Summary of one dimension's track list, used to skip steps which cannot
  // change anything. These must stay keyed by dimension: the column pass and
  // the row pass have different answers, and step 3 of the grid sizing
  // algorithm asks about the columns after the row pass has already run.
  struct TrackFlags {
    bool hasPercentageTracks = false;
    bool hasOnlyFixedTracks = false;
    bool hasIntrinsicTracks = false;
    bool hasFlexibleTracks = false;
  };
  TrackFlags columnTrackFlags;
  TrackFlags rowTrackFlags;

  // Pre-computed baseline sharing groups
  BaselineItemGroups&
      baselineItemGroups; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

  const TrackFlags& trackFlags(Dimension dimension) const;

  // 11.3. Track Sizing Algorithm, run once per axis
  void runTrackSizing(
      Dimension dimension,
      CrossDimensionEstimator estimator = nullptr);
  void initializeTrackSizes(Dimension dimension);
  void resolveIntrinsicTrackSizes(Dimension dimension);
  void maximizeTrackSizes(Dimension dimension);
  void expandFlexibleTracks(Dimension dimension);
  void stretchAutoTracks(Dimension dimension);

  // 11.5. Resolve intrinsic sizes
  void shimBaselineAlignedItems();
  void accomodateSpanningItemsCrossingContentSizedTracks(Dimension dimension);
  void accomodateSpanningItemsCrossingFlexibleTracks(Dimension dimension);
  void distributeExtraSpaceAcrossSpannedTracks(
      Dimension dimension,
      std::vector<ItemSizeContribution>& gridItemSizeContributions,
      AffectedSize affectedSizeType);
  void distributeSpaceToFlexibleTracksForItems(
      Dimension dimension,
      const std::vector<ItemSizeContribution>& gridItemSizeContributions);

  // Free space distribution for 11.6 - 11.8
  void distributeFreeSpaceToTracks(
      Dimension dimension,
      float targetAvailableSize);
  float findFrSize(
      Dimension dimension,
      size_t startIndex,
      size_t endIndex,
      float spaceToFill,
      const std::unordered_set<GridTrack*>& nonFlexibleTracks);
  float calculateFreeSpace(Dimension dimension);

  // Measuring items and their size contributions
  float measureItem(
      const GridItem& item,
      Dimension dimension,
      const ItemConstraint& constraints);
  // TODO: Yoga does not support min-content constraint yet so we use the
  // max-content size contributions here
  float minContentContribution(
      const GridItem& item,
      Dimension dimension,
      const ItemConstraint& itemConstraints);
  float maxContentContribution(
      const GridItem& item,
      Dimension dimension,
      const ItemConstraint& itemConstraints);
  float minimumContribution(
      const GridItem& item,
      Dimension dimension,
      const ItemConstraint& itemConstraints);
  float automaticMinimumSize(
      const GridItem& item,
      Dimension dimension,
      const ItemConstraint& itemConstraints,
      const std::vector<GridTrack>& tracks,
      float containerSize);
  float contentBasedMinimum(
      const GridItem& item,
      Dimension dimension,
      const ItemConstraint& itemConstraints,
      const std::vector<GridTrack>& tracks,
      float containerSize);
  float limitedMinContentContribution(
      const GridItem& item,
      Dimension dimension,
      const ItemConstraint& itemConstraints);
  float computeFixedTracksLimit(
      const GridItem& item,
      Dimension dimension,
      const std::vector<GridTrack>& tracks,
      float containerSize);
  ItemConstraint calculateItemConstraints(
      const GridItem& item,
      Dimension dimension);
  float estimateCrossDimension(const GridItem& item) const;

  // Classifying sizing functions
  static bool isFixedSizingFunction(
      const StyleSizeLength& sizingFunction,
      float referenceLength);
  static bool isIntrinsicSizingFunction(
      const StyleSizeLength& sizingFunction,
      float referenceLength);
  static bool isAutoSizingFunction(
      const StyleSizeLength& sizingFunction,
      float referenceLength);
  static bool isFlexibleSizingFunction(const StyleSizeLength& sizingFunction);
  static bool isPercentageSizingFunction(
      const StyleSizeLength& sizingFunction);

  // Estimating the axis not currently being sized
  void computeItemTrackCrossingFlags();
  bool itemSizeDependsOnIntrinsicTracks(const GridItem& item) const;
  bool contributionsChanged(
      Dimension dimension,
      CrossDimensionEstimator estimatorBefore,
      CrossDimensionEstimator estimatorAfter);
  float calculateEffectiveRowGapForEstimation();
  float calculateEffectiveGapFromBaseSizes(Dimension dimension);
  CrossDimensionEstimator makeRowHeightEstimatorUsingFixedTracks(float gap);
  CrossDimensionEstimator makeCrossDimensionEstimatorUsingBaseSize(
      Dimension dimension,
      float gap);
};

} // namespace facebook::yoga
