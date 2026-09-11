#include "nexus_mods.h"

#include "core_patches.h"
#include "settings.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

#include <key_mapping.h>

namespace {

// ---------------------------------------------------------------------------
// Palette. Dark navy ground, launcher purple accent. Everything the HUD and the
// menu paint comes from here so the two read as one product.
// ---------------------------------------------------------------------------

const ImVec4 COL_BG = ImVec4(0.043f, 0.047f, 0.078f, 1.0f);  // #0B0C14
const ImVec4 COL_PANEL = ImVec4(0.071f, 0.078f, 0.118f, 1.0f);
const ImVec4 COL_CARD = ImVec4(0.098f, 0.106f, 0.157f, 1.0f);
const ImVec4 COL_CARD_HOVER = ImVec4(0.137f, 0.149f, 0.216f, 1.0f);
const ImVec4 COL_LINE = ImVec4(1.0f, 1.0f, 1.0f, 0.07f);
const ImVec4 COL_TEXT = ImVec4(0.909f, 0.921f, 0.960f, 1.0f);
const ImVec4 COL_MUTED = ImVec4(0.545f, 0.565f, 0.651f, 1.0f);
const ImVec4 COL_OFF = ImVec4(0.352f, 0.372f, 0.443f, 1.0f);

struct AccentDef {
    char const* name;
    ImVec4 color;
};

const AccentDef ACCENTS[] = {
    {"NEXUS", ImVec4(0.588f, 0.322f, 0.941f, 1.0f)},  // #9652F0
    {"SKY", ImVec4(0.220f, 0.741f, 0.973f, 1.0f)},
    {"MINT", ImVec4(0.204f, 0.827f, 0.600f, 1.0f)},
    {"AMBER", ImVec4(0.961f, 0.620f, 0.043f, 1.0f)},
    {"ROSE", ImVec4(0.984f, 0.443f, 0.522f, 1.0f)},
};
const int ACCENT_COUNT = (int)(sizeof(ACCENTS) / sizeof(ACCENTS[0]));

ImVec4 accent() {
    int i = Settings::nexus_hud_accent;
    if(i < 0 || i >= ACCENT_COUNT) {
        i = 0;
    }
    return ACCENTS[i].color;
}

ImU32 col(ImVec4 c, float alpha = 1.0f) {
    c.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}

// ---------------------------------------------------------------------------
// Fonts. The atlas holds four sizes, each already multiplied by Settings::scale.
// Ask for a logical size and get the closest atlas face plus the exact pixel
// size to render it at, so nothing is scaled further than it has to be.
// ---------------------------------------------------------------------------

NexusMods::Fonts g_fonts = {nullptr, nullptr, nullptr, nullptr};

struct Face {
    ImFont* font;
    float px;
};

// The base UI scale is the launcher's own global scale (HiDPI and the like).
// The mod menu can additionally enlarge itself by the player's Menu Size setting
// without touching the HUD: drawMenu raises g_uiScaleOverride for the duration of
// its own draw and clears it afterwards. Every menu widget sizes off ui(), so
// they all grow together, while drawHud (called separately, with the override
// cleared) keeps rendering the HUD at the base scale it always used.
float g_uiScaleOverride = 0.0f;

float baseUi() {
    float s = (float)Settings::scale;
    return s > 0.01f ? s : 1.0f;
}

float ui() {
    if(g_uiScaleOverride > 0.01f) {
        return g_uiScaleOverride;
    }
    return baseUi();
}

// Menu Size, clamped. Defaults to 1.3x so the menu reads comfortably at 1440p
// and on small laptop screens.
float menuScaleFactor() {
    float m = Settings::nexus_menu_scale;
    if(m < 0.8f) m = 0.8f;
    if(m > 2.0f) m = 2.0f;
    return m;
}

Face face(float logical) {
    float px = logical * ui();
    ImFont* candidates[4] = {g_fonts.small, g_fonts.medium, g_fonts.large, g_fonts.huge};
    const float bases[4] = {15.0f, 18.0f, 24.0f, 36.0f};
    ImFont* best = nullptr;
    float bestDelta = 0.0f;
    for(int i = 0; i < 4; i++) {
        if(!candidates[i]) {
            continue;
        }
        float delta = std::fabs(bases[i] - logical);
        if(!best || delta < bestDelta) {
            best = candidates[i];
            bestDelta = delta;
        }
    }
    if(!best) {
        best = ImGui::GetFont();
        px = ImGui::GetFontSize();
    }
    return Face{best, px};
}

ImVec2 measure(Face const& f, char const* text, float wrap = 0.0f) {
    return f.font->CalcTextSizeA(f.px, FLT_MAX, wrap, text);
}

void drawText(ImDrawList* dl, Face const& f, ImVec2 pos, ImU32 color, char const* text, float wrap = 0.0f) {
    dl->AddText(f.font, f.px, pos, color, text, nullptr, wrap);
}

// ---------------------------------------------------------------------------
// Module registry
// ---------------------------------------------------------------------------

enum Category {
    CAT_COMBAT = 0,
    CAT_DISPLAY,
    CAT_GAMEPLAY,
    CAT_PERKS,
    CAT_COUNT
};

char const* CATEGORY_NAMES[CAT_COUNT] = {"COMBAT", "DISPLAY", "GAMEPLAY", "PERKS"};

enum Special {
    SPECIAL_NONE = 0,
    SPECIAL_VSYNC,
    SPECIAL_FULLSCREEN,
    SPECIAL_COMING_SOON  // announced, visible in the grid, not toggleable yet
};

struct Module {
    char const* id;
    char const* name;
    char const* subtitle;
    char const* description;
    int category;
    int* enabled;   // nullptr when special
    int* pos;       // nullptr when the module draws no HUD box
    float* scale;   // nullptr when nothing about it is resizable
    int special;
};

Module MODULES[] = {
    {"fps", "FPS Counter", "Frames per second",
     "Live frames per second, sampled from the render loop every frame.",
     CAT_DISPLAY, &Settings::nexus_fps, &Settings::nexus_fps_pos, &Settings::nexus_fps_scale, SPECIAL_NONE},
    {"fpsgraph", "FPS Graph", "Frame rate drawn as a live graph",
     "Plots your frame rate over the last few seconds as a filled graph, with the current value read out. It samples the same render loop the FPS counter does, so the two always agree.",
     CAT_DISPLAY, &Settings::nexus_fps_graph, &Settings::nexus_fps_graph_pos, &Settings::nexus_fps_graph_scale, SPECIAL_NONE},
    {"cps", "CPS Counter", "Left and right clicks per second",
     "Counts your left and right mouse clicks over a rolling one second window. The PvP click speed readout.",
     CAT_COMBAT, &Settings::nexus_cps, &Settings::nexus_cps_pos, &Settings::nexus_cps_scale, SPECIAL_NONE},
    {"keystrokes", "Keystrokes", "WASD and clicks light up",
     "WASD and the mouse buttons drawn as keycaps that light up as you press them.",
     CAT_DISPLAY, &Settings::nexus_keystrokes, &Settings::nexus_keystrokes_pos, &Settings::nexus_keystrokes_scale, SPECIAL_NONE},
    {"session", "Session Timer", "Time played this session",
     "How long this Minecraft session has been running, counted from launch.",
     CAT_DISPLAY, &Settings::nexus_session_timer, &Settings::nexus_session_timer_pos, &Settings::nexus_session_timer_scale, SPECIAL_NONE},
    {"clock", "Real Clock", "Your local wall clock time",
     "Your computer's local time, so you can watch the clock without leaving the game.",
     CAT_DISPLAY, &Settings::nexus_real_clock, &Settings::nexus_real_clock_pos, &Settings::nexus_real_clock_scale, SPECIAL_NONE},
    {"watermark", "Watermark", "NEXUS branding on your screen",
     "Draws the NEXUS wordmark on your HUD so clips and screenshots carry the branding.",
     CAT_DISPLAY, &Settings::nexus_watermark, &Settings::nexus_watermark_pos, &Settings::nexus_watermark_scale, SPECIAL_NONE},
    {"crosshair", "Custom Crosshair", "Clean Nexus cross at screen centre",
     "Draws a thin Nexus cross at the exact centre of the screen. It is always centred, so there is no corner to pick, but you can resize it.",
     CAT_DISPLAY, &Settings::nexus_crosshair, nullptr, &Settings::nexus_crosshair_scale, SPECIAL_NONE},
    {"togglesprint", "Toggle Sprint", "Holds sprint while you play",
     "Keeps sprint held for you instead of making you hold the key down. Tap the sprint key once to start sprinting and once more to stop.",
     CAT_GAMEPLAY, &Settings::nexus_toggle_sprint, nullptr, nullptr, SPECIAL_NONE},
    {"discord", "Discord Button", "Invite button on the pause screen",
     "Shows a Join Discord button while the game is paused. It opens the STS Studio invite in your browser.",
     CAT_PERKS, &Settings::nexus_discord_button, nullptr, nullptr, SPECIAL_NONE},
    {"vsync", "VSync", "Sync framerate to your display",
     "Locks the frame rate to your monitor's refresh rate. Removes tearing, costs a little input latency.",
     CAT_DISPLAY, nullptr, nullptr, nullptr, SPECIAL_VSYNC},
    {"fullscreen", "Fullscreen", "Give Minecraft the whole screen",
     "Puts the game window into fullscreen on your current display.",
     CAT_DISPLAY, nullptr, nullptr, nullptr, SPECIAL_FULLSCREEN},
    {"replay", "Replay Mod", "Record and rewatch your gameplay",
     "Record your matches and play them back afterwards with a free moving camera. This one is in active development and is not available to switch on yet.",
     CAT_GAMEPLAY, nullptr, nullptr, nullptr, SPECIAL_COMING_SOON},
};
const int MODULE_COUNT = (int)(sizeof(MODULES) / sizeof(MODULES[0]));

bool moduleEnabled(Module const& m, GameWindow* window) {
    switch(m.special) {
    case SPECIAL_VSYNC:
        return Settings::vsync;
    case SPECIAL_FULLSCREEN:
        return window ? window->getFullscreen() : Settings::fullscreen;
    case SPECIAL_COMING_SOON:
        return false;
    default:
        return m.enabled && *m.enabled != 0;
    }
}

void setModuleEnabled(Module const& m, GameWindow* window, bool on) {
    switch(m.special) {
    case SPECIAL_VSYNC:
        Settings::vsync = on;
        if(window) {
            window->setSwapInterval(on ? 1 : 0);
        }
        break;
    case SPECIAL_FULLSCREEN:
        if(window) {
            window->setFullscreen(on);
        }
        Settings::fullscreen = on;
        break;
    case SPECIAL_COMING_SOON:
        return;  // not switchable yet, and nothing to persist
    default:
        if(m.enabled) {
            *m.enabled = on ? 1 : 0;
        }
        break;
    }
    Settings::save();
}

// ---------------------------------------------------------------------------
// Menu state
// ---------------------------------------------------------------------------

bool g_menuOpen = false;
float g_menuAnim = 0.0f;
int g_openSettings = -1;  // index into MODULES, -1 shows the grid
int g_categoryFilter = -1;
char g_search[64] = "";
bool g_resetScroll = true;
bool g_cursorFreed = false;
std::unordered_map<std::string, float> g_anim;

std::chrono::steady_clock::time_point g_sessionStart = std::chrono::steady_clock::now();

// Click history for the CPS module. Kept here so the module owns its own state
// rather than sharing the upstream keystroke HUD's.
std::vector<long long> g_lmb;
std::vector<long long> g_rmb;
bool g_lmbLast = false;
bool g_rmbLast = false;
int g_lcps = 0;
int g_rcps = 0;

// Toggle Sprint runtime state.
bool g_sprintHeld = false;

// FPS graph rolling history, newest last. One sample per frame; capped so the
// graph always shows roughly the last couple of seconds of frames.
std::vector<float> g_fpsHistory;
const int FPS_HISTORY_MAX = 120;

float& anim(char const* key) {
    return g_anim[key];
}

float ease(float current, float target, float speed) {
    float dt = ImGui::GetIO().DeltaTime;
    if(dt <= 0.0f || dt > 0.25f) {
        dt = 1.0f / 60.0f;
    }
    float next = current + (target - current) * (1.0f - std::exp(-dt * speed));
    if(std::fabs(target - next) < 0.001f) {
        next = target;
    }
    return next;
}

ImGuiKey menuHotkey() {
    std::string const& k = Settings::nexus_menu_key;
    if(k == "RightControl") return ImGuiKey_RightCtrl;
    if(k == "RightAlt") return ImGuiKey_RightAlt;
    if(k == "Insert") return ImGuiKey_Insert;
    if(k == "Home") return ImGuiKey_Home;
    if(k == "F1") return ImGuiKey_F1;
    if(k == "F2") return ImGuiKey_F2;
    if(k == "F3") return ImGuiKey_F3;
    if(k == "F4") return ImGuiKey_F4;
    if(k == "F6") return ImGuiKey_F6;
    if(k == "F7") return ImGuiKey_F7;
    if(k == "F8") return ImGuiKey_F8;
    if(k == "F9") return ImGuiKey_F9;
    if(k == "F10") return ImGuiKey_F10;
    return ImGuiKey_RightShift;
}

char const* menuHotkeyLabel() {
    std::string const& k = Settings::nexus_menu_key;
    if(k == "RightControl") return "Right Ctrl";
    if(k == "RightAlt") return "Right Alt";
    if(k.rfind("F", 0) == 0 || k == "Insert" || k == "Home") return k.c_str();
    return "Right Shift";
}

void openUrl(char const* url) {
#ifdef __APPLE__
    std::string cmd = std::string("open '") + url + "' >/dev/null 2>&1 &";
#else
    std::string cmd = std::string("xdg-open '") + url + "' >/dev/null 2>&1 &";
#endif
    std::system(cmd.c_str());
}

// ---------------------------------------------------------------------------
// Small painted widgets. Everything is drawn by hand so the menu and the HUD
// share one look instead of inheriting the default ImGui theme.
// ---------------------------------------------------------------------------

// Rounded card with a hover lift. Returns true when clicked.
bool cardFrame(char const* id, ImVec2 size, bool active, float* outHover) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    float& h = anim(id);
    h = ease(h, hovered ? 1.0f : 0.0f, 18.0f);
    if(outHover) {
        *outHover = h;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 bg = ImVec4(COL_CARD.x + (COL_CARD_HOVER.x - COL_CARD.x) * h,
                       COL_CARD.y + (COL_CARD_HOVER.y - COL_CARD.y) * h,
                       COL_CARD.z + (COL_CARD_HOVER.z - COL_CARD.z) * h,
                       1.0f);
    float rounding = 10.0f * ui();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col(bg), rounding);
    if(active) {
        dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), col(accent(), 0.55f + 0.25f * h), rounding, 0, 1.5f * ui());
    } else {
        dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), col(COL_LINE, 1.0f + h), rounding, 0, 1.0f * ui());
    }
    return clicked;
}

