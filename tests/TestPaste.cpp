#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

// A move is only ever recorded once under one parent: pasting a variation
// that starts the same way as one already there goes into that one.
SUITE(Paste)
{
  TEST(CopiedBranchPastedWhereItIsMergesIntoIt)
  {
    TestScenarioGui scenario;
    scenario.clickPoint(3, 3);
    scenario.clickPoint(15, 15);
    scenario.clickPoint(15, 3);

    scenario.clickSelectText("|<");
    scenario.clickSelectText(">");
    scenario.clickSelectText("Copy");
    scenario.clickSelectText("|<");
    scenario.clickSelectText("Paste");

    const sgf::Node& root = scenario.game().root();
    CHECK_EQUAL(root.childCount(), size_t(1));
    CHECK_EQUAL(root.child(0).childCount(), size_t(1));
    CHECK_EQUAL(root.child(0).child(0).childCount(), size_t(1));
    CHECK_EQUAL(&scenario.game().current(), &root.child(0));
    CHECK(scenario.shows("Merged into the variation that was already here."));
  }

  TEST(OnlyTheNewPartBranchesOff)
  {
    TestScenarioGui scenario;
    scenario.clickPoint(3, 3);
    scenario.clickPoint(15, 15);
    scenario.clickSelectText("|<");

    CHECK(scenario.game().pasteBranch("(;B[dd];W[dp];B[pd])").ok);

    const sgf::Node& root = scenario.game().root();
    CHECK_EQUAL(root.childCount(), size_t(1));
    const sgf::Node& black = root.child(0);
    CHECK_EQUAL(black.childCount(), size_t(2));
    CHECK_EQUAL(black.child(0).get("W"), "pp");
    CHECK_EQUAL(black.child(1).get("W"), "dp");
    CHECK_EQUAL(black.child(1).childCount(), size_t(1));
  }

  TEST(VariationsOfOneRecordMergeWithEachOther)
  {
    TestScenarioGui scenario;
    CHECK(scenario.game().pasteBranch("(;GM[1]SZ[19](;B[dd];W[pp])(;B[dd];W[dp])(;B[pd]))").ok);

    const sgf::Node& root = scenario.game().root();
    CHECK_EQUAL(root.childCount(), size_t(2));
    CHECK_EQUAL(root.child(0).get("B"), "dd");
    CHECK_EQUAL(root.child(0).childCount(), size_t(2));
    CHECK_EQUAL(root.child(1).get("B"), "pd");
  }

  TEST(TheCommentsOfBothAreKept)
  {
    TestScenarioGui scenario;
    CHECK(scenario.game().pasteBranch("(;B[dd]C[Mine])").ok);
    scenario.clickSelectText("|<");
    CHECK(scenario.game().pasteBranch("(;B[dd]C[Theirs]TR[aa])").ok);

    const sgf::Node& black = scenario.game().root().child(0);
    CHECK_EQUAL(scenario.game().root().childCount(), size_t(1));
    CHECK_EQUAL(black.get("C"), "Mine\n\nTheirs");
    CHECK_EQUAL(black.get("TR"), "aa");
  }

  TEST(ADifferentMoveIsStillANewVariation)
  {
    TestScenarioGui scenario;
    scenario.clickPoint(3, 3);
    scenario.clickSelectText("|<");
    CHECK(scenario.game().pasteBranch("(;B[pp])").ok);

    CHECK_EQUAL(scenario.game().root().childCount(), size_t(2));
  }
}
