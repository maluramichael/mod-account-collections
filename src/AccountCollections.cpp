/*
 * mod-account-collections
 *
 * Kanboard #781: mounts and companion (non-combat) pets are account-wide on the
 * mod-playerbots WotLK 3.3.5a fork, which has no client collections window.
 * Whenever a character on an account knows a mount or companion spell, every
 * other character on that account learns it too - reconciled at login and
 * captured live whenever a spell is learned at runtime.
 *
 * Storage is a small per-account table in the characters DB, created
 * programmatically in WorldScript::OnStartup() (deliberately NOT shipped as an
 * SQL update file: on this fork failing module SQL aborts the whole worldserver
 * boot, so schema creation must tolerate failure at runtime instead).
 *
 * Released under GNU GPL v2; redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Config.h"
#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldSession.h"

// Playerbots fork header, pulls in GET_PLAYERBOT_AI.
#include "Playerbots.h"

#include "AccountCollections.h"

namespace AccountCollections
{
    Config& GetConfig()
    {
        static Config cfg;
        return cfg;
    }
}

using AccountCollections::GetConfig;

namespace
{
    // Create our per-account collections table. Deliberately NOT an SQL update
    // file: on this fork a failing module SQL aborts the whole worldserver boot,
    // so we create the table programmatically and tolerate failure at runtime
    // instead (mirrors mod-guild-tax's EnsureSchema()).
    void EnsureSchema()
    {
        CharacterDatabase.Execute(
            "CREATE TABLE IF NOT EXISTS `account_wide_collections` ("
            "`account_id` INT UNSIGNED NOT NULL, "
            "`spell_id` INT UNSIGNED NOT NULL, "
            "PRIMARY KEY (`account_id`, `spell_id`)"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;");
    }

    // A mount spell grants either the SPELL_AURA_MOUNTED visual/usable-mount aura
    // or a SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED aura (some mount spells split
    // visual and speed across two effects; checking either effect catches both
    // single- and multi-effect mount spells).
    bool IsMountSpell(SpellInfo const* spellInfo)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsAura(SPELL_AURA_MOUNTED) || effect.IsAura(SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED))
                return true;
        }

        return false;
    }

    // A companion spell SPELL_EFFECT_SUMMON's a non-combat critter. We classify by
    // the summoned creature's template type (CREATURE_TYPE_CRITTER or
    // CREATURE_TYPE_NON_COMBAT_PET). This naturally excludes hunter pet taming
    // (a different effect, SPELL_EFFECT_TAME_CREATURE) and warlock/other minion
    // summons (those summon BEAST/DEMON/UNDEAD/etc. typed creatures).
    bool IsCompanionSpell(SpellInfo const* spellInfo)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (!effect.IsEffect(SPELL_EFFECT_SUMMON))
                continue;

            uint32 entry = static_cast<uint32>(effect.MiscValue);
            if (!entry)
                continue;

            CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(entry);
            if (!creatureTemplate)
                continue;

            if (creatureTemplate->type == CREATURE_TYPE_CRITTER || creatureTemplate->type == CREATURE_TYPE_NON_COMBAT_PET)
                return true;
        }

        return false;
    }

    // Classifies spellId against the enabled categories. Returns true (and sets
    // isMount/isCompanion) only for spells the config says we should track.
    bool IsCollectibleSpell(uint32 spellId, bool& isMount, bool& isCompanion)
    {
        isMount = false;
        isCompanion = false;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return false;

        AccountCollections::Config const& cfg = GetConfig();

        if (cfg.Mounts && IsMountSpell(spellInfo))
            isMount = true;
        else if (cfg.Companions && IsCompanionSpell(spellInfo))
            isCompanion = true;

        return isMount || isCompanion;
    }

    void RememberSpell(uint32 accountId, uint32 spellId)
    {
        CharacterDatabase.Execute(
            "INSERT IGNORE INTO `account_wide_collections` (`account_id`, `spell_id`) VALUES ({}, {})",
            accountId, spellId);
    }
}

// =====================================================================
//  PlayerScript: login reconciliation + live capture.
// =====================================================================
class AccountCollectionsPlayerScript : public PlayerScript
{
public:
    AccountCollectionsPlayerScript() : PlayerScript("AccountCollections_PlayerScript") { }

    // Login sync, in two passes:
    //   1) Capture - any mount/companion spell this character already knows that
    //      isn't in the account table yet gets added.
    //   2) Grant - every spell in the account table this character doesn't know
    //      yet is learned via Player::learnSpell(). Mounts are granted even if the
    //      character lacks the riding skill for that mount type: the spell is
    //      inert without the skill (using it just fails), and granting it anyway
    //      keeps the sync simple and avoids re-checking/re-granting on every skill
    //      change. See README.md for the full rationale.
    void OnPlayerLogin(Player* player) override
    {
        AccountCollections::Config const& cfg = GetConfig();
        if (!cfg.Enable || !player)
            return;

        if (!cfg.IncludeBots && GET_PLAYERBOT_AI(player) != nullptr)
            return;

        WorldSession* session = player->GetSession();
        if (!session)
            return;

        uint32 accountId = session->GetAccountId();
        if (!accountId)
            return;

        for (auto const& spellEntry : player->GetSpellMap())
        {
            uint32 spellId = spellEntry.first;

            bool isMount = false;
            bool isCompanion = false;
            if (IsCollectibleSpell(spellId, isMount, isCompanion))
                RememberSpell(accountId, spellId);
        }

        QueryResult result = CharacterDatabase.Query(
            "SELECT `spell_id` FROM `account_wide_collections` WHERE `account_id` = {}", accountId);
        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint32 spellId = fields[0].Get<uint32>();

            if (!player->HasSpell(spellId))
                player->learnSpell(spellId, false);
        } while (result->NextRow());
    }

    // Runtime capture: whenever this character learns a new spell (trained,
    // scroll, vendor, quest reward, ...), remember it on the account immediately
    // if it's a mount/companion. This does NOT push it live to other online
    // characters on the account - they pick it up on their next OnPlayerLogin,
    // same as any spell learned while this character was already logged out.
    void OnPlayerLearnSpell(Player* player, uint32 spellId) override
    {
        AccountCollections::Config const& cfg = GetConfig();
        if (!cfg.Enable || !player)
            return;

        if (!cfg.IncludeBots && GET_PLAYERBOT_AI(player) != nullptr)
            return;

        WorldSession* session = player->GetSession();
        if (!session)
            return;

        bool isMount = false;
        bool isCompanion = false;
        if (!IsCollectibleSpell(spellId, isMount, isCompanion))
            return;

        RememberSpell(session->GetAccountId(), spellId);
    }
};

// =====================================================================
//  WorldScript: config load + schema.
// =====================================================================
class AccountCollectionsWorldScript : public WorldScript
{
public:
    AccountCollectionsWorldScript() : WorldScript("AccountCollections_WorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        AccountCollections::Config& cfg = GetConfig();
        cfg.Enable      = sConfigMgr->GetOption<bool>("AccountCollections.Enable", true);
        cfg.IncludeBots = sConfigMgr->GetOption<bool>("AccountCollections.IncludeBots", false);
        cfg.Mounts      = sConfigMgr->GetOption<bool>("AccountCollections.Mounts", true);
        cfg.Companions  = sConfigMgr->GetOption<bool>("AccountCollections.Companions", true);
    }

    void OnStartup() override
    {
        EnsureSchema();
    }
};

// =====================================================================
//  Registration
// =====================================================================
void AddAccountCollectionsScripts()
{
    new AccountCollectionsPlayerScript();
    new AccountCollectionsWorldScript();
}