// Animated on/off switch. Draws only; the caller owns the hit test.
void drawSwitch(ImDrawList* dl, ImVec2 p, float height, bool on, float t) {
    float w = height * 1.9f;
    float r = height * 0.5f;
    ImVec2 a = p;
    ImVec2 b = ImVec2(p.x + w, p.y + height);
    ImVec4 offBg = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
    ImVec4 ac = accent();
    ImVec4 bg = ImVec4(offBg.x + (ac.x - offBg.x) * t,
                       offBg.y + (ac.y - offBg.y) * t,
                       offBg.z + (ac.z - offBg.z) * t,
                       offBg.w + (0.95f - offBg.w) * t);
    dl->AddRectFilled(a, b, col(bg), r);
    float knobX = p.x + r + (w - height) * t;
    dl->AddCircleFilled(ImVec2(knobX, p.y + r), r - 2.0f * ui(),
                        col(on ? ImVec4(1, 1, 1, 1) : COL_OFF), 16);
}

void drawGear(ImDrawList* dl, ImVec2 center, float radius, ImU32 color) {
    dl->AddCircle(center, radius * 0.52f, color, 14, 1.6f * ui());
    for(int i = 0; i < 6; i++) {
        float a = (float)i * 3.14159265f / 3.0f;
        ImVec2 inner = ImVec2(center.x + std::cos(a) * radius * 0.72f, center.y + std::sin(a) * radius * 0.72f);
        ImVec2 outer = ImVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
        dl->AddLine(inner, outer, color, 1.8f * ui());
    }
}

