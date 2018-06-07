#include "HeuristicOptimizer.h"
#include "Coordinator.h"
#include "Display.h"
#include "HomomorphicOperators.h"
#include "VisitorTemp2.h"
#include <gtest/gtest.h>

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

class VisitorTestFixture : public testing::Test {
public:
    VisitorTestFixture()
            : catalog("resources")
    { Catalog::instance(catalog); }

protected:
    Catalog catalog;
};

TEST_F(VisitorTestFixture, testFoo) {
    auto left = Scan("red10");
    auto right = Scan("red10");
    auto unioned = left.Union(right);
    auto encoded = unioned.Encode();
/*
    auto input = Scan("red10");
    auto gray = input.Map(lightdb::Greyscale());
    auto encoded = gray.Encode();
*/
    auto environment = LocalEnvironment();
    auto coordinator = Coordinator();
    Plan plan = HeuristicOptimizer(environment).optimize(encoded);

    print_plan(plan);

    coordinator.save(plan, "out.hevc");
}
