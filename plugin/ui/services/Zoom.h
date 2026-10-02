// Window scale. Regular plugin windows use the same factor on both axes;
// the Artemis standalone stretches its 1024-wide design over the 1560-wide
// display while preserving the existing 720px height. The tone browser
// counter-scales each axis so its cards and text remain screen-pixel sized.
#pragma once

#include <juce_core/juce_core.h>

namespace t3k::ui {

class Zoom {
public:
  struct Listener {
    virtual ~Listener() = default;
    virtual void zoomChanged() = 0;
  };

  double factor() const { return factor_; }
  double horizontalFactor() const { return factor_; }
  double verticalFactor() const { return verticalFactor_; }

  // The shell, on every fit of the root.
  void set(double factor) { set(factor, factor); }
  void set(double horizontal, double vertical) {
    if (juce::approximatelyEqual(horizontal, factor_) && juce::approximatelyEqual(vertical, verticalFactor_))
      return;
    factor_ = horizontal;
    verticalFactor_ = vertical;
    listeners_.call([](Listener& l) { l.zoomChanged(); });
  }

  void addListener(Listener* l) { listeners_.add(l); }
  void removeListener(Listener* l) { listeners_.remove(l); }

private:
  double factor_ = 1.0;
  double verticalFactor_ = 1.0;
  juce::ListenerList<Listener> listeners_;
};

}  // namespace t3k::ui
