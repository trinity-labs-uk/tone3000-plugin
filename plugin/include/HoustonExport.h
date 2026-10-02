#pragma once

#include <juce_core/juce_core.h>

namespace houston_export {
enum class PublishResult { published, collision, failed };

// Publish a complete hidden model folder within the same filesystem. Linux
// reserves its destination atomically, including against an empty directory
// another producer creates after the caller's existence check.
PublishResult publishDirectory(const juce::File& workspace, const juce::File& destination);
}  // namespace houston_export
