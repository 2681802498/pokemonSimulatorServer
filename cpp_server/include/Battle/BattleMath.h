#pragma once

#include "Moves.h"

class Pokemon;

int heavySlamPowerByWeightRatio(int attackerWeight, int defenderWeight);
float stageMultiplier(int stage);
float criticalHitChanceForStage(int stage);
bool hasMajorStatus(const Pokemon* pokemon);
bool isSheerForceBoostedMove(const Move& move);