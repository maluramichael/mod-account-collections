# mod-account-collections

An [AzerothCore](https://www.azerothcore.org/) module (WotLK 3.3.5a) that makes **mounts
and companion pets account-wide**.

## What it does

When any character on an account learns a mount or a non-combat companion pet, every other
character on that account gains it too:

- **On login**, a character's known mounts/companions are recorded and any the account
  already owns are learned.
- **Live**, newly learned mounts/companions are captured as they happen; other characters
  pick them up the next time they log in.

The module creates its own storage table automatically on first start — there is no SQL
file to import, and no client patch is needed.

## Configuration

`conf/mod_account_collections.conf.dist`:

| Key                             | Default | Description                                   |
|---------------------------------|---------|-----------------------------------------------|
| `AccountCollections.Enable`     | `1`     | Master on/off switch                          |
| `AccountCollections.IncludeBots`| `0`     | Also sync AI-controlled characters            |
| `AccountCollections.Mounts`     | `1`     | Share mounts                                  |
| `AccountCollections.Companions` | `1`     | Share companion (non-combat) pets             |

Mounts are shared regardless of riding skill — knowing a mount you can't yet ride is
harmless and keeps syncing simple.

## Installation

Clone into your AzerothCore `modules/` directory and rebuild the worldserver.

## License

Released under the GNU GPL v2 (or later).
