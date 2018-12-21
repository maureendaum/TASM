#include "HeuristicOptimizer.h"
#include "Greyscale.h"
#include "extension.h"
#include "Display.h"
#include "AssertVideo.h"

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

class UDFTestFixture : public testing::Test {
public:
    UDFTestFixture()
            : catalog("resources")
    { Catalog::instance(catalog); }

protected:
    Catalog catalog;
};

TEST_F(UDFTestFixture, testGreyscale) {
    auto name = "red10";
    auto output = "out.hevc";

    auto input = Scan(name);
    auto gray = input.Map(lightdb::Greyscale);
    auto encoded = gray.Encode();

    auto environment = LocalEnvironment();
    auto coordinator = Coordinator();
    Plan plan = HeuristicOptimizer(environment).optimize(encoded);

    print_plan(plan);

    coordinator.save(plan, output);

    EXPECT_VIDEO_VALID(output);
    EXPECT_VIDEO_FRAMES(output, 600);
    EXPECT_VIDEO_RESOLUTION(output, 240, 320);
    EXPECT_EQ(remove(output), 0);
}

TEST_F(UDFTestFixture, testDrawtext) {
    auto name = "red10";
    auto output = "out.hevc";

    auto drawtext = lightdb::extensibility::Load("drawtext");

    auto input = Scan(name);
    auto annotated = input.Map(drawtext);
    auto encoded = annotated.Encode();

    auto environment = LocalEnvironment();
    auto coordinator = Coordinator();
    Plan plan = HeuristicOptimizer(environment).optimize(encoded);

    print_plan(plan);

    coordinator.save(plan, output);

    EXPECT_VIDEO_VALID(output);
    EXPECT_VIDEO_FRAMES(output, 600);
    EXPECT_VIDEO_RESOLUTION(output, 240, 320);
    EXPECT_EQ(remove(output), 0);

    //TODO add Tesseract OCR check
}