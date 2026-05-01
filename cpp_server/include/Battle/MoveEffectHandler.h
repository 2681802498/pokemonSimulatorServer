#pragma once

class Battle;
class Pokemon;
class Move;

namespace MoveEffectHandlers {
void applyStandardMoveEffect(Battle& battle, Pokemon* attacker, Pokemon* defender, const Move& move);
}