// Pill button used across the menu. Returns true when clicked.
bool pillButton(char const* id, char const* label, ImVec2 size, bool primary, bool enabled = true) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    bool hovered = enabled && ImGui::IsItemHovered();
    bool clicked = enabled && ImGui::IsItemClicked(ImGuiMouseButton_Left);
    float& h = anim(id);
    h = ease(h, hovered ? 1.0f : 0.0f, 20.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 b = ImVec2(p.x + size.x, p.y + size.y);
    float rounding = size.y * 0.5f;
    ImVec4 ac = accent();
    if(!enabled) {
        dl->AddRectFilled(p, b, col(ImVec4(1, 1, 1, 0.05f)), rounding);
    } else if(primary) {
        dl->AddRectFilled(p, b, col(ac, 0.78f + 0.22f * h), rounding);
    } else {
        dl->AddRectFilled(p, b, col(ImVec4(1, 1, 1, 0.06f + 0.06f * h)), rounding);
        dl->AddRect(p, b, col(COL_LINE, 1.0f + h), rounding, 0, 1.0f * ui());
    }
    Face f = face(15.0f);
    ImVec2 ts = measure(f, label);
    ImU32 textColor = !enabled ? col(COL_OFF) : (primary ? col(ImVec4(1, 1, 1, 1)) : col(COL_TEXT));
    drawText(dl, f, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), textColor, label);
    return clicked;
}

bool chipButton(char const* id, char const* label, bool selected) {
    Face f = face(15.0f);
    ImVec2 ts = measure(f, label);
    ImVec2 size = ImVec2(ts.x + 22.0f * ui(), 26.0f * ui());
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    float& h = anim(id);
    h = ease(h, (hovered || selected) ? 1.0f : 0.0f, 20.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 b = ImVec2(p.x + size.x, p.y + size.y);
    float rounding = size.y * 0.5f;
    if(selected) {
        dl->AddRectFilled(p, b, col(accent(), 0.22f), rounding);
        dl->AddRect(p, b, col(accent(), 0.75f), rounding, 0, 1.2f * ui());
    } else {
        dl->AddRectFilled(p, b, col(ImVec4(1, 1, 1, 0.04f + 0.05f * h)), rounding);
    }
    drawText(dl, f, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f),
             selected ? col(COL_TEXT) : col(COL_MUTED), label);
    return clicked;
}

std::string toLower(std::string s) {
    for(auto& c : s) {
        c = (char)std::tolower((unsigned char)c);
    }
    return s;
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------

int cpsFor(std::vector<long long>& history, bool down, bool& last) {
    long long now = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    if(down && !last) {
        history.push_back(now);
    }
    last = down;
    while(!history.empty() && now - history.front() > 1000) {
        history.erase(history.begin());
    }
    return (int)history.size();
}

void updateClickHistory() {
    g_lcps = cpsFor(g_lmb, ImGui::IsKeyDown(ImGuiKey_MouseLeft), g_lmbLast);
    g_rcps = cpsFor(g_rmb, ImGui::IsKeyDown(ImGuiKey_MouseRight), g_rmbLast);
}

void updateFpsHistory() {
    g_fpsHistory.push_back(ImGui::GetIO().Framerate);
    if((int)g_fpsHistory.size() > FPS_HISTORY_MAX) {
        g_fpsHistory.erase(g_fpsHistory.begin());
    }
}

ImGuiKey letterKey(char c) {
    if(c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    if(c >= 'A' && c <= 'Z') {
        return (ImGuiKey)(ImGuiKey_A + (c - 'A'));
    }
    return ImGuiKey_None;
}

float moduleScale(Module const& m) {
    float s = m.scale ? *m.scale : 1.0f;
    if(s < 0.5f) s = 0.5f;
    if(s > 2.5f) s = 2.5f;
    return s * ui();
}

struct HudText {
    std::string head;   // rendered in the accent colour
    std::string tail;   // rendered muted, after a space
};

HudText hudTextFor(Module const& m) {
    HudText out;
    char buf[64];
    if(std::strcmp(m.id, "fps") == 0) {
        std::snprintf(buf, sizeof(buf), "%d", (int)(ImGui::GetIO().Framerate + 0.5f));
        out.head = buf;
        out.tail = "FPS";
    } else if(std::strcmp(m.id, "cps") == 0) {
        std::snprintf(buf, sizeof(buf), "%d | %d", g_lcps, g_rcps);
        out.head = buf;
        out.tail = "CPS";
    } else if(std::strcmp(m.id, "session") == 0) {
        long long secs = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::steady_clock::now() - g_sessionStart)
                             .count();
        if(secs >= 3600) {
            std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", secs / 3600, (secs / 60) % 60, secs % 60);
        } else {
            std::snprintf(buf, sizeof(buf), "%lld:%02lld", secs / 60, secs % 60);
        }
        out.head = buf;
        out.tail = "SESSION";
    } else if(std::strcmp(m.id, "clock") == 0) {
        std::time_t t = std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &t);
#else
        localtime_r(&t, &local);
#endif
        std::strftime(buf, sizeof(buf), "%H:%M", &local);
        out.head = buf;
        out.tail = "";
    } else if(std::strcmp(m.id, "watermark") == 0) {
        out.head = "NEXUS";
        out.tail = "LAUNCHER";
    }
    return out;
}

ImVec2 measureKeystrokes(float s) {
    float cap = 30.0f * s;
    float gap = 4.0f * s;
    float w = cap * 3 + gap * 2;
    float h = cap + gap + cap + gap + cap * 0.55f + gap + cap * 0.78f;
    return ImVec2(w, h);
}

ImVec2 measureHud(Module const& m, float s) {
    if(std::strcmp(m.id, "keystrokes") == 0) {
        return measureKeystrokes(s);
    }
    if(std::strcmp(m.id, "fpsgraph") == 0) {
        return ImVec2(134.0f * s, 54.0f * s);
    }
    HudText t = hudTextFor(m);
    Face big = face(19.0f / ui() * s);
    Face small = face(12.0f / ui() * s);
    ImVec2 headSize = measure(big, t.head.c_str());
    float width = headSize.x;
    if(!t.tail.empty()) {
        width += 6.0f * s + measure(small, t.tail.c_str()).x;
    }
    float padX = 12.0f * s;
    float padY = 7.0f * s;
    return ImVec2(width + padX * 2 + 4.0f * s, headSize.y + padY * 2);
}

