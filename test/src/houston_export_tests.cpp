#include "chain_test_helpers.h"
#include "HoustonExport.h"

#include <gtest/gtest.h>

namespace {
struct ExportRoot {
  juce::File path = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("t3k-houston-export", "", false);
  ~ExportRoot() { path.deleteRecursively(); }
};

void restoreBlock(ChainTestProcessor& processor, const juce::ValueTree& block) {
  juce::ValueTree snapshot("ChainSnapshot"), lane("ChainBlocks");
  lane.appendChild(block, nullptr);
  snapshot.appendChild(lane, nullptr);
  processor.restoreFromTree(snapshot);
  ASSERT_TRUE(waitForChainLoaded(processor));
}

juce::File exportedFile(const juce::var& result) {
  EXPECT_TRUE(result["error"].isVoid()) << result["error"].toString().toStdString();
  return juce::File(result["path"].toString());
}

juce::File sidecarFor(const juce::File& file) {
  return file.getSiblingFile(file.getFileName() + ".provenance.json");
}

void expectOriginalBytes(const juce::File& exported, const char* fixture) {
  juce::MemoryBlock actual, expected;
  ASSERT_TRUE(exported.loadFileAsData(actual));
  ASSERT_TRUE(testFile(fixture).loadFileAsData(expected));
  EXPECT_EQ(actual, expected);
}

void expectProvenance(const juce::File& exported, const char* source) {
  const auto metadata = juce::JSON::parse(sidecarFor(exported).loadFileAsString());
  EXPECT_EQ(static_cast<int>(metadata["version"]), 1);
  EXPECT_EQ(metadata["source"].toString(), juce::String(source));
  EXPECT_EQ(metadata["origin"].toString(), juce::String("plugin"));
}
}  // namespace

TEST(HoustonExportTest, RestoredCatalogNamExportsOriginalCachedBytesOffline) {
  ExportRoot root;
  ChainTestProcessor processor;
  restoreBlock(processor, makeNamBlockTree("amp", 123, 456));
  const auto before = juce::JSON::toString(processor.getChainState(-1));
  const auto result = processor.saveModelToHouston("amp", root.path);
  const auto file = exportedFile(result);
  EXPECT_EQ(file.getParentDirectory().getParentDirectory(), root.path.getChildFile("NAM"));
  EXPECT_EQ(file.getFileName(), juce::String("Test Amp - amp.nam"));
  expectOriginalBytes(file, "a2-amp-test.nam");
  expectProvenance(file, "t3k");
  EXPECT_FALSE(static_cast<bool>(result["alreadySaved"]));
  EXPECT_EQ(before, juce::JSON::toString(processor.getChainState(-1)));
}

TEST(HoustonExportTest, CatalogIrUsesIrInboxAndUnmodifiedWav) {
  ExportRoot root;
  ChainTestProcessor processor;
  restoreBlock(processor, makeIrBlockTree("cab", 123, 456));
  const auto file = exportedFile(processor.saveModelToHouston("cab", root.path));
  EXPECT_EQ(file.getParentDirectory().getParentDirectory(), root.path.getChildFile("IR"));
  expectOriginalBytes(file, "cab-ir-test.wav");
  expectProvenance(file, "t3k");
}

TEST(HoustonExportTest, LocalFilesKeepOtherSourceAndPluginStash) {
  ExportRoot root;
  TONE3000Processor processor;
  const auto loaded = processor.loadLocalTonePath(testFile("a2-amp-test.nam"));
  ASSERT_TRUE(loaded["error"].isVoid());
  ASSERT_TRUE(waitForChainLoaded(processor));
  const auto before = juce::JSON::toString(processor.getChainState(-1));
  const auto file = exportedFile(processor.saveModelToHouston(loaded["blockId"].toString().toStdString(), root.path));
  expectOriginalBytes(file, "a2-amp-test.nam");
  expectProvenance(file, "other");
  EXPECT_EQ(before, juce::JSON::toString(processor.getChainState(-1)));
  EXPECT_TRUE(testFile("a2-amp-test.nam").existsAsFile());
}

