# Online play (experimental)

Steam games get gbe_fork in place of their `steam_api(64).dll` (`runtime/steamworks/README.md`). gbe_fork tells the
game Steam runs and the game is owned, but it never reaches Steam's servers, so a game that signs in to its own
online services with a Steam ticket, lists lobbies or joins a friend stays offline.

## Steam features = Online

Game settings › More settings › Steam features › **Online (experimental)** keeps gbe_fork and puts Playden's online
bridge (`tools/steam-online`) in front of it, 64-bit and 32-bit:

- The game's `steam_api64.dll` / `steam_api.dll` becomes the bridge, with gbe_fork beside it as `steam_api64_gbe.dll` /
  `steam_api_gbe.dll`. Every Steam call the bridge does not answer goes to gbe_fork.
- At start the bridge signs in to a Steam CM over WebSocket (WinHTTP) with the account's refresh token, keeps the
  session with heartbeats, and shows the account online and playing the game.

What it answers through Steam:

| Interface | What the game gets |
| --- | --- |
| ISteamUser | Session and Web API tickets that Steam validates, Steam's encrypted app ticket. `BeginAuthSession`/`EndAuthSession` (hosting) check other players' tickets with Steam. |
| ISteamMatchmaking (009) | Lobbies: list with filters, create, join, leave, lobby and member data, chat, game server, owner, type, member limit, invites. |
| ISteamFriends (015-018) | The friends list, names, states, and the game and lobby a friend is in. |
| Invites | There is no Steam overlay on the console: an invite to a lobby is accepted at once. |

When Steam can't be reached in time, tickets fall back to gbe_fork's and lobby calls fail with "no connection".

## Stay signed in, and the token on disk

The bridge needs the account's Steam refresh token in the game's folder: `steam_settings/playden_online.txt`
(mode 0600), beside the bridge. It is not encrypted: a jailbroken console has nowhere safe to keep a key, and
anything that can read the game's folder can read the token. That is the same exposure as Playden's own saved
session (`session.v1`) when **Stay signed in** is on.

So Online works only while Stay signed in is on:

- With it off, or signed out, an Online game is prepared with the emulator alone and its token file is removed.
  `runtime.log` says `mode=online without a saved Steam session ... emulator only`.
- Switching a game back to Offline emulation or Game's own files removes the token file too.
- Turning Stay signed in off takes effect for each Online game the next time it is prepared (when it starts).

## Tested on a PS5

Firmware 13.60, kit 0.13 with prospero-win's `iphlpapi` patch (a game sees the console's network adapter; without it
EOS P2P cannot join a friend).

- RuneScape: Dragonwilds (1374490): signs in to its online services (Epic, with a Steam Web API ticket), lists and
  creates lobbies, and joins a friend's world.
- Palworld (1623730): joins servers from its Official Servers list (they check the encrypted app ticket).
- Yu-Gi-Oh! Master Duel (1449850): "no response from the game server". It fails the same way on a PC with real
  Steam.

## The log

`steam_settings/playden_online.log`, beside the bridge: logon result, presence, tickets handed out and Steam's acks,
other players' tickets checked, lobbies, invites, friends count, the interfaces bridged, and a line for every Steam
networking interface the game asks for. Never a token or ticket bytes. A fatal crash in the game leaves `CRASH` lines
(exception, module and offset, the modules on its stack).

## Not done

- **Steam networking** (ISteamNetworkingSockets/Messages and ISteamNetworking P2P through Steam's relays) is not
  bridged. Co-op that sends its traffic through Steam finds the lobby but does not connect.
- **Rich presence** and `InviteUserToGame` are not bridged.
- **Kernel anti-cheat** games stay unsupported.

## Building the bridge

`tools/steam-online/build.sh` (clang and lld-link from LLVM 18; no Windows SDK) writes
`runtime/steamworks/steam_api64_online.dll` and `steam_api_online.dll`. The build is reproducible; their hashes are
pinned in `src/runtime/steamworks.h`.
