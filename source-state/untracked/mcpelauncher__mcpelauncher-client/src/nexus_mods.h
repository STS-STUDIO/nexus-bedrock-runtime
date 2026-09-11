#pragma once

// Nexus mods: the in-game module grid, the HUD modules it drives and the
// gameplay hooks that have no HUD box. Everything here persists through
// Settings, using the same mcpelauncher-client-settings.txt keys nexus8
// shipped, so an existing player's layout survives the update.

#include <game_window.h>

struct ImFont;

namespace NexusMods {

// Corner presets a HUD module can snap to.
enum Corner {
    CORNER_TOP_LEFT = 0,
    CORNER_TOP_RIGHT = 1,
    CORNER_BOTTOM_LEFT = 2,
    CORNER_BOTTOM_RIGHT = 3,
    CORNER_COUNT = 4
};

struct Fonts {
    ImFont* small;   // 15px
    ImFont* medium;  // 18px
    ImFont* large;   // 24px
    ImFont* huge;    // 36px
};

// Called once from ImGuiUIInit after the font atlas is built.
void setFonts(Fonts const& fonts);

bool isMenuOpen();
void setMenuOpen(bool open);
void toggleMenu();

// Per-frame entry points, called from ImGuiUIDrawFrame.
void handleMenuHotkey();           // Right Shift (or whatever nexus_menu_key names)
void drawMenubarSection(GameWindow* window);
void drawMenu(GameWindow* window);
void drawHud(GameWindow* window);

// Toggle Sprint. Called from the keyboard callback before the event reaches
// the game. Returns the action the game should see, or -1 to drop the event.
// key/action are KeyCode / KeyAction cast to int.
int filterSprintKey(int key, int action);

}  // namespace NexusMods