void drawKeycap(ImDrawList* dl, ImVec2 a, ImVec2 b, char const* label, bool down, float s, float opacity) {
    float rounding = 5.0f * s;
    ImVec4 ac = accent();
    if(down) {
        dl->AddRectFilled(a, b, col(ac, 0.85f), rounding);
    } else {
        dl->AddRectFilled(a, b, col(COL_BG, opacity), rounding);
        dl->AddRect(a, b, col(ImVec4(1, 1, 1, 0.13f)), rounding, 0, 1.0f * s);
    }
    if(label && label[0]) {
        Face f = face(13.0f / ui() * s);
        ImVec2 ts = measure(f, label);
        drawText(dl, f,
                 ImVec2(a.x + ((b.x - a.x) - ts.x) * 0.5f, a.y + ((b.y - a.y) - ts.y) * 0.5f),
                 down ? col(ImVec4(1, 1, 1, 1)) : col(COL_TEXT, 0.85f), label);
    }
}

void drawHudModule(ImDrawList* dl, Module const& m, ImVec2 pos, ImVec2 size, float s) {
    float opacity = Settings::nexus_hud_opacity;
    if(opacity < 0.0f) opacity = 0.0f;
    if(opacity > 1.0f) opacity = 1.0f;

    if(std::strcmp(m.id, "keystrokes") == 0) {
        float cap = 30.0f * s;
        float gap = 4.0f * s;
        char up = GameOptions::fullKeyboard ? GameOptions::upKeyFullKeyboard : GameOptions::upKey;
        char left = GameOptions::fullKeyboard ? GameOptions::leftKeyFullKeyboard : GameOptions::leftKey;
        char down = GameOptions::fullKeyboard ? GameOptions::downKeyFullKeyboard : GameOptions::downKey;
        char right = GameOptions::fullKeyboard ? GameOptions::rightKeyFullKeyboard : GameOptions::rightKey;
        char lbl[2] = {0, 0};

        float y = pos.y;
        lbl[0] = up;
        drawKeycap(dl, ImVec2(pos.x + cap + gap, y), ImVec2(pos.x + cap * 2 + gap, y + cap), lbl,
                   ImGui::IsKeyDown(letterKey(up)), s, opacity);
        y += cap + gap;
        char row[3] = {left, down, right};
        for(int i = 0; i < 3; i++) {
            lbl[0] = row[i];
            float x = pos.x + (cap + gap) * i;
            drawKeycap(dl, ImVec2(x, y), ImVec2(x + cap, y + cap), lbl,
                       ImGui::IsKeyDown(letterKey(row[i])), s, opacity);
        }
        y += cap + gap;
        drawKeycap(dl, ImVec2(pos.x, y), ImVec2(pos.x + size.x, y + cap * 0.55f), "",
                   ImGui::IsKeyDown(ImGuiKey_Space), s, opacity);
        {
            // The space bar reads as a bar rather than a letter.
            float cy = y + cap * 0.275f;
            dl->AddLine(ImVec2(pos.x + size.x * 0.3f, cy), ImVec2(pos.x + size.x * 0.7f, cy),
                        col(ImGui::IsKeyDown(ImGuiKey_Space) ? ImVec4(1, 1, 1, 1) : COL_TEXT, 0.85f), 1.6f * s);
        }
        y += cap * 0.55f + gap;
        float half = (size.x - gap) * 0.5f;
        drawKeycap(dl, ImVec2(pos.x, y), ImVec2(pos.x + half, y + cap * 0.78f), "LMB",
                   ImGui::IsKeyDown(ImGuiKey_MouseLeft), s, opacity);
        drawKeycap(dl, ImVec2(pos.x + half + gap, y), ImVec2(pos.x + size.x, y + cap * 0.78f), "RMB",
                   ImGui::IsKeyDown(ImGuiKey_MouseRight), s, opacity);
        return;
    }

    if(std::strcmp(m.id, "fpsgraph") == 0) {
        ImVec2 b = ImVec2(pos.x + size.x, pos.y + size.y);
        float rounding = 7.0f * s;
        dl->AddRectFilled(pos, b, col(COL_BG, opacity), rounding);
        dl->AddRect(pos, b, col(ImVec4(1, 1, 1, 0.08f)), rounding, 0, 1.0f * s);
        dl->AddRectFilled(pos, ImVec2(pos.x + 3.0f * s, b.y), col(accent(), 0.9f), rounding);

        float padX = 12.0f * s + 4.0f * s;
        float padY = 6.0f * s;

        // Current value, top left, sharing the HUD's head/tail styling.
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", (int)(ImGui::GetIO().Framerate + 0.5f));
        Face big = face(16.0f / ui() * s);
        Face small = face(11.0f / ui() * s);
        ImVec2 headSize = measure(big, buf);
        drawText(dl, big, ImVec2(pos.x + padX, pos.y + padY), col(COL_TEXT), buf);
        ImVec2 tailSize = measure(small, "FPS");
        drawText(dl, small, ImVec2(pos.x + padX + headSize.x + 5.0f * s, pos.y + padY + headSize.y - tailSize.y - 1.0f * s),
                 col(accent(), 0.95f), "FPS");

        // The graph fills the width below the readout.
        float gx0 = pos.x + padX;
        float gx1 = b.x - 12.0f * s;
        float gy0 = pos.y + padY + headSize.y + 3.0f * s;
        float gy1 = b.y - padY;
        int n = (int)g_fpsHistory.size();
        if(gx1 > gx0 + 4.0f && gy1 > gy0 + 2.0f && n > 1) {
            float maxFps = 1.0f;
            for(float v : g_fpsHistory) {
                if(v > maxFps) maxFps = v;
            }
            float top = maxFps * 1.1f;  // headroom so a spike is not clipped
            float gw = gx1 - gx0;
            float gh = gy1 - gy0;
            ImVec2 prev;
            for(int i = 0; i < n; i++) {
                float fx = gx0 + gw * ((float)i / (float)(n - 1));
                float norm = g_fpsHistory[i] / top;
                if(norm < 0.0f) norm = 0.0f;
                if(norm > 1.0f) norm = 1.0f;
                float fy = gy1 - gh * norm;
                if(i > 0) {
                    dl->AddQuadFilled(prev, ImVec2(fx, fy), ImVec2(fx, gy1), ImVec2(prev.x, gy1), col(accent(), 0.18f));
                    dl->AddLine(prev, ImVec2(fx, fy), col(accent(), 0.95f), 1.4f * s);
                }
                prev = ImVec2(fx, fy);
            }
        }
        return;
    }

    HudText t = hudTextFor(m);
    ImVec2 b = ImVec2(pos.x + size.x, pos.y + size.y);
    float rounding = 7.0f * s;
    dl->AddRectFilled(pos, b, col(COL_BG, opacity), rounding);
    dl->AddRect(pos, b, col(ImVec4(1, 1, 1, 0.08f)), rounding, 0, 1.0f * s);
    // Accent rail down the leading edge, so every module reads as one family.
    dl->AddRectFilled(pos, ImVec2(pos.x + 3.0f * s, b.y), col(accent(), 0.9f), rounding);

    Face big = face(19.0f / ui() * s);
    Face small = face(12.0f / ui() * s);
    ImVec2 headSize = measure(big, t.head.c_str());
    float x = pos.x + 12.0f * s + 4.0f * s;
    float y = pos.y + (size.y - headSize.y) * 0.5f;
    drawText(dl, big, ImVec2(x, y), col(COL_TEXT), t.head.c_str());
    if(!t.tail.empty()) {
        ImVec2 tailSize = measure(small, t.tail.c_str());
        drawText(dl, small,
                 ImVec2(x + headSize.x + 6.0f * s, y + (headSize.y - tailSize.y) * 0.72f),
                 col(accent(), 0.95f), t.tail.c_str());
    }
}

