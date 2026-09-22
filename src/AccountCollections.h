/*
 * mod-account-collections - shared declarations.
 *
 * Kanboard #781: mounts and companion pets are account-wide. When any character
 * on an account learns a mount or companion (non-combat pet) spell, every other
 * character on that account also knows it - no client collections window needed
 * on 3.3.5a.
 *
 * Released under GNU GPL v2; redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef MOD_ACCOUNT_COLLECTIONS_H
#define MOD_ACCOUNT_COLLECTIONS_H

#include <cstdint>

namespace AccountCollections
{
    // Cached config (populated in WorldScript::OnAfterConfigLoad).
    struct Config
    {
        bool Enable      = true;  // module master switch
        bool IncludeBots = false; // also sync playerbot accounts (random fleet + altbots)
        bool Mounts      = true;  // sync mount spells
        bool Companions  = true;  // sync companion (non-combat pet) spells
    };

    Config& GetConfig();
}

#endif // MOD_ACCOUNT_COLLECTIONS_H
