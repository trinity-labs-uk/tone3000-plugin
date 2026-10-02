// Device layout regressions: JUCE may report the 1560 x 720 Artemis panel
// as 780 x 360 logical pixels. Both must retain readable, separate controls.
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <set>
#include <vector>

#include "Drive.h"
#include "Host.h"
#include "services/UiPrefs.h"
#include "views/Faceplate.h"
#include "views/MainScreen.h"
#include "views/PluginHeader.h"
#include "views/gallery/GalleryTile.h"
#include "views/gallery/StereoPanRail.h"
#include "widgets/DbMeter.h"
#include "widgets/Knob.h"
#include "widgets/RetryLoadBadge.h"

namespace t3k::ui::testbed {
namespace {

bool visible(const juce::Component& component) {
  for (auto* current = &component; current != nullptr; current = current->getParentComponent())
    if (!current->isVisible()) return false;
  return true;
}

void collect(juce::Component& parent, const std::function<bool(juce::Component&)>& accept,
             std::vector<juce::Component*>& found) {
  for (auto* child : parent.getChildren()) {
    if (!visible(*child)) continue;
    if (accept(*child)) found.push_back(child);
    collect(*child, accept, found);
  }
}

juce::String controlName(const juce::Component& component) {
  if (component.getTitle().isNotEmpty()) return component.getTitle();
  if (component.getName().isNotEmpty()) return component.getName();
  return component.getHelpText().upToFirstOccurrenceOf(".", false, false);
}

struct DeviceLayoutTests : juce::UnitTest {
  DeviceLayoutTests() : juce::UnitTest("Artemis compact layout", "ui") {}

  static void pump() { juce::MessageManager::getInstance()->runDispatchLoopUntil(80); }

  void separateControls(juce::Component& parent, const std::vector<juce::Component*>& controls) {
    const auto bounds = parent.getLocalBounds().toFloat();
    for (size_t i = 0; i < controls.size(); ++i) {
      const auto box = parent.getLocalArea(controls[i], controls[i]->getLocalBounds().toFloat());
      expect(!box.isEmpty() && bounds.contains(box), controlName(*controls[i]) + " remains inside its row");
      for (size_t j = 0; j < i; ++j) {
        const auto other = parent.getLocalArea(controls[j], controls[j]->getLocalBounds().toFloat());
        expect(!box.intersects(other), controlName(*controls[i]) + " overlaps " + controlName(*controls[j]));
      }
    }
  }

