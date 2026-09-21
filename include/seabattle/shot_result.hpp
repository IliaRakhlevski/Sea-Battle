#pragma once
/**
 * @file shot_result.hpp
 * @brief What one shot turned out to be.
 *
 * Design decisions:
 *
 * - This header exists so that own_board.hpp and enemy_map.hpp never have to
 *   include each other. The own board PRODUCES this value, the knowledge map
 *   CONSUMES it, and neither is allowed to see the other's types: the fog of
 *   war is expressed as a dependency rather than as an agreement between
 *   programmers. A header holding one enumeration looks thin; keeping those
 *   two apart is its whole job.
 *
 * - Three outcomes, none of them carrying anything. In particular "sunk"
 *   carries no coordinates: the shooter reconstructs the ship from its own
 *   hit marks, which works precisely because ships may not touch. That was
 *   verified by brute force while the rules were being settled.
 *
 * - There is no "you won" and no "you lost". Both sides derive the outcome -
 *   a player has lost once ship_count of its ships are sunk, and the shooter
 *   counts sinkings on its own map. A result nobody has to be told about
 *   cannot be forgotten to be sent.
 *
 * - There is no "you already fired here" either, and that omission is
 *   deliberate. It is not an outcome of a shot but a refusal to accept the
 *   request, and the two have different natures: an outcome changes the state
 *   of a cell and decides whose turn comes next, a refusal does neither.
 *   Putting it here would place a value meaning "nothing happened" inside
 *   every switch that handles things which did happen. The refusal travels in
 *   the return type instead - OwnBoard::receive_shot returns
 *   std::optional<ShotResult>, and an empty result is the rejection.
 *
 *   That refusal is a second line of defence, not the first one. A shooter
 *   already knows which cells it has fired at - its own knowledge map says so -
 *   and is expected to filter a repeat before sending it. The board refuses
 *   anyway, for the same reason it validates the placement list it is handed:
 *   it does not trust what arrives from outside.
 *
 * @note Sunk implies Hit. The cell a Sunk result refers to is a hit cell as
 *       well, and whoever records the result has to mark it as one, or
 *       reconstructing the ship from the hit marks will be short its last
 *       cell.
 */

namespace seabattle {

    /**
     * @brief The outcome of one shot, as reported back to the shooter.
     *
     * These three are the only things a player ever learns about the
     * opponent's board.
     */
    enum class ShotResult
    {
        Miss,   ///< Water. The turn passes to the opponent.
        Hit,    ///< A ship was struck and is still afloat. The shooter fires again.
        Sunk    ///< A ship was struck and its last intact cell is now gone.
    };

} // namespace seabattle