TEST(HoustonExportTest, RepeatedSaveReusesIdenticalStagedFile) {
  ExportRoot root;
  ChainTestProcessor processor;
  restoreBlock(processor, makeNamBlockTree("amp", 123, 456));
  const auto first = processor.saveModelToHouston("amp", root.path);
  const auto second = processor.saveModelToHouston("amp", root.path);
  EXPECT_EQ(exportedFile(first), exportedFile(second));
  EXPECT_TRUE(static_cast<bool>(second["alreadySaved"]));
  EXPECT_EQ(root.path.getChildFile("NAM").getNumberOfChildFiles(juce::File::findDirectories), 1);
  EXPECT_EQ(exportedFile(first).getParentDirectory().getNumberOfChildFiles(juce::File::findFiles), 2);
}

TEST(HoustonExportTest, LooseBrowserDownloadWithSameNameIsUntouched) {
  ExportRoot root;
  const auto browserFile = root.path.getChildFile("NAM/Test Amp - amp.nam");
  ASSERT_TRUE(browserFile.getParentDirectory().createDirectory().wasOk());
  ASSERT_TRUE(browserFile.replaceWithText("browser bytes"));
  ASSERT_TRUE(sidecarFor(browserFile).replaceWithText("browser metadata"));
  ChainTestProcessor processor;
  restoreBlock(processor, makeNamBlockTree("amp", 123, 456));
  const auto file = exportedFile(processor.saveModelToHouston("amp", root.path));
  EXPECT_NE(file, browserFile);
  EXPECT_EQ(browserFile.loadFileAsString(), juce::String("browser bytes"));
  EXPECT_EQ(sidecarFor(browserFile).loadFileAsString(), juce::String("browser metadata"));
  expectOriginalBytes(file, "a2-amp-test.nam");
  expectProvenance(file, "t3k");
}

TEST(HoustonExportTest, ConcurrentProducerPackNeverGetsReplacedAtPublication) {
  ExportRoot root;
  const auto workspace = root.path.getChildFile(".pending");
  const auto pack = root.path.getChildFile("Capture");
  ASSERT_TRUE(workspace.createDirectory().wasOk());
  ASSERT_TRUE(workspace.getChildFile("capture.nam").replaceWithText("native bytes"));
  // The export selected an unused name. Model another producer claiming it
  // before publication, both while its directory is empty and after data lands.
  ASSERT_FALSE(pack.exists());
  ASSERT_TRUE(pack.createDirectory().wasOk());
  EXPECT_EQ(houston_export::publishDirectory(workspace, pack), houston_export::PublishResult::collision);
  EXPECT_TRUE(workspace.getChildFile("capture.nam").existsAsFile());
  EXPECT_EQ(pack.getNumberOfChildFiles(juce::File::findFiles), 0);
  ASSERT_TRUE(pack.getChildFile("unrelated.nam").replaceWithText("browser bytes"));
  EXPECT_EQ(houston_export::publishDirectory(workspace, pack), houston_export::PublishResult::collision);
  EXPECT_EQ(pack.getChildFile("unrelated.nam").loadFileAsString(), juce::String("browser bytes"));
  EXPECT_TRUE(workspace.getChildFile("capture.nam").existsAsFile());
  const auto alternative = root.path.getChildFile("Capture (1)");
  EXPECT_EQ(houston_export::publishDirectory(workspace, alternative), houston_export::PublishResult::published);
  EXPECT_EQ(alternative.getChildFile("capture.nam").loadFileAsString(), juce::String("native bytes"));
  EXPECT_FALSE(workspace.exists());
}

