
#include "it.h"

static struct {
    const char *name;
    SDL_Keymod mod;
} string_to_mod[] = {
    {"CTRL", KMOD_CTRL},
    {"LCTRL", KMOD_LCTRL},
    {"RCTRL", KMOD_RCTRL},
    {"SHIFT", KMOD_SHIFT},
    {"LSHIFT", KMOD_LSHIFT},
    {"RSHIFT", KMOD_RSHIFT},
    {"ALT", KMOD_LALT},
    {"RALT", KMOD_RALT}
};

static struct {
    const char *name;
    SDL_Scancode code;
} string_to_scancode[] = {
    {"US_A",  SDL_SCANCODE_A},
    {"US_B",  SDL_SCANCODE_B},
    {"US_C",  SDL_SCANCODE_C},
    {"US_D",  SDL_SCANCODE_D},
    {"US_E",  SDL_SCANCODE_E},
    {"US_F",  SDL_SCANCODE_F},
    {"US_G",  SDL_SCANCODE_G},
    {"US_H",  SDL_SCANCODE_H},
    {"US_I",  SDL_SCANCODE_I},
    {"US_J",  SDL_SCANCODE_J},
    {"US_K",  SDL_SCANCODE_K},
    {"US_L",  SDL_SCANCODE_L},
    {"US_M",  SDL_SCANCODE_M},
    {"US_N",  SDL_SCANCODE_N},
    {"US_O",  SDL_SCANCODE_O},
    {"US_P",  SDL_SCANCODE_P},
    {"US_Q",  SDL_SCANCODE_Q},
    {"US_R",  SDL_SCANCODE_R},
    {"US_S",  SDL_SCANCODE_S},
    {"US_T",  SDL_SCANCODE_T},
    {"US_U",  SDL_SCANCODE_U},
    {"US_V",  SDL_SCANCODE_V},
    {"US_W",  SDL_SCANCODE_W},
    {"US_X",  SDL_SCANCODE_X},
    {"US_Y",  SDL_SCANCODE_Y},
    {"US_Z",  SDL_SCANCODE_Z},

    {"US_1",  SDL_SCANCODE_1},
    {"US_2",  SDL_SCANCODE_2},
    {"US_3",  SDL_SCANCODE_3},
    {"US_4",  SDL_SCANCODE_4},
    {"US_5",  SDL_SCANCODE_5},
    {"US_6",  SDL_SCANCODE_6},
    {"US_7",  SDL_SCANCODE_7},
    {"US_8",  SDL_SCANCODE_8},
    {"US_9",  SDL_SCANCODE_9},
    {"US_0",  SDL_SCANCODE_0},

    {"US_F1",  SDL_SCANCODE_F1},
    {"US_F2",  SDL_SCANCODE_F2},
    {"US_F3",  SDL_SCANCODE_F3},
    {"US_F4",  SDL_SCANCODE_F4},
    {"US_F5",  SDL_SCANCODE_F5},
    {"US_F6",  SDL_SCANCODE_F6},
    {"US_F7",  SDL_SCANCODE_F7},
    {"US_F8",  SDL_SCANCODE_F8},
    {"US_F9",  SDL_SCANCODE_F9},
    {"US_F10",  SDL_SCANCODE_F10},
    {"US_F11",  SDL_SCANCODE_F11},
    {"US_F12",  SDL_SCANCODE_F12},

    {"US_ENTER",  SDL_SCANCODE_RETURN},
    {"US_ESCAPE",  SDL_SCANCODE_ESCAPE},
    {"US_BACKSPACE",  SDL_SCANCODE_BACKSPACE},
    {"US_TAB",  SDL_SCANCODE_TAB},
    {"US_SPACE",  SDL_SCANCODE_SPACE},

    {"US_MINUS",  SDL_SCANCODE_MINUS},
    {"US_EQUALS",  SDL_SCANCODE_EQUALS},
    {"US_LEFTBRACKET",  SDL_SCANCODE_LEFTBRACKET},
    {"US_RIGHTBRACKET",  SDL_SCANCODE_RIGHTBRACKET},
    {"US_BACKSLASH",  SDL_SCANCODE_BACKSLASH},

    {"US_SEMICOLON",  SDL_SCANCODE_SEMICOLON},
    {"US_APOSTROPHE",  SDL_SCANCODE_APOSTROPHE},
    {"US_GRAVE",  SDL_SCANCODE_GRAVE},
    {"US_COMMA",  SDL_SCANCODE_COMMA},
    {"US_PERIOD",  SDL_SCANCODE_PERIOD},
    {"US_SLASH",  SDL_SCANCODE_SLASH},
    {"US_CAPSLOCK",  SDL_SCANCODE_CAPSLOCK},
    {"US_PRINTSCREEN",  SDL_SCANCODE_PRINTSCREEN},
    {"US_SCROLLLOCK",  SDL_SCANCODE_SCROLLLOCK},
    {"US_PAUSE",  SDL_SCANCODE_PAUSE},

    {"US_INSERT",  SDL_SCANCODE_INSERT},
    {"US_DELETE",  SDL_SCANCODE_DELETE},
    {"US_HOME",  SDL_SCANCODE_HOME},
    {"US_END",  SDL_SCANCODE_END},
    {"US_PAGEUP",  SDL_SCANCODE_PAGEUP},
    {"US_PAGEDOWN",  SDL_SCANCODE_PAGEDOWN},

    {"US_RIGHT",  SDL_SCANCODE_RIGHT},
    {"US_LEFT",  SDL_SCANCODE_LEFT},
    {"US_DOWN",  SDL_SCANCODE_DOWN},
    {"US_UP",  SDL_SCANCODE_UP},

    {"US_NUMLOCK",  SDL_SCANCODE_NUMLOCKCLEAR}, /**< num lock on PC, clear on Mac keyboards */
    {"US_KP_DIVIDE",  SDL_SCANCODE_KP_DIVIDE},
    {"US_KP_MULTIPLY",  SDL_SCANCODE_KP_MULTIPLY},
    {"US_KP_MINUS",  SDL_SCANCODE_KP_MINUS},
    {"US_KP_PLUS",  SDL_SCANCODE_KP_PLUS},
    {"US_KP_ENTER",  SDL_SCANCODE_KP_ENTER},
    {"US_KP_1",  SDL_SCANCODE_KP_1},
    {"US_KP_2",  SDL_SCANCODE_KP_2},
    {"US_KP_3",  SDL_SCANCODE_KP_3},
    {"US_KP_4",  SDL_SCANCODE_KP_4},
    {"US_KP_5",  SDL_SCANCODE_KP_5},
    {"US_KP_6",  SDL_SCANCODE_KP_6},
    {"US_KP_7",  SDL_SCANCODE_KP_7},
    {"US_KP_8",  SDL_SCANCODE_KP_8},
    {"US_KP_9",  SDL_SCANCODE_KP_9},
    {"US_KP_0",  SDL_SCANCODE_KP_0},
    {"US_KP_PERIOD",  SDL_SCANCODE_KP_PERIOD},
    {"US_KP_EQUALS",  SDL_SCANCODE_KP_EQUALS}
};
