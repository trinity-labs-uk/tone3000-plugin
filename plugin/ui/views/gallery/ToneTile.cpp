#include "ToneTile.h"

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr int kImageFadeMs = 200;
constexpr float kDimmedImage = 0.35f;
const juce::Colour kStrip = juce::Colours::black.withAlpha(0.35f);
}  // namespace

void ToneTile::Strip::paint(juce::Graphics& g) {
  g.setColour(kStrip);
  g.fillRect(getLocalBounds().withHeight(kChromeHeight));
}

ToneTile::ToneTile(Services& services, const ChainItem& block, int size)
    : GalleryTile(services, block.blockId, size),
      block_(block),
      image_(services.images),
      power_(Icon::Power, ChromeIconButton::Tone::power, help::Key::blockPower),
      swap_(Icon::ArrowLeftRight, ChromeIconButton::Tone::plain, help::Key::swapTone),
      remove_(Icon::Trash2, ChromeIconButton::Tone::plain, help::Key::removeBlock),
      glow_(services.meters, MeterStore::blockOutId(block.blockId)),
      led_(services.meters, MeterStore::blockOutId(block.blockId)) {
  image_.setCornerRadius(gallery::kTileCorner);
  image_.setGlow(glow_.glow());
  glow_.onChange = [this] { image_.setGlow(glow_.glow()); };
  addAndMakeVisible(image_);
  addChildComponent(dots_);
  addChildComponent(retry_);
  retry_.onRetry = [this] { this->services().modelLoads.retry(blockId()); };

  addAndMakeVisible(chrome_);
  for (auto* b : {&power_, &swap_, &remove_}) chrome_.addAndMakeVisible(*b);
  power_.onClick = [this] { togglePower(); };
  swap_.onClick = [this] { if (onSwap) onSwap(blockId()); };
  remove_.onClick = [this] { this->services().chain.removeBlock(blockId()); };

  addAndMakeVisible(ledSlot_);
  ledSlot_.addChildComponent(led_);
  ledSlot_.setSize(BlockLed::kSize, BlockLed::kSize);
  addMouseListener(&hover_, true);
  services.pointer.addListener(this);

  setBlock(block);
  setHovered(false);
  resized();  // the base set the size before these children existed
}

ToneTile::~ToneTile() {
  services().pointer.removeListener(this);
  removeMouseListener(&hover_);
}

void ToneTile::setBlock(const ChainItem& block) {
  block_ = block;
  enabled_ = block.params.enabled;
  setHelpText(help::toneTile(block.tone.title));
  setTitle(block.tone.title);
  image_.setTone(block.tone.image, block.tone.gear, block.tone.local, kGlyphSize);
  syncState();
}

// A model download/prepare is in flight: `modelLoading` covers switches
// (the previous model keeps playing, so `loaded` stays true) and `!loaded`
// covers fresh blocks that have nothing to play yet.
void ToneTile::syncState() {
  const bool busy = block_.modelLoading || (!block_.loaded && !block_.loadFailed);
  const bool armed = dropArmed();
  image_.setVisible(!armed);
  imageFade_.animateTo(enabled_ && !busy && !block_.loadFailed ? 1.0f : kDimmedImage, kImageFadeMs);
  dots_.setVisible(!armed && busy && !block_.loadFailed);
  retry_.setVisible(!armed && block_.loadFailed);
  chrome_.setVisible(!armed);
  ledSlot_.setVisible(!armed);
  power_.setOn(enabled_);
  repaint();
}

void ToneTile::togglePower() {
  enabled_ = !enabled_;
  power_.setOn(enabled_);
  imageFade_.animateTo(enabled_ ? 1.0f : kDimmedImage, kImageFadeMs);
  this->services().chain.setBlockParam(blockId(), "enabled", enabled_);
}

void ToneTile::open() {
  if (onOpen) onOpen(blockId());
}