  void checkLayout(PluginRoot& root, bool stereo, int width, int height) {
    expect(root.getBounds() == juce::Rectangle<int>(0, 0, width, height));
    expect(root.getTransform().isIdentity(), "the panel uses native logical bounds");
    expectEquals(width * 6, height * 13);
    auto* header = drive::find(root, [](juce::Component& c) { return dynamic_cast<PluginHeader*>(&c) != nullptr; });
    auto* plate = drive::find(root, [](juce::Component& c) { return dynamic_cast<Faceplate*>(&c) != nullptr; });
    expect(header != nullptr && plate != nullptr);
    if (!header || !plate) return;
    expectEquals(header->getHeight(), PluginHeader::kDeviceHeight);
    expectEquals(plate->getHeight(), Faceplate::kDeviceHeight);
    expect(root.getLocalBounds().contains(header->getBounds()));
    expect(root.getLocalBounds().contains(plate->getBounds()));

    auto* main = drive::find(root, [](juce::Component& c) { return dynamic_cast<MainScreen*>(&c) != nullptr; });
    expect(main != nullptr);
    if (main) {
      const auto band = root.getLocalArea(main, main->getLocalBounds().toFloat());
      std::vector<juce::Component*> meters, rails;
      collect(*main, [](juce::Component& c) { return dynamic_cast<DbMeter*>(&c) != nullptr; }, meters);
      collect(*main, [](juce::Component& c) { return dynamic_cast<StereoPanRail*>(&c) != nullptr; }, rails);
      expectEquals(static_cast<int>(meters.size()), 2);
      expectEquals(static_cast<int>(rails.size()), stereo ? 1 : 0);
      for (auto* meter : meters)
        expect(band.contains(root.getLocalArea(meter, meter->getLocalBounds().toFloat())),
               "the full input/output meter remains inside the middle band");
      for (auto* rail : rails) {
        const auto railBounds = root.getLocalArea(rail, rail->getLocalBounds().toFloat());
        expect(band.contains(railBounds), "the stereo pan rail stays inside the middle band");
        for (auto* meter : meters)
          expect(!railBounds.intersects(root.getLocalArea(meter, meter->getLocalBounds().toFloat())),
                 "the pan rail stays clear of the meters");
        std::vector<juce::Component*> railControls;
        collect(*rail, [](juce::Component& c) {
          return dynamic_cast<Knob*>(&c) != nullptr || dynamic_cast<juce::Button*>(&c) != nullptr;
        }, railControls);
        separateControls(*rail, railControls);
        for (auto* control : railControls)
          expect(band.contains(root.getLocalArea(control, control->getLocalBounds().toFloat())),
                 controlName(*control) + " remains visible in the scaled pan rail");
      }
    }

    std::vector<juce::Component*> headerPeers;
    for (auto* child : header->getChildren())
      if (visible(*child)) headerPeers.push_back(child);
    separateControls(*header, headerPeers);

    std::vector<juce::Component*> controls;
    collect(*plate, [](juce::Component& c) {
      return dynamic_cast<Knob*>(&c) != nullptr || dynamic_cast<juce::Button*>(&c) != nullptr;
    }, controls);
    separateControls(*plate, controls);
    for (const auto* title : {"Gate", "Pitch", "Bass", "Middle", "Treble", "Input", "Output"}) {
      bool found = false;
      for (const auto* control : controls)
        found |= dynamic_cast<const Knob*>(control) != nullptr && control->getTitle() == title;
      expect(found, juce::String(title) + " is visible in the fully populated faceplate");
    }

    std::vector<juce::Component*> tiles;
    collect(root, [](juce::Component& c) { return dynamic_cast<GalleryTile*>(&c) != nullptr; }, tiles);
    expect(!tiles.empty(), "the chain gallery is visible");
    std::set<int> rows;
    for (size_t i = 0; i < tiles.size(); ++i) {
      const auto box = root.getLocalArea(tiles[i], tiles[i]->getLocalBounds().toFloat());
      // Tiles can scroll horizontally, but must never be clipped vertically
      // by the header, the lower controls, or the other stereo lane.
      expect(box.getY() >= header->getBottom() && box.getBottom() <= plate->getY(),
             "gallery tiles fit between the top and bottom controls");
      expect(box.getWidth() > 0 && box.getWidth() <= (stereo ? 80.0f : 112.0f));
      expectWithinAbsoluteError(box.getWidth(), box.getHeight(), 0.01f);
      rows.insert(juce::roundToInt(box.getY()));
      for (size_t j = 0; j < i; ++j)
        expect(!box.intersects(root.getLocalArea(tiles[j], tiles[j]->getLocalBounds().toFloat())),
               "gallery tiles and stereo rows stay separate");
      std::vector<juce::Component*> actions;
      collect(*tiles[i], [](juce::Component& c) { return dynamic_cast<juce::Button*>(&c) != nullptr; }, actions);
      separateControls(*tiles[i], actions);
    }
    expectEquals(static_cast<int>(rows.size()), stereo ? 2 : 1);
  }