Module const* moduleById(char const* id) {
    for(int i = 0; i < MODULE_COUNT; i++) {
        if(std::strcmp(MODULES[i].id, id) == 0) {
            return &MODULES[i];
        }
    }
    return nullptr;
}

void drawCrosshair(ImDrawList* dl, ImVec2 center) {
    Module const* crosshair = moduleById("crosshair");
    float s = crosshair ? moduleScale(*crosshair) : ui();
    float arm = 9.0f * s;
    float gap = 3.0f * s;
    float thickness = 2.0f * s;
    ImU32 c = col(accent(), 0.95f);
    ImU32 shadow = col(ImVec4(0, 0, 0, 0.55f));
    struct Seg {
        ImVec2 a, b;
    };
    Seg segs[4] = {
        {ImVec2(center.x - gap - arm, center.y), ImVec2(center.x - gap, center.y)},
        {ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + arm, center.y)},
        {ImVec2(center.x, center.y - gap - arm), ImVec2(center.x, center.y - gap)},
        {ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + arm)},
    };
    for(auto const& seg : segs) {
        dl->AddLine(seg.a, seg.b, shadow, thickness + 2.0f * s);
    }
    for(auto const& seg : segs) {
        dl->AddLine(seg.a, seg.b, c, thickness);
    }
    dl->AddCircleFilled(center, thickness * 0.6f, c, 8);
}

}  // namespace

