#pragma once

#include "Battle/Items.h"
#include <iosfwd>
#include <string>
#include <vector>

struct ItemTestSummary {
    int total = 0;
    int passed = 0;
    int failed = 0;
    int jsonBattleItemCount = 0;
    int unsupportedJsonBattleItemCount = 0;
    std::vector<std::string> failedItemNames;
};

std::vector<ItemType> getAllTestItemTypes();
bool runSingleItemTest(ItemType itemType, std::ostream& out, std::ostream& err);
ItemTestSummary runAllItemTests(std::ostream& out, std::ostream& err);