  void runTest() override {
    const auto fixtures = Fixtures::load(fixturesDir().getChildFile("scenarios.json"));
    for (const bool stereo : {false, true}) {
      const auto* source = fixtures.find(stereo ? "main-stereo" : "main-mono");
      expect(source != nullptr);
      if (!source) return;
      for (const bool auth : {false, true}) {
        for (const int width : {780, 1560}) {
          const int height = width * 6 / 13;
          beginTest(juce::String(width) + "px " + (stereo ? "stereo" : "mono") + (auth ? " signed in" : " signed out"));
          Scenario scenario = *source;
          scenario.data = source->data.clone();
          auto* properties = scenario.data.getDynamicObject();
          properties->setProperty("deviceViewport", true);
          properties->setProperty("deviceWidth", width);
          properties->setProperty("deviceHeight", height);
          properties->setProperty("auth", auth);
          properties->setProperty("hints", true);
          MockBackend backend(scenario.data);
          ScaledHost host(backend, scenario, fixtures.root);
          auto& root = host.pluginRoot();
          root.services().pointer.sawInput(true);
          root.services().prefs.setBool(UiPrefs::kShowGateControl, true);
          root.services().prefs.setBool(UiPrefs::kShowPitchControl, true);
          for (const auto* id : {"gateEnabled", "pitchEnabled"})
            if (auto* parameter = backend.parameter(id)) parameter->setValueNotifyingHost(1.0f);
          for (const auto* id : {"spreadEnabled", "alignEnabled"})
            if (auto* parameter = backend.parameter(id)) parameter->setValueNotifyingHost(0.0f);
          pump();
          checkLayout(root, stereo, width, height);

          // The advertised and powered image groups have different children;
          // both must fit alongside tone power, balance and the output knob.
          for (const auto* id : {"spreadEnabled", "alignEnabled"})
            if (auto* parameter = backend.parameter(id)) parameter->setValueNotifyingHost(1.0f);
          pump();
          checkLayout(root, stereo, width, height);

          root.services().hints.setEnabled(false);
          pump();
          checkLayout(root, stereo, width, height);
          root.services().hints.setEnabled(true);
          pump();
          checkLayout(root, stereo, width, height);

          if (width == 780 && !stereo && !auth) {
            beginTest("switching a live mono chain to stereo reflows the gallery and controls");
            const auto* stereoScenario = fixtures.find("main-stereo");
            expect(stereoScenario != nullptr);
            if (stereoScenario) {
              backend.setChain(stereoScenario->data["chain"].clone());
              root.services().chain.refresh(true);
              pump();
              checkLayout(root, true, width, height);
            }
          }

          if (width == 780 && stereo && auth) {
            beginTest("a banner and hints leave both compact stereo rows clear");
            const auto* warning = fixtures.find("banner-no-input");
            expect(warning != nullptr);
            if (warning) {
              backend.setDevice(warning->data["device"]);
              pump();
              expect(root.services().banners.active().has_value());
              auto* banner = drive::find(root, [](juce::Component& c) {
                return dynamic_cast<AppBanner*>(&c) != nullptr && visible(c);
              });
              expect(banner != nullptr, "the warning is actually rendered");
              checkLayout(root, stereo, width, height);

              auto failedChain = backend.getChainState(-1).clone();
              auto* blocks = failedChain["chain"].getArray();
              expect(blocks != nullptr && !blocks->isEmpty());
              if (blocks != nullptr && !blocks->isEmpty()) {
                auto* block = blocks->getReference(0).getDynamicObject();
                expect(block != nullptr);
                if (block) {
                  block->setProperty("loadFailed", true);
                  block->setProperty("loaded", false);
                  block->setProperty("modelLoading", false);
                  backend.setChain(failedChain);
                  root.services().chain.refresh(true);
                  pump();
                  auto* retry = drive::find(root, [](juce::Component& c) {
                    return dynamic_cast<RetryLoadBadge*>(&c) != nullptr && visible(c);
                  });
                  expect(retry != nullptr, "the failed download exposes its retry control");
                  checkLayout(root, stereo, width, height);
                }
              }
            }
          }
        }
      }
    }
  }
};

DeviceLayoutTests deviceLayoutTests;

}  // namespace
}  // namespace t3k::ui::testbed