namespace NexusMods {

void setFonts(Fonts const& fonts) {
    g_fonts = fonts;
}

bool isMenuOpen() {
    return g_menuOpen;
}

void setMenuOpen(bool open) {
    if(g_menuOpen == open) {
        return;
    }
    g_menuOpen = open;
    g_resetScroll = true;
    if(!open) {
        g_openSettings = -1;
    }
}

void toggleMenu() {
    setMenuOpen(!g_menuOpen);
}

void handleMenuHotkey() {
    if(ImGui::IsKeyPressed(menuHotkey(), false)) {
        toggleMenu();
    }
    if(g_menuOpen && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        setMenuOpen(false);
    }
}

void drawMenubarSection(GameWindow* window) {
    if(!ImGui::BeginMenu("Nexus")) {
        return;
    }
    for(int i = 0; i < MODULE_COUNT; i++) {
        Module const& m = MODULES[i];
        bool on = moduleEnabled(m, window);
        bool soon = (m.special == SPECIAL_COMING_SOON);
        if(ImGui::MenuItem(m.name, soon ? "SOON" : nullptr, on, !soon)) {
            setModuleEnabled(m, window, !on);
        }
    }
    ImGui::Separator();
    if(ImGui::MenuItem("Open Mod Menu", menuHotkeyLabel())) {
        setMenuOpen(true);
    }
    ImGui::EndMenu();
}

// ---------------------------------------------------------------------------

static void drawModuleGrid(GameWindow* window, float contentWidth) {
    std::string needle = toLower(g_search);
    int columns = contentWidth > 660.0f * ui() ? 3 : (contentWidth > 430.0f * ui() ? 2 : 1);
    float gap = 12.0f * ui();
    float cardW = (contentWidth - gap * (columns - 1)) / columns;
    float cardH = 112.0f * ui();

    int drawn = 0;
    for(int i = 0; i < MODULE_COUNT; i++) {
        Module const& m = MODULES[i];
        if(g_categoryFilter >= 0 && m.category != g_categoryFilter) {
            continue;
        }
        if(!needle.empty()) {
            std::string hay = toLower(std::string(m.name) + " " + m.subtitle + " " + CATEGORY_NAMES[m.category]);
            if(hay.find(needle) == std::string::npos) {
                continue;
            }
        }

        if(drawn % columns != 0) {
            ImGui::SameLine(0, gap);
        }
        bool on = moduleEnabled(m, window);
        bool soon = (m.special == SPECIAL_COMING_SOON);

        char cardId[64];
        std::snprintf(cardId, sizeof(cardId), "##nexuscard_%s", m.id);
        ImVec2 origin = ImGui::GetCursorScreenPos();
        float hover = 0.0f;
        bool clicked = cardFrame(cardId, ImVec2(cardW, cardH), on, &hover);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float pad = 14.0f * ui();

        Face title = face(18.0f);
        drawText(dl, title, ImVec2(origin.x + pad, origin.y + pad), col(COL_TEXT), m.name);

        Face sub = face(15.0f);
        drawText(dl, sub, ImVec2(origin.x + pad, origin.y + pad + 24.0f * ui()),
                 col(COL_MUTED), m.subtitle, cardW - pad * 2 - 26.0f * ui());

        // Category tag, top right.
        Face tag = face(15.0f);
        char const* catName = CATEGORY_NAMES[m.category];
        ImVec2 tagSize = measure(tag, catName);
        drawText(dl, tag, ImVec2(origin.x + cardW - pad - tagSize.x, origin.y + pad + 2.0f * ui()),
                 col(accent(), 0.45f + 0.35f * hover), catName);

        // Footer: switch plus state label on the left, gear on the right.
        // A coming-soon module shows a static badge in place of the switch.
        float switchH = 18.0f * ui();
        float footerY = origin.y + cardH - pad - switchH;
        if(soon) {
            char const* label = "COMING SOON";
            Face badge = face(15.0f);
            ImVec2 ls = measure(badge, label);
            float bh = switchH + 4.0f * ui();
            ImVec2 bb0 = ImVec2(origin.x + pad, footerY - 2.0f * ui());
            ImVec2 bb1 = ImVec2(bb0.x + ls.x + 18.0f * ui(), bb0.y + bh);
            dl->AddRectFilled(bb0, bb1, col(accent(), 0.16f), bh * 0.5f);
            dl->AddRect(bb0, bb1, col(accent(), 0.55f), bh * 0.5f, 0, 1.0f * ui());
            drawText(dl, badge, ImVec2(bb0.x + 9.0f * ui(), bb0.y + (bh - ls.y) * 0.5f), col(accent(), 0.9f), label);
        } else {
            float& t = anim((std::string("sw_") + m.id).c_str());
            t = ease(t, on ? 1.0f : 0.0f, 22.0f);
            drawSwitch(dl, ImVec2(origin.x + pad, footerY), switchH, on, t);

            Face state = face(15.0f);
            drawText(dl, state, ImVec2(origin.x + pad + switchH * 1.9f + 8.0f * ui(), footerY + 1.0f * ui()),
                     on ? col(accent()) : col(COL_OFF), on ? "ENABLED" : "DISABLED");
        }

        bool gearClicked = false;
        if(m.pos || m.scale) {
            char gearId[64];
            std::snprintf(gearId, sizeof(gearId), "##gear_%s", m.id);
            ImVec2 gearSize = ImVec2(26.0f * ui(), 26.0f * ui());
            ImGui::SetCursorScreenPos(ImVec2(origin.x + cardW - pad - gearSize.x, footerY - 4.0f * ui()));
            ImGui::InvisibleButton(gearId, gearSize);
            bool gearHovered = ImGui::IsItemHovered();
            gearClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            drawGear(dl, ImVec2(origin.x + cardW - pad - gearSize.x * 0.5f, footerY - 4.0f * ui() + gearSize.y * 0.5f),
                     9.0f * ui(), gearHovered ? col(COL_TEXT) : col(COL_MUTED));
            ImGui::SetCursorScreenPos(origin);
            ImGui::Dummy(ImVec2(cardW, cardH));
        }

        if(gearClicked) {
            g_openSettings = i;
            g_resetScroll = true;
        } else if(clicked && !soon) {
            setModuleEnabled(m, window, !on);
        }
        drawn++;
    }

    if(drawn == 0) {
        ImGui::Dummy(ImVec2(0, 24.0f * ui()));
        Face f = face(18.0f);
        ImVec2 p = ImGui::GetCursorScreenPos();
        drawText(ImGui::GetWindowDrawList(), f, p, col(COL_MUTED), "No mods match your search.");
        ImGui::Dummy(ImVec2(0, 40.0f * ui()));
    }
}

static void drawModuleSettings(GameWindow* window, float contentWidth) {
    Module const& m = MODULES[g_openSettings];
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if(pillButton("##nexusback", "< BACK", ImVec2(96.0f * ui(), 30.0f * ui()), false)) {
        g_openSettings = -1;
        g_resetScroll = true;
        return;
    }

    ImGui::Dummy(ImVec2(0, 10.0f * ui()));

    ImVec2 p = ImGui::GetCursorScreenPos();
    Face title = face(24.0f);
    drawText(dl, title, p, col(COL_TEXT), m.name);
    ImGui::Dummy(ImVec2(0, 30.0f * ui()));

    p = ImGui::GetCursorScreenPos();
    Face body = face(15.0f);
    ImVec2 descSize = measure(body, m.description, contentWidth);
    drawText(dl, body, p, col(COL_MUTED), m.description, contentWidth);
    ImGui::Dummy(ImVec2(contentWidth, descSize.y + 14.0f * ui()));

    // The enable switch, restated here so the settings page is self contained.
    {
        bool on = moduleEnabled(m, window);
        float switchH = 20.0f * ui();
        ImVec2 sp = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##nexussettingsswitch", ImVec2(switchH * 1.9f + 90.0f * ui(), switchH + 4.0f * ui()));
        if(ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            setModuleEnabled(m, window, !on);
            on = !on;
        }
        float& t = anim("settingspill");
        t = ease(t, on ? 1.0f : 0.0f, 22.0f);
        drawSwitch(dl, sp, switchH, on, t);
        Face state = face(15.0f);
        drawText(dl, state, ImVec2(sp.x + switchH * 1.9f + 10.0f * ui(), sp.y + 2.0f * ui()),
                 on ? col(accent()) : col(COL_OFF), on ? "ENABLED" : "DISABLED");
    }

    ImGui::Dummy(ImVec2(0, 12.0f * ui()));

    if(!m.pos && !m.scale) {
        p = ImGui::GetCursorScreenPos();
        drawText(dl, body, p, col(COL_OFF),
                 "Gameplay module. It changes how the game behaves and has no HUD box, so there is nothing to position or scale.",
                 contentWidth);
        ImGui::Dummy(ImVec2(contentWidth, 40.0f * ui()));
        return;
    }

    if(m.pos) {
        p = ImGui::GetCursorScreenPos();
        Face label = face(15.0f);
        drawText(dl, label, p, col(COL_TEXT), "HUD POSITION");
        ImGui::Dummy(ImVec2(0, 24.0f * ui()));

        // Corner buttons on the left, live preview on the right.
        char const* cornerNames[CORNER_COUNT] = {"TOP LEFT", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM RIGHT"};
        float previewW = 190.0f * ui();
        float buttonsW = contentWidth - previewW - 20.0f * ui();
        if(buttonsW < 200.0f * ui()) {
            buttonsW = contentWidth;
            previewW = 0.0f;
        }
        float bw = (buttonsW - 10.0f * ui()) * 0.5f;
        float bh = 34.0f * ui();

        ImVec2 blockStart = ImGui::GetCursorScreenPos();
        for(int c = 0; c < CORNER_COUNT; c++) {
            char id[48];
            std::snprintf(id, sizeof(id), "##corner_%d", c);
            ImGui::SetCursorScreenPos(ImVec2(blockStart.x + (c % 2) * (bw + 10.0f * ui()),
                                             blockStart.y + (c / 2) * (bh + 10.0f * ui())));
            if(pillButton(id, cornerNames[c], ImVec2(bw, bh), *m.pos == c)) {
                *m.pos = c;
                Settings::save();
            }
        }

        if(previewW > 0.0f) {
            float previewH = bh * 2 + 10.0f * ui();
            ImVec2 pv = ImVec2(blockStart.x + buttonsW + 20.0f * ui(), blockStart.y);
            ImVec2 pvEnd = ImVec2(pv.x + previewW, pv.y + previewH);
            dl->AddRectFilled(pv, pvEnd, col(COL_BG), 8.0f * ui());
            dl->AddRect(pv, pvEnd, col(COL_LINE), 8.0f * ui(), 0, 1.0f * ui());
            // A little box in the chosen corner, so the preset reads at a glance.
            float boxW = previewW * 0.34f;
            float boxH = previewH * 0.2f;
            float inset = 8.0f * ui();
            float bx = (*m.pos == CORNER_TOP_LEFT || *m.pos == CORNER_BOTTOM_LEFT) ? pv.x + inset : pvEnd.x - inset - boxW;
            float by = (*m.pos == CORNER_TOP_LEFT || *m.pos == CORNER_TOP_RIGHT) ? pv.y + inset : pvEnd.y - inset - boxH;
            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + boxW, by + boxH), col(accent(), 0.85f), 3.0f * ui());
            Face tiny = face(15.0f);
            ImVec2 ls = measure(tiny, "POSITION PREVIEW");
            drawText(dl, tiny, ImVec2(pv.x + (previewW - ls.x) * 0.5f, pv.y + (previewH - ls.y) * 0.5f),
                     col(COL_OFF), "POSITION PREVIEW");
        }

        ImGui::SetCursorScreenPos(ImVec2(blockStart.x, blockStart.y + (bh + 10.0f * ui()) * 2));
        ImGui::Dummy(ImVec2(contentWidth, 6.0f * ui()));
    }

    if(m.scale) {
        p = ImGui::GetCursorScreenPos();
        Face label = face(15.0f);
        drawText(dl, label, p, col(COL_TEXT), "SIZE");
        ImGui::Dummy(ImVec2(0, 22.0f * ui()));

        ImGui::PushStyleColor(ImGuiCol_FrameBg, col(ImVec4(1, 1, 1, 0.06f)));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, col(ImVec4(1, 1, 1, 0.10f)));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, col(ImVec4(1, 1, 1, 0.12f)));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, col(accent(), 0.9f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, col(accent()));
        ImGui::PushStyleColor(ImGuiCol_Text, col(COL_TEXT));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * ui());
        ImGui::SetNextItemWidth(contentWidth * 0.6f);
        float value = *m.scale;
        if(ImGui::SliderFloat("##nexusmodsettings_scale", &value, 0.6f, 2.0f, "%.2fx")) {
            *m.scale = value;
        }
        if(ImGui::IsItemDeactivatedAfterEdit()) {
            Settings::save();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(6);
        ImGui::Dummy(ImVec2(0, 8.0f * ui()));
    }

    if(pillButton("##nexusreset", "RESET POSITION", ImVec2(180.0f * ui(), 32.0f * ui()), false)) {
        if(m.pos) {
            *m.pos = CORNER_TOP_LEFT;
        }
        if(m.scale) {
            *m.scale = 1.0f;
        }
        Settings::save();
    }

    ImGui::Dummy(ImVec2(0, 8.0f * ui()));
    p = ImGui::GetCursorScreenPos();
    drawText(dl, body, p, col(COL_OFF),
             "HUD modules snap to corner presets and stack inside their corner, so two modules in the same corner never overlap.",
             contentWidth);
    ImGui::Dummy(ImVec2(contentWidth, 36.0f * ui()));
}

