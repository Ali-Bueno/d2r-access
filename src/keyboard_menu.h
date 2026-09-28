#pragma once

#include <D2RLPlugin/api.h>

#include <string_view>

// The keyboard in a menu. In keyboard mode the game keeps no focused widget
// (MEASURED 2026-09-27), so the mod keeps a cursor of its own and acts through
// the mouse, as the user's Diablo mod does (the user, 2026-09-27):
//
//   arrows                   move the cursor: control to control by where they
//                            are drawn, and by item inside a grid of items
//                            (menu_cursor.h). The pointer is put on the middle
//                            of what the cursor is on, so the game shows its
//                            tooltip, and the readers read the cursor as they
//                            read the controller's focus (NativeUi::SetCursor).
//   A, D                     left click, right click, where the cursor is (the
//                            user, 2026-09-27: A and D in a menu, as A and W
//                            are the two hands in play). With Shift or Ctrl
//                            held the game sees the click with it: Shift + left
//                            click equips, Ctrl + left click sells, moves or
//                            drops (the game's own hints say which). Ctrl+D is
//                            the whole description's switch, not a click.
//   Ctrl + arrows            read line by line: up and down a line, left and
//                            right the first and the last (InputDirection::PostLineStep).
//   PageUp, PageDown         the tab before and after, in a menu that has tabs.
//   Home, End                the menu before and after: character, inventory,
//                            skill tree, quest log, game menu.
//   backslash                say it again (Home says it again in play).
//   Shift+S                  the list of skills for the left hand, in play as
//                            well (the user, 2026-09-27). The game's own S opens
//                            the right hand's. The hand is said as the list opens.
//
// With an item in hand the cursor goes cell by cell: where the item would be
// put down matters cell by cell. A connected pad is not touched; with one
// connected the cursor wakes only when an arrow key is pressed.
// docs/keyboard-play.md
namespace D2RAccess::KeyboardMenu {
enum class Act { ClickLeft, ClickRight, TabPrevious, TabNext, MenuPrevious, MenuNext, SayAgain, Detail };

// After KeyboardService::Tick and before the readers, so what is read is
// where the cursor ends up.
void Tick(const D2RL::PluginContext* context);
void Reset();
// True while a menu is up and the keys above are the menu's.
bool Active();
// True while the cursor is on a box of text. The keys that type, alone or with
// Shift, are the box's then, and so are the left and right arrows; up and down
// leave the box. Any thread.
bool Typing();
// A key of the menu as the loader delivered it. The same keys are watched
// directly as well, for the screens where the loader delivers none.
void Deliver(Act act);
// A click on a button of the open menu by the name its layout gives it, for a
// key that presses it for the player. False when it is not shown.
bool Press(const D2RL::PluginContext* context, std::string_view button);
// A click on the HUD's button of the left hand's skill, which opens the list
// of skills for that hand, or shuts it when it is open for it. In play as in a
// menu. False when the button is not shown or gives no rectangle.
bool LeftSkillList(const D2RL::PluginContext* context);
// The game's own filling of the belt from the inventory (native_belt_fill.h),
// while the belt is in the menu. False when there was nothing to fill, the
// game refused, or the belt is not in the menu.
bool FillBelt(const D2RL::PluginContext* context);
// The game's own sorting (native_sort.h) of the grid the cursor is in, or of
// the player's inventory when it is in none. False when the game refused or
// there is no grid. What is under the cursor afterwards is read as any move is.
bool Sort(const D2RL::PluginContext* context);
}
