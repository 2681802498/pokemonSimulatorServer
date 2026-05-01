#pragma once

#include <iosfwd>
#include <string>
#include <vector>

struct MoveTestSummary {
    int total = 0;
    int passed = 0;
    int failed = 0;
    int jsonMoveCount = 0;
    int statusMoveCount = 0;
    int implementedStatusMoveCount = 0;
    int unsupportedStatusMoveCount = 0;
    std::vector<std::string> failedMoveNames;
    std::vector<std::string> unsupportedStatusMoveNames;
};

MoveTestSummary runAllMoveTests(std::ostream& out, std::ostream& err);