void drawMenu(GameWindow* window) {
    ImGuiIO& io = ImGui::GetIO();
    g_menuAnim = ease(g_menuAnim, g_menuOpen ? 1.0f : 0.0f, 20.0f);

    // Free the OS cursor while the menu is up, otherwise the game keeps it
    // captured and none of this is clickable. Re-asserted every frame because
    // the input mode machinery can grab it back.
    if(g_menuOpen) {
        if(window->getCursorDisabled()) {
            window->setCursorDisabled(false);
        }
        g_cursorFreed = true;
    } else if(g_cursorFreed) {
        g_cursorFreed = false;
        if(CorePatches::isMouseLocked()) {
            window->setCursorDisabled(true);
        }
    }

    if(g_menuAnim <= 0.002f) {
        return;
    }

    // Enlarge the whole menu by the player's Menu Size for the duration of this
    // draw only. Cleared at the bottom so drawHud keeps the base UI scale.
    g_uiScaleOverride = baseUi() * menuScaleFactor();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
        col(ImVec4(0.0f, 0.0f, 0.0f, 0.62f), g_menuAnim));

    float width = 840.0f * ui();
    float height = 560.0f * ui();
    if(width > vp->Size.x - 40.0f * ui()) width = vp->Size.x - 40.0f * ui();
    if(height > vp->Size.y - 40.0f * ui()) height = vp->Size.y - 40.0f * ui();

    // Slight rise as it opens. Subtle, not a bounce.
    float rise = (1.0f - g_menuAnim) * 24.0f * ui();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f + rise),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f * ui());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f * ui());
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, col(ImVec4(0, 0, 0, 0)));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, col(ImVec4(1, 1, 1, 0.10f)));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, col(ImVec4(1, 1, 1, 0.18f)));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, col(accent(), 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text, col(COL_TEXT));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;
    if(!g_menuOpen) {
        flags |= ImGuiWindowFlags_NoMouseInputs;
    }

    if(ImGui::Begin("##nexuspage", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        float rounding = 16.0f * ui();

        // Panel body plus a soft accent glow along the top edge.
        dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), col(COL_BG, 0.94f * g_menuAnim + 0.06f), rounding);
        dl->AddRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), col(ImVec4(1, 1, 1, 0.09f)), rounding, 0, 1.0f * ui());
        dl->AddRectFilled(ImVec2(wp.x + rounding, wp.y), ImVec2(wp.x + ws.x - rounding, wp.y + 2.0f * ui()),
                          col(accent(), 0.85f));

        float pad = 26.0f * ui();
        float contentWidth = ws.x - pad * 2;

        // ---- Header ----
        ImGui::SetCursorScreenPos(ImVec2(wp.x + pad, wp.y + 22.0f * ui()));
        ImVec2 hp = ImGui::GetCursorScreenPos();
        Face wordmark = face(36.0f);
        drawText(dl, wordmark, hp, col(accent()), "NEXUS");
        ImVec2 wmSize = measure(wordmark, "NEXUS");
        Face tagFace = face(18.0f);
        drawText(dl, tagFace, ImVec2(hp.x + wmSize.x + 10.0f * ui(), hp.y + wmSize.y * 0.42f),
                 col(COL_TEXT, 0.85f), "MOD MENU");
        Face byline = face(15.0f);
        drawText(dl, byline, ImVec2(hp.x + 2.0f * ui(), hp.y + wmSize.y + 2.0f * ui()), col(COL_OFF), "BY STS STUDIO");

        // Close button, right aligned.
        {
            float rightEdge = wp.x + ws.x - pad;
            float closeSize = 26.0f * ui();
            ImGui::SetCursorScreenPos(ImVec2(rightEdge - closeSize, wp.y + 24.0f * ui()));
            ImGui::InvisibleButton("##nexusclose", ImVec2(closeSize, closeSize));
            bool closeHover = ImGui::IsItemHovered();
            if(ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                setMenuOpen(false);
            }
            ImVec2 cc = ImVec2(rightEdge - closeSize * 0.5f, wp.y + 24.0f * ui() + closeSize * 0.5f);
            float arm = closeSize * 0.26f;
            ImU32 closeCol = closeHover ? col(COL_TEXT) : col(COL_MUTED);
            dl->AddLine(ImVec2(cc.x - arm, cc.y - arm), ImVec2(cc.x + arm, cc.y + arm), closeCol, 1.8f * ui());
            dl->AddLine(ImVec2(cc.x + arm, cc.y - arm), ImVec2(cc.x - arm, cc.y + arm), closeCol, 1.8f * ui());
        }

        // Menu size stepper: [A-] 130% [A+], to the left of the close button.
        // A global setting, so it lives in the header and is drawn on every page.
        {
            float closeSize = 26.0f * ui();
            float btn = 30.0f * ui();
            float ctrlY = wp.y + 24.0f * ui() + (closeSize - btn) * 0.5f;
            float rightEdge = wp.x + ws.x - pad;
            char pctBuf[16];
            std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(menuScaleFactor() * 100.0f + 0.5f));
            Face lf = face(15.0f);
            ImVec2 pls = measure(lf, pctBuf);
            float gap2 = 8.0f * ui();
            float plusX = rightEdge - closeSize - 16.0f * ui() - btn;
            float pctX = plusX - gap2 - pls.x;
            float minusX = pctX - gap2 - btn;

            ImGui::SetCursorScreenPos(ImVec2(minusX, ctrlY));
            if(pillButton("##nexusmenusize_minus", "A-", ImVec2(btn, btn), false)) {
                float v = menuScaleFactor() - 0.1f;
                if(v < 0.8f) v = 0.8f;
                Settings::nexus_menu_scale = v;
                Settings::save();
            }
            drawText(dl, lf, ImVec2(pctX, ctrlY + (btn - pls.y) * 0.5f), col(COL_MUTED), pctBuf);
            ImGui::SetCursorScreenPos(ImVec2(plusX, ctrlY));
            if(pillButton("##nexusmenusize_plus", "A+", ImVec2(btn, btn), false)) {
                float v = menuScaleFactor() + 0.1f;
                if(v > 2.0f) v = 2.0f;
                Settings::nexus_menu_scale = v;
                Settings::save();
            }
        }

        // ---- Divider under the header ----
        // The menu has a single page, so there is no tab strip. A short accent
        // rule sits where the active tab underline used to, then a hairline
        // across the rest of the width.
        float ruleY = wp.y + 92.0f * ui();
        dl->AddRectFilled(ImVec2(wp.x + pad, ruleY), ImVec2(wp.x + pad + 46.0f * ui(), ruleY + 2.0f * ui()),
                          col(accent()), 1.0f * ui());
        dl->AddRectFilled(ImVec2(wp.x + pad, ruleY + 1.0f * ui()), ImVec2(wp.x + ws.x - pad, ruleY + 2.0f * ui()),
                          col(COL_LINE));

        // ---- Search and category chips (module grid only) ----
        float bodyTop = ruleY + 16.0f * ui();
        if(g_openSettings < 0) {
            ImGui::SetCursorScreenPos(ImVec2(wp.x + pad, bodyTop));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, col(COL_PANEL));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, col(COL_CARD));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, col(COL_CARD));
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, col(COL_OFF));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f * ui());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f * ui(), 8.0f * ui()));
            ImGui::SetNextItemWidth(contentWidth);
            ImGui::InputTextWithHint("##nexussearch", "Search", g_search, sizeof(g_search));
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);

            float chipsY = ImGui::GetCursorScreenPos().y + 10.0f * ui();
            float chipX = wp.x + pad;
            ImGui::SetCursorScreenPos(ImVec2(chipX, chipsY));
            if(chipButton("##cat_all", "ALL", g_categoryFilter < 0)) {
                g_categoryFilter = -1;
            }
            for(int c = 0; c < CAT_COUNT; c++) {
                ImGui::SameLine(0, 8.0f * ui());
                char id[32];
                std::snprintf(id, sizeof(id), "##cat_%d", c);
                if(chipButton(id, CATEGORY_NAMES[c], g_categoryFilter == c)) {
                    g_categoryFilter = (g_categoryFilter == c) ? -1 : c;
                }
            }
            bodyTop = ImGui::GetCursorScreenPos().y + 34.0f * ui();
        }

        // ---- Scrolling body ----
        float footerH = 40.0f * ui();
        float bodyH = (wp.y + ws.y - footerH) - bodyTop;
        if(bodyH < 60.0f * ui()) {
            bodyH = 60.0f * ui();
        }
        ImGui::SetCursorScreenPos(ImVec2(wp.x + pad, bodyTop));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, col(ImVec4(0, 0, 0, 0)));
        if(ImGui::BeginChild("##nexusgrid", ImVec2(contentWidth, bodyH), ImGuiChildFlags_None,
                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav)) {
            // A newly shown page always starts at the top. Without this the grid
            // can come up scrolled to wherever focus last landed.
            if(g_resetScroll) {
                ImGui::SetScrollY(0.0f);
                g_resetScroll = false;
            }
            if(g_openSettings >= 0 && g_openSettings < MODULE_COUNT) {
                drawModuleSettings(window, contentWidth);
            } else {
                drawModuleGrid(window, contentWidth);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // ---- Footer ----
        {
            char hint[160];
            std::snprintf(hint, sizeof(hint),
                          "%s opens this menu  -  click the gear on a card for its settings",
                          menuHotkeyLabel());
            Face hf = face(15.0f);
            drawText(dl, hf, ImVec2(wp.x + pad, wp.y + ws.y - footerH + 12.0f * ui()), col(COL_OFF), hint);

            char const* discordLabel = "discord.gg/stsstudio";
            ImVec2 ds = measure(hf, discordLabel);
            float dx = wp.x + ws.x - pad - ds.x;
            float dy = wp.y + ws.y - footerH + 12.0f * ui();
            ImGui::SetCursorScreenPos(ImVec2(dx, dy - 4.0f * ui()));
            ImGui::InvisibleButton("##nexusdiscordbtn", ImVec2(ds.x, ds.y + 8.0f * ui()));
            bool dHover = ImGui::IsItemHovered();
            if(ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                openUrl("https://discord.gg/stsstudio");
            }
            drawText(dl, hf, ImVec2(dx, dy), dHover ? col(accent()) : col(COL_MUTED), discordLabel);
            if(dHover) {
                dl->AddLine(ImVec2(dx, dy + ds.y + 1.0f * ui()), ImVec2(dx + ds.x, dy + ds.y + 1.0f * ui()),
                            col(accent(), 0.7f), 1.0f * ui());
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(4);

    // Back to the base scale for everything drawn after the menu (the HUD).
    g_uiScaleOverride = 0.0f;
}

// ---------------------------------------------------------------------------

void drawHud(GameWindow* window) {
    updateClickHistory();
    updateFpsHistory();

    bool inGame = CorePatches::isMouseLocked();
    bool showHud = inGame || g_menuOpen || Settings::nexus_hud_in_menus != 0;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workPos = vp->WorkPos;
    ImVec2 workSize = vp->WorkSize;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    float margin = 14.0f * ui();
    float gap = 6.0f * ui();
    // Where each corner's stack has grown to. Anything else that wants to sit in
    // a corner (the Discord button) starts from here, so nothing overlaps.
    float cornerCursor[CORNER_COUNT];
    for(int corner = 0; corner < CORNER_COUNT; corner++) {
        bool top = (corner == CORNER_TOP_LEFT || corner == CORNER_TOP_RIGHT);
        cornerCursor[corner] = top ? workPos.y + margin : workPos.y + workSize.y - margin;
    }

    if(showHud) {
        for(int corner = 0; corner < CORNER_COUNT; corner++) {
            bool top = (corner == CORNER_TOP_LEFT || corner == CORNER_TOP_RIGHT);
            bool left = (corner == CORNER_TOP_LEFT || corner == CORNER_BOTTOM_LEFT);

            for(int i = 0; i < MODULE_COUNT; i++) {
                Module const& m = MODULES[i];
                if(!m.pos || *m.pos != corner) {
                    continue;
                }
                if(!moduleEnabled(m, window)) {
                    continue;
                }
                float s = moduleScale(m);
                ImVec2 size = measureHud(m, s);
                float x = left ? workPos.x + margin : workPos.x + workSize.x - margin - size.x;
                float y = top ? cornerCursor[corner] : cornerCursor[corner] - size.y;
                drawHudModule(dl, m, ImVec2(x, y), size, s);
                cornerCursor[corner] += top ? (size.y + gap) : -(size.y + gap);
            }
        }
    }

    if(inGame && !g_menuOpen && Settings::nexus_crosshair != 0) {
        drawCrosshair(dl, ImVec2(workPos.x + workSize.x * 0.5f, workPos.y + workSize.y * 0.5f));
    }

    // Join Discord button, shown while the game is paused (mouse released) and
    // the menu is closed. Bottom right, clear of Minecraft's centred buttons.
    if(!inGame && !g_menuOpen && Settings::nexus_discord_button != 0) {
        float bw = 168.0f * ui();
        float bh = 38.0f * ui();
        // Sits above whatever the bottom right corner is already stacking.
        ImVec2 pos = ImVec2(workPos.x + workSize.x - bw - margin,
                            cornerCursor[CORNER_BOTTOM_RIGHT] - bh);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(bw, bh), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if(ImGui::Begin("##joindiscord", nullptr,
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground)) {
            if(pillButton("##nexusdiscordjoin", "JOIN DISCORD", ImVec2(bw, bh), true)) {
                openUrl("https://discord.gg/stsstudio");
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}

// ---------------------------------------------------------------------------

int filterSprintKey(int key, int action) {
    if(Settings::nexus_toggle_sprint == 0) {
        g_sprintHeld = false;
        return action;
    }
    if(key != (int)KeyCode::LEFT_CTRL) {
        return action;
    }
    // KeyAction: 0 down, 1 up, 2 repeat in game-window's enum. Compare by value
    // so this stays independent of the header's ordering.
    if(action == (int)KeyAction::PRESS) {
        g_sprintHeld = !g_sprintHeld;
        return g_sprintHeld ? (int)KeyAction::PRESS : (int)KeyAction::RELEASE;
    }
    if(action == (int)KeyAction::RELEASE) {
        // Swallow the physical release so the game keeps sprinting.
        return -1;
    }
    return action;
}

}  // namespace NexusMods
