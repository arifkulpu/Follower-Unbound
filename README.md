# Follower Unbound

Follower Unbound is an SKSE plugin that adds configurable needs, combat consumables, equipment management, looting, and merchant behavior to your Skyrim followers. Its in-game interface is built with PrismaUI.

## What it currently does

### Follower management

- Finds active player followers and keeps a profile for each one.
- Tracks hunger, thirst, and fatigue over game time. When enabled and supplies are available, followers eat or drink when their needs reach the configured thresholds.
- Lets you inspect follower status, carried gold, carry weight, and equipment in the PrismaUI interface.
- Supports animal companions for the needs system when pet support is enabled.

### Combat and consumables

- Uses eligible health, magicka, and stamina potions during combat according to configured thresholds. Critical health can trigger emergency potion use.
- Can apply poison to compatible weapons while the follower is out of combat. Staff users and incompatible weapons are excluded.
- Uses animation graph events and compatible installed animation mods where available; animation availability depends on the follower's behavior graph and installed animation replacers.

### Optional animation mods

Follower Unbound requests the animation events below. These mods are optional; the game or follower's behavior graph may use a fallback or may not show a visible animation if the required NPC behavior support is absent.

- **Eating:** sends `IdleEat` / `IdleEating`. [Eating Animations and Sounds SE](https://www.nexusmods.com/skyrimspecialedition/mods/42602) provides eating animations. For follower NPC behavior, see the separate [Eating Animations and Sounds - NPC Follower Patch](https://www.nexusmods.com/skyrimspecialedition/mods/97535); compatibility depends on the food keywords and behavior setup.
- **Drinking and potions:** sends `IdleDrink`, `drinkPotion`, and related fallback events. [Ultimate Animated Potions NG](https://www.nexusmods.com/skyrimspecialedition/mods/97674) can respond to the potion events when the follower's graph supports them. For SunHelm waterskins, the plugin recognizes known waterskin forms and requests the drink idle; [Eating Animations and Sounds - SunHelm Patch](https://www.nexusmods.com/skyrimspecialedition/mods/53017) supplies related animation keywords.
- **Poisoning:** sends `APR_PoisonStart` with the poison animation variables, then tries generic poison events if that is not accepted. [Animated Poisons Redone](https://www.nexusmods.com/skyrimspecialedition/mods/194089) is optional, but its author documents the animation as player-only; follower playback is therefore not guaranteed. Follower Unbound also attaches a poison-bottle visual and plays a vanilla poison-use sound when it runs its poison routine.
- **Loot/search:** requests Skyrim's `IdleSearchBody` idle when a follower retrieves a physical dropped item or recovers an item it dropped. [Immersive Interactions - Animated Actions](https://www.nexusmods.com/skyrimspecialedition/mods/47670) provides the animation setup used by this idle. This request is best-effort and does not make followers pathfind to loot.
  - **NPC underwear after looting:** when follower distribution removes an NPC corpse's last body-slot garment, Follower Unbound can add and equip a configured replacement from `Data/SKSE/Plugins/Underwear.ini`. This prevents stripped corpses from being left without clothing; it is separate from the optional [Underwear.dll](https://www.nexusmods.com/skyrimspecialedition/mods/99021) NPC processing. The configured form must exist and resolve correctly for the replacement to work.

These are event/idle integrations, not bundled animation assets. Install the linked animation mods and their required behavior-generation patches where applicable. A successful event request does not guarantee a visible animation on every follower or custom skeleton.

### Looting

- Loot distribution is triggered when **you open and then close** a corpse or container menu. Followers do not automatically sweep every corpse when combat ends.
- Only items left in that container after you close its menu are considered. Items you take remain yours.
- Player-owned containers and containers in player-owned cells are skipped. Quest/alias containers are skipped.
- Eligible leftover inventory is divided among active followers who have looting enabled. Global category settings decide which item types are included. Gold is credited as follower gold; item stacks are split, and any indivisible remainder stays behind.
- Transfers are processed one item operation at a time through the UI task queue to reduce inventory-hook conflicts and game-thread stalls.
- Death Drop Overhaul support: when the player closes a dead NPC's container, the plugin looks in that same cell for weapon and shield references tagged by Death Drop Overhaul as dropped by that corpse. A nearby eligible follower can play the loot/search animation and pick up the physical item. This is limited to the corpse's tagged drops and the configured loot permissions; it does not sweep unrelated ground items. Ground pickups are added to inventory and are not automatically equipped by this path.
- Followers can recover items that they themselves drop during combat after combat has ended, provided the item reference remains nearby, loaded, and in the same cell.
- Configured `Underwear.ini` body-slot items are used on an NPC corpse if a follower's loot transfer removes its last body-slot garment. This is separate from Underwear.dll's own NPC processing.

### Equipment and inventory protection

- The Equipment page lists follower weapons and apparel. You can select specific items to equip and protect.
- Protected items are excluded from selling and are used by the equipment-protection logic during outfit changes.
- Automatically equipping better loot can be enabled or disabled. Better-equipment prompts use PrismaUI after queued loot work finishes.
- Mage robe preference can be enabled for followers identified as mages.

### Trade and supplies

- Followers can buy configured supplies and sell eligible excess inventory near merchants when shopping/selling systems are enabled.
- Buy Supplies is organized into clickable category tiles so you can jump directly to a group and return to the category overview. Categories cover restorative potions, poisons, treatment potions, food and drinks, ammunition, soul gems, travel/utility goods, special potions, and alchemy ingredients.
- Each follower has independent purchase toggles and target stock limits. Food and drink rows explain their included item types on hover; soups and stews are treated as medium food when the item is flagged as food by the game.
- Soul gem buying and selling are enabled by default. All six capacity/types (Petty, Lesser, Common, Greater, Grand, and Black) can be configured separately; the default target is 5 Petty Soul Gems and 1 of each other type. Existing co-save profiles are migrated to enable these preferences.
- Soul gems are sourced from general merchants and court wizards/enchanters. Food and drink supplies are available from innkeepers and general merchants.
- Followers can craft supported arrows, bolts, torches, and standard health, magicka, and stamina potions when the relevant crafting option is enabled, the stock target has not been met, and ingredients are available. Crafting requires a nearby workstation within 400 Skyrim units: a forge for arrows, bolts, and torches, or an alchemy lab for potions. Potion recipes use the corresponding vanilla ingredients and create one potion per crafting operation. Steel bolts are crafted in batches of 10; steel or iron arrows in batches of 24; torches one at a time.
- Crafting item hover tooltips show the required materials. Arrow and bolt production are separate options and are disabled by default; potion and torch crafting have their own stock targets.

### Rest and sleep integration

When the player finishes waiting or sleeping, active followers receive Follower Unbound's existing rest update. Follower Unbound sends the rest type and current in-game day to King Core, which can relay it to Life Again for light-wound recovery. This bridge requires compatible current versions of Follower Unbound, King Core, and Life Again.

## In-game interface

- **Shift + I** toggles the PrismaUI interface.
- Pages: Overview, Equipment, Loot Rules, Trade & Supplies, and Systems.
- The interface uses the custom Skyrim-themed icon pack in `PrismaUI/views/FollowerUnbound/assets/icons/`, with larger, higher-contrast icons and text and more visible scrollbars.
- Follower selection uses a portrait-enabled picker that switches companions immediately. Equipment cards have more room and wrap long item names at word boundaries; the same readability and spacing refinements apply across the interface.
- Buy Supplies category tiles open their matching product groups. Food/drink and crafting rows provide hover information about contents or recipes.
- The plugin retains a legacy SKSE Menu Framework interface as a fallback when PrismaUI is unavailable.
- The plugin can run its background systems without PrismaUI, but the modern interface requires PrismaUI.

## Requirements

- Skyrim Special Edition / Anniversary Edition with a game version supported by the installed SKSE and Address Library.
- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [Prisma UI](https://www.nexusmods.com/skyrimspecialedition/mods/148718) for the modern interface; without it, use the legacy interface if SKSE Menu Framework is installed.
- Death Drop Overhaul is optional. Its tagged dropped weapons/shields are only handled when that mod is installed and the relevant loot settings are enabled.
- SKSE Menu Framework is optional and used for the legacy fallback interface.

Animation, needs, merchant, and class integrations are conditional: their related features require compatible game assets, plugins, or supplies. Missing optional animation mods fall back to supported Skyrim animation events where possible.

## Configuration

Most settings are available in the PrismaUI **Systems**, **Loot Rules**, and **Trade & Supplies** pages. The plugin configuration file is:

```text
Data/SKSE/Plugins/FollowerUnbound.ini
```

The INI is generated with defaults if it does not exist. Current setting sections include:

- `[Systems]`: needs, combat potions, poison, shopping, selling, auto-equipment, equipment protection, and pet support.
- `[Looting]`: automatic leftover distribution and global loot categories.
- `[NeedsRates]` and `[Thresholds]`: need depletion rates, consumption timing, potion thresholds, and emergency thresholds.
- `[Selling]`: sale categories, soul gem protection, and transaction batch multiplier.
- `[Notifications]`: HUD notification preferences.

Per-follower master loot permission and merchant preferences are stored with follower data in the SKSE co-save rather than in this INI. Global loot categories are configured in the Systems page or INI.

## Installing with MO2

Install the mod so the enabled MO2 mod contains these paths:

```text
Follower Unbound/
├── PrismaUI/views/FollowerUnbound/
│   ├── app.js
│   ├── index.html
│   ├── styles.css
│   ├── ui-polish.css
│   └── assets/
│       ├── icons/       (custom Skyrim-themed UI icons)
│       └── portraits/   (follower portraits)
└── SKSE/Plugins/
    └── FollowerUnbound.dll
```

If you build the plugin yourself, copy the generated DLL to the mod's `SKSE/Plugins` folder. Keep the PrismaUI view files in the same enabled mod. Restart Skyrim after replacing the DLL.

## Building from source

The project uses CMake, MSVC, C++23, and CommonLibSSE-NG.

```powershell
cmake -S . -B build
cmake --build build --config Release --target FollowerUnbound
```

The output DLL is `build/Release/FollowerUnbound.dll`.

## Logs

The plugin log is written to:

```text
Documents/My Games/Skyrim Special Edition/SKSE/FollowerUnbound.log
```

It records plugin initialization, settings, loot queue decisions, item transfers, and recovery events. For hangs or crashes, include the relevant log and crash/hang report when investigating.

## Current scope and limitations

- Loot distribution does not start just because combat ends. You must open and close a corpse/container for the remaining contents to be processed.
- Followers do not pathfind to corpses or dropped items. Physical-item recovery is limited to same-cell references inside its distance limit.
- Only Death Drop Overhaul references bearing that mod's dropper metadata are considered for its physical weapon/shield drops.
- Animation requests are best-effort. A behavior graph or conflicting animation setup may prevent a visible idle from playing.
- Crafting is limited to the supported vanilla recipes and listed workstation types; it is not a general-purpose crafting system.

## Updates — 2026-10-16

- Reworked Buy Supplies navigation into category tiles with direct category views and a return-to-categories control.
- Added recipe/material hover details to crafting entries and item-group explanations to food and drink purchase rows.
- Added food and drink category explanations, including the medium-food classification used for soups, stews, bread, cheese, and similar foods.
- Enabled buying and selling soul gems by default, exposed all six soul gem types with individual stock targets, and migrated existing follower profiles to the enabled defaults.
- Added custom Skyrim-themed icons throughout the PrismaUI pages and improved icon sizing, text readability, row contrast, and scrollbar visibility.
- Increased Equipment item-card space and corrected long-name wrapping. Replaced the delayed native companion selector with an immediate portrait-enabled picker.
- Updated the README installation layout to include the current UI stylesheet, icon pack, and portrait assets.

