# OpenZone PDA

A S.T.A.L.K.E.R.-style PDA for DayZ. One device you carry that holds the map, your
faction, your contacts, your notes and your conversations — and takes modules that
give it a Geiger counter, a dosimeter, an antenna, or a radio from another mod.

Built on [OpenZone Core](https://github.com/covalschi/openzone-core). Designed to run
on **any server and any map**, configured entirely from JSON.

## Planned features

- **Device profiles.** Admins declare any number of PDA classnames in JSON — including
  items from other mods — and give each its own pages, marker limits, radio range and
  power draw. A rookie's PDA is not a Duty officer's PDA.
- **Map and markers.** Personal, faction and server-wide markers; share a marker with a
  friend, your faction, or a radio channel.
- **Factions, contacts and friends.** Self-contained, with optional providers that pick
  up Expansion factions or parties when those are installed.
- **Chat backed by Discord.** Private conversations and group chats live as private
  Discord threads, visible only to their participants. Accounts are linked by Discord
  OAuth against the player's SteamID.
- **Configurable radio.** Virtual frequencies, per-model bands, interference, battery
  drain and a rebindable push-to-talk key.
- **Always-on overlay.** Chat and radio stay readable while you move — the full device
  screen is a separate menu.

## Requirements

- [Community Framework](https://steamcommunity.com/sharedfiles/filedetails/?id=1559212036)
- OpenZone Core

The Discord bridge is optional. Without it the PDA works; the chat pages report that
they are offline.

## Status

Early development. Nothing is published to the Workshop yet, and the items are not in
the central economy — an admin has to spawn them.

Working and exercised on a test server: the device itself (battery, power, PIN, lazy
auto-lock), sealed quest devices opened with a decryptor on a countdown, the seven-page
shell with a tab strip built from the device's own profile, per-device map markers,
the contacts list with proximity-gated friend requests, notes, and the hardware
contract that lets another mod add its own module, tab and page.

Built but not yet connected, so honestly: shared markers, module behaviours (the
Geiger counter and dosimeter read "no data" until a mod supplies one), tiered battery
drain, data carriers, and Discord account linking. The list under **Planned features**
describes where this is going, not what it does today.

## Licence

CC BY-NC-SA 4.0 with an additional permission for server operators — see `LICENSE`
and `NOTICE`.