std::vector<ContextMenu::Item> ToneTile::menuItems() {
  std::vector<ContextMenu::Item> items{
      {"Copy", Icon::Copy, help::Key::copyBlock,
       [this] { this->services().chain.copyBlock(blockId()); }},
  };
  if (services().backend.canSaveToHouston()) {
    items.push_back({"Save to Houston", Icon::Download, help::Key::saveToHouston,
                    [this] {
                      savingToHouston_ = true;
                      services().toast.show("Saving to Houston...");
                      services().backend.saveModelToHouston(
                          blockId(), [safe = juce::Component::SafePointer<ToneTile>(this)](juce::var result) {
                            if (safe == nullptr) return;
                            safe->savingToHouston_ = false;
                            const auto error = result["error"].toString();
                            safe->services().toast.show(error.isNotEmpty() ? error : "Saved to Houston Imports");
                          });
                    },
                    savingToHouston_ || !block_.loaded || block_.modelLoading || block_.loadFailed});
  }
  for (auto& item : localLoadItems()) items.push_back(std::move(item));
  return items;
}

void ToneTile::dropArmedChanged(bool) { syncState(); }

void ToneTile::travellingChanged(bool) { setHovered(hovered_); }

void ToneTile::pointerMoved(const juce::MouseEvent& e, bool leaving) {
  // Leaving onto one of our own children is still hovering the tile.
  setHovered(!leaving || getLocalBounds().contains(e.getEventRelativeTo(this).getPosition()));
}

void ToneTile::setHovered(bool hovered) {
  hovered_ = hovered;
  const bool shown = services().pointer.coarse() || hovered_ || travelling();
  chrome_.setAlpha(shown ? 1.0f : 0.0f);
  chrome_.setInterceptsMouseClicks(false, shown);
}

void ToneTile::resized() {
  const auto box = getLocalBounds();
  image_.setBounds(box);
  // On compact tiles, reserve the quick-action row before placing loading
  // feedback. Its retry target must remain inside the tile and below power,
  // swap and delete even when a banner reduces the stereo lane height.
  const auto feedback = getWidth() <= 112 ? box.withTrimmedTop(kChromeHeight).reduced(4) : box;
  dots_.setBounds(feedback.withSizeKeepingCentre(LoadingDots::kWidth, LoadingDots::kDot + 2 * LoadingDots::kMargin));
  const float retryScale = std::max(0.05f, std::min({1.0f, feedback.getWidth() / float(retry_.getWidth()),
                                                   feedback.getHeight() / float(retry_.getHeight())}));
  retry_.setTransform(juce::AffineTransform::scale(retryScale));
  retry_.setTopLeftPosition(juce::roundToInt(feedback.getCentreX() / retryScale - retry_.getWidth() / 2.0f),
                            juce::roundToInt(feedback.getCentreY() / retryScale - retry_.getHeight() / 2.0f));

  chrome_.setBounds(box.withHeight(kChromeHeight));
  const int y = kChromePad;
  const int gap = getWidth() < 112 ? 2 : kChromeGap;
  const int size = std::max(1, std::min(theme::kIconBoxSize, (getWidth() - 2 * kChromePad - 2 * gap) / 3));
  power_.setBounds(kChromePad, y, size, size);
  remove_.setBounds(getWidth() - kChromePad - size, y, size, size);
  swap_.setBounds(remove_.getX() - gap - size, y, size, size);

  ledSlot_.setTopLeftPosition(getWidth() - kLedInset - BlockLed::kSize,
                              getHeight() - kLedInset - BlockLed::kSize);
}

void ToneTile::paint(juce::Graphics& g) {
  paint::fill(g, getLocalBounds().toFloat(), gallery::kTileCorner, theme::kSurface);
  if (dropArmed()) {
    const float s = gallery::kFileDropGlyphSize;
    Icons::draw(g, Icon::Upload, getLocalBounds().toFloat().withSizeKeepingCentre(s, s), theme::kGray);
  }
}

void ToneTile::paintOverChildren(juce::Graphics& g) {
  if (dropArmed())
    paint::dashedBorder(g, getLocalBounds().toFloat(), gallery::kTileCorner,
                        gallery::kFileDropBorder, gallery::kAddTileBorderWidth);
}

}  // namespace t3k::ui
