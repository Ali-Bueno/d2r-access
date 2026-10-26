# Player-to-player trade

What a trade says, in order (esES):

1. **Request.** The game shows its own prompt menu in `NpcDialogPanel` without a name. The mod
   puts the partner in: "canaria quiere comerciar contigo. Aceptar" for the player asked,
   "Pidiendo a canaria que comercie. Cancelar" for the one asking (`TradeService::NamePrompt`,
   matched against the game's `strTradeAgreeTo` / `strWaitingForOtherPlayer`).
2. **Window.** Once both agree, focus lands in the trade window, whose grids are named:
   "Comercio con canaria. Tu oferta. Fila 1, columna 1" (`grid_us`), "Oferta de canaria"
   (`grid_them`). The name comes from the window's own `name_them` box. Before, both read
   "Inventario".
3. **Partner changes**, queued: "Objetos de canaria: 2", "canaria ofrece 1000 de oro",
   "canaria ha aceptado el trato" / "canaria ya no acepta el trato".
4. **This player's acceptance** as the server holds it: "Has aceptado el trato" / "Ya no aceptas
   el trato". The server withdraws both acceptances whenever either offer changes, so this is
   the cue to accept again.
5. **End**: "Comercio completado" or "Comercio cancelado". Without the completion hook
   (`[native] enabled = false`) it says "Comercio terminado".
6. **Busy notice**: the game builds it as name + `strTradeBusy`, and esES drops the space
   ("canariano está disponible."); `TradeService::FixNotice` puts it back.

Code: `src/native_trade.*` (reads, hook), `src/trade.*` (tracker, phrases; `tests/trade_tests.cpp`),
`src/trade_service.*` (100 ms sampling, speech), `interaction_reader.cpp` (grid names).
`[trade] enabled` in the settings file, `access-trade on|off` for the run.

## Live evidence (2026-09-25 log, two clients, partner "canaria")

- The asker's `NpcDialogPanel` read "Pidiendo a otro jugador que comercie. Cancelar"; the one
  asked got "Aceptar comerciar. Aceptar" opened for them, with no notice naming the asker.
- `TradeLayout` opens together with the prompt (while the trade is only pending), so the generic
  "Comerciar" title was spoken and cut by the prompt.
- Gold is offered through the inventory's gold button ("Comerciar con oro" -> `DropGoldModal`).
- `TradePanelMessage:Confirm` twice (the offer changed in between); the window closed and the
  carried gold fell by the 1000 offered: a completed trade.

## Native evidence (D2R 3.3.93787, RVAs from image base `0x140000000`)

D2R does not use the classic 0x77 UI actions for trading. S2C `0x2E` (dispatcher `0x12DB30`)
switches on its second byte: 1 state snapshot (`0x1EDE70`), 2 cancelled/declined (`0x1EDBA0`),
3 **completed** (`0x1EDC50`), 4 aborted (`0x1EDB30`). The server sends 3 to both players only
after both accepted and the exchange validated (`0x5288E0`). C2S `0x2B` is the request, `0x2C`
the reply/accept/cancel.

Trade module globals (all rip-relative in the fingerprinted functions):

| RVA | Meaning | Written by |
|---|---|---|
| `0x2A96AB0` dword | pending request id | `0x1ED7E0` (asking), `0x1EDE70` |
| `0x2A96AB8` dword | active trade id, non-zero once both agreed | `0x1EDE70`; reset `0x1F32C0` |
| `0x2A96AC0` dword | partner unit id, -1 none | snapshot pkt+0x1B, `0x1ED7E0` |
| `0x2A96AC4` dword | partner gold | pkt+0x17 |
| `0x2A96AC8` dword | partner accepted | pkt+0x20 |
| `0x2A96AD0`/`AD8` | partner item records (16 bytes) / count | `0x1F1410` |
| `0x2A96AF0` dword | this player accepted | pkt+0x13 |
| `0x2A96B10` byte | this client asked | `0x1ED7E0` |

- **Name:** `0x1EFF60` = roster(`0x2A96AC0`): `0x136200(unitId)` walks the party roster (head
  `0x2A4DB90`), and `0x136300(entry)` returns `entry + 0xFF`, the name. The plugin calls the
  first and reads the name itself (the getter asserts on null).
- **Completion hook:** `0x1EDC50(packet)` takes only `rcx`; prefix `48 89 5C 24 10 57 48 83 EC
  30 48 8B F9`. It also restores the weapon-swap flag saved when the window opened
  (`docs/weapon-swap.md`). Every ending runs the reset `0x1F32C0`, so polling alone cannot tell
  completed from cancelled.
- **The partner's items** are client-made copies in this player's own inventory, page byte
  `ItemData+0x55 == 1` (`0x1F2450`, `0x36CFE0`); `grid_them` is bound to page 1, `grid_us` to 2
  (`0x2E5B90`), so the grid cell reader should name them (not yet seen live).
- `TradePanel::Update` (`0x2E4EE0`) swaps `ready_them`'s frames (ButtonWidget `+0x7E8/+0x7EC`,
  via `0x8706D0`) on the partner-accepted flag; the mod reads the flag instead.
- The UI binding store mirrors the flags (`0x8461C0()` + `0x11C860`, keys partner accepted
  `0x26DE2837DF8BADC2`, we accepted `0x09B5174D9DE80534`, accept locked `0x92290AB1A63553B9`).
  The 10 s lock after an offer changes (`0x2A96B58`) is not announced.

## Live check pending

A two-client trade: both prompts name the partner; the grids name the offer side; items and gold
offered by the partner are told; both acceptances; completed and cancelled endings; the partner's
items read in `grid_them`.

## The gold window's title (2026-10-20)

A tester at the stash heard "Drop Gold - Deposit". One window (`DropGoldModal`) drops, deposits,
withdraws and offers gold; its layout's title is always Drop Gold (`strDropGoldInfo`) and the game
sets the shown one as it opens. The announcer now reads the live `Title` 120 ms after the window
opens (`UiMessageMonitor`, `RuntimeTitlePanels`) and says that, the layout's only when it reads
empty. Log: `[modal] DropGoldModal title: "..."`. Unverified live: what said "Deposit" the
first time is unknown, so the title may be heard twice.

## Whose offer, said alone (2026-10-25b)

Live (tester, `trade_` log, partner "Bronwyn"): the grids were named, as "Trade with Bronwyn. Your
offer. Row 1, column 1" and "Trade with Bronwyn. Bronwyn's offer. Row 4, column 6", but the next step
came within 0.3 s and interrupted them after the window's title, so the tester never heard whose offer
it was. A trade grid's body is now the side alone ("Bronwyn's offer", "Your offer"), said when the
focus enters that grid; the trade itself is named when its window opens and in the partner's
messages. `Inventory()` in `interaction_reader.cpp`.