TEST(HoustonExportTest, FilenameCollisionPreservesExistingAssetAndMetadata) {
  ExportRoot root;
  ChainTestProcessor processor;
  restoreBlock(processor, makeNamBlockTree("amp", 123, 456));
  const auto original = exportedFile(processor.saveModelToHouston("amp", root.path));
  ASSERT_TRUE(original.replaceWithText("user file"));
  ASSERT_TRUE(sidecarFor(original).replaceWithText("{\"source\":\"other\"}"));
  const auto second = exportedFile(processor.saveModelToHouston("amp", root.path));
  EXPECT_NE(second, original);
  EXPECT_EQ(original.loadFileAsString(), juce::String("user file"));
  EXPECT_EQ(sidecarFor(original).loadFileAsString(), juce::String("{\"source\":\"other\"}"));
  expectOriginalBytes(second, "a2-amp-test.nam");
  expectProvenance(second, "t3k");
}

TEST(HoustonExportTest, FilesystemFailureDoesNotChangeLoadedChain) {
  ExportRoot root;
  ASSERT_TRUE(root.path.replaceWithText("not a directory"));
  ChainTestProcessor processor;
  restoreBlock(processor, makeIrBlockTree("cab", 123, 456));
  const auto before = juce::JSON::toString(processor.getChainState(-1));
  const auto result = processor.saveModelToHouston("cab", root.path);
  EXPECT_TRUE(result["error"].toString().isNotEmpty());
  EXPECT_EQ(before, juce::JSON::toString(processor.getChainState(-1)));
  EXPECT_EQ(root.path.loadFileAsString(), juce::String("not a directory"));
}

TEST(HoustonExportTest, MissingBlockDoesNotCreateDirectories) {
  ExportRoot root;
  TONE3000Processor processor;
  EXPECT_TRUE(processor.saveModelToHouston("missing", root.path)["error"].toString().isNotEmpty());
  EXPECT_FALSE(root.path.exists());
}

TEST(HoustonExportTest, UnsafeAndLongCatalogNamesStayInsideInbox) {
  ExportRoot root;
  ChainTestProcessor processor;
  auto block = makeNamBlockTree("amp", 123, 456);
  auto tone = juce::JSON::parse(block["toneJson"].toString());
  tone.getDynamicObject()->setProperty("title", "../../" + juce::String::repeatedString("long name ", 100));
  tone["models"][0].getDynamicObject()->setProperty("name", "../escape");
  block.setProperty("toneJson", juce::JSON::toString(tone), nullptr);
  restoreBlock(processor, block);
  const auto file = exportedFile(processor.saveModelToHouston("amp", root.path));
  EXPECT_EQ(file.getParentDirectory().getParentDirectory(), root.path.getChildFile("NAM"));
  EXPECT_LT(file.getFileName().getNumBytesAsUTF8(), 200u);
  EXPECT_FALSE(file.isHidden());
  expectOriginalBytes(file, "a2-amp-test.nam");
}

// Optional cross-repository fixture: retain actual native exports for Houston's
// importer integration test instead of fabricating plugin output in Python.
TEST(HoustonExportTest, RetainIntegrationFixtureWhenRequested) {
  const auto configured = juce::SystemStats::getEnvironmentVariable("T3K_HOUSTON_EXPORT_FIXTURE", "");
  if (configured.isEmpty()) GTEST_SKIP() << "Set T3K_HOUSTON_EXPORT_FIXTURE to retain native exports";
  const juce::File root(configured);
  ChainTestProcessor processor;
  restoreBlock(processor, makeNamBlockTree("amp", 123, 456));
  expectProvenance(exportedFile(processor.saveModelToHouston("amp", root)), "t3k");
  restoreBlock(processor, makeIrBlockTree("cab", 789, 987));
  expectProvenance(exportedFile(processor.saveModelToHouston("cab", root)), "t3k");
  const auto loaded = processor.loadLocalTonePath(testFile("a2-amp-test.nam"));
  ASSERT_TRUE(waitForChainLoaded(processor));
  expectProvenance(exportedFile(processor.saveModelToHouston(loaded["blockId"].toString().toStdString(), root)), "other");
}
