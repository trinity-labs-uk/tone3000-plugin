// The one uniform scale the shell's AffineTransform applies to the root.
// Views lay out in design space and never need it; the tone browser is the
// exception and counter-scales its body to keep it screen-pixel sized.
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

  // The shell, on every fit of the root.
  void set(double factor) {
    if (juce::approximatelyEqual(factor, factor_)) return;
    factor_ = factor;
    listeners_.call([](Listener& l) { l.zoomChanged(); });
  }

  void addListener(Listener* l) { listeners_.add(l); }
  void removeListener(Listener* l) { listeners_.remove(l); }

private:
  double factor_ = 1.0;
  juce::ListenerList<Listener> listeners_;
};

}  // namespace t3k::ui
