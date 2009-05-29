#include <stdio.h>
#include <stdint.h>
#include <SDL.h>

// these are in util.h
#define CLAMP(N,L,H) (((N)>(H))?(H):(((N)<(L))?(L):(N)))
#define UNUSED __attribute__((unused))


/*
TODO:
string descriptions for joystick and midi keyboard events
joystick axes and hats should emit synthetic repeated 'press' events
come up with an api for handling unicode data
velocity for joystick and midi
should really be keeping the trees balanced... meh
less stupid data structure than a linked list for the keysym translator?
put the keymaps themselves into a tree of some sort, indexed by name.
	this way, it's possible to bind a key to change the current keymap
	-- e.g. bind Ctrl-X to a "set keymap" function with data="Ctrl-X map"
need to make a list of all the things that can have associated keymaps...



All of the modifier keys MIGHT additionally be presentable as keys themselves.
(e.g. for FT2 style playback control bindings)
However, if the platform doesn't support this, too bad.

For the keyboard, most of the time the 'keycode' is a plain SDL keysym value. However, if the
keysym was zero, the scancode is used instead, with the high bit set. Most of the time SDL does
provide a keysym, but for some weird keys it might not. Scancodes are inherently non-portable,
so this is only to allow extra keys to be bound to something.
If the entire keycode value is zero, then we have no idea what key was pressed.
(This happens for input methods e.g. scim that only send a unicode character.)

For instrument list keyjazz, keydown events should keep track of the channel that got
assigned to each note, and keyup should look up that channel and send a keyoff.



Keymap files are split into sections denoted by [brackets]
Each section defines a specific keymap.

Note that this file can use the same parser as the config file.

Widgets and pages use keymaps with certain names by default, but you can define keymaps
with pretty much any name you want.

the event parser is fairly liberal and can understand many different representations of keys.
some examples follow:


; the Global map is always active, and receives all events not handled by a prior keymap
[Global]
F5=song_start_set_infopage
Ctrl+F5 = song_start
F8=song_stop
kp_multiply = set_octave +1
MIDI/1 #87 = song_loop_current_pattern
; following event is actually the fourth button on the second joystick, but in numeric format. doing things
; this way is supported but not recommended, and is really more for debugging/troubleshooting than anything.
@2/2 #3 = song_loop_pattern
keyboard/0 shift+f9 = page_switch message_editor
f9=page_switch load_module
f10=page_switch save_module
^S = song_save
escape = main_menu_toggle

[Pattern Editor]
M-q = pat_transpose +1
M-a = pat_transpose -1
M-S-q = pat_transpose +12
M-S-a = pat_transpose -12

[Sample List]
keyboard/0 meta+A = smp_sign_convert


Keymap lookup order: (prefix) | widget -> page -> global
The "prefix" map is user-defined by the kmap_set_prefix binding, and is normally empty. This map can be used
to implement multi-key events. For example:

[Global]
Ctrl-X = kmap_set_prefix ctrlx_map

[ctrlx_map]
Ctrl-S = song_save
Ctrl-C = quit

This effectively binds the Ctrl-X keys like Emacs, and also allows for "regular" bindings of those keys in
other maps (e.g. Ctrl-C can still be bound to centralise cursor, and so forth)
If a prefix map is active, no other keymaps are searched; in addition, prefix maps only last for one key.

	-- if (prefix_map && ev.bits.release) clear_prefix_map();



Additionally, a keymap can "inherit" keys from another map as follows. In this example, events not handled by
the "hamster" keymap are checked against the pattern editor map:

[hamster]
@inherit = Pattern Editor

One could conceivably define a key in the Pattern Editor map to load the hamster map,
and a reciprocating key in the monsquaz map that changes it back.
(FT2's "record mode" -- as well as IT's own capslock+key behavior -- could be implemented this way.)

* Need a function to do this: it should replace the keymap that owns the key that was pressed to trigger
the function. That is, if the keymap replace was bound to a key in the Pattern Editor map, the new keymap
loaded replaces the pointer that was previously pointing to the Pattern Editor map.
This way each keymap "layer" is independent and none of them can interfere with each other.


Somehow the keymap bindings need to know the context they're being called in.
For example, it would be entirely inappropriate to bind the left arrow to a thumbbar value adjust function
from within the pattern editor keymap.

*/

// ------------------------------------------------------------------------------------------------------------

// Superficially similar to SDL's event structure, but packed much tighter.
#pragma pack(push, 1)
typedef union {
	struct {
		unsigned int dev_type :  4; // SKDEV_TYPE_whatever
		unsigned int dev_id   :  4; // which device? (1->n; 0 is a pseudo "all" device)

		// note: not all "press" events have a corresponding "release"
		unsigned int release  :  1; // 1 for key-up

		// next three fields are only relevant for the pc keyboard
		unsigned int repeat   :  1; // 1 for synthetic key-repeat
		unsigned int unicode  :  1; // 1 if character maps to printable unicode
		unsigned int modifier :  5; // ctrl, alt, shift

		unsigned int keycode  : 16; // keyboard keysym/scancode
	} bits;
	uint32_t ival;
} isysev_t;
#pragma pack(pop)


// Device types (we can have 16 of these)
enum {
	SKDEV_TYPE_PCKEYBOARD,
	SKDEV_TYPE_MIDI,
	SKDEV_TYPE_JOYSTICK,
	// other device IDs are reserved
	SKDEV_TYPE_SENTINEL,
};

// Device IDs
// #0 is reserved as a sort of catch-all, to allow for binding the same event on all connected
// devices of a given type.
enum {
	SKDEV_ID_ANY = 0,
	SKDEV_ID_MAX = 15,
};


// Keyboard modifier bits
enum {
	SKMODE_CTRL  = 1 << 0,
	SKMODE_ALT   = 1 << 1,
	SKMODE_SHIFT = 1 << 2,
};

// Keycode flag bits (currently only used for PC keyboard)
enum {
	SKCODE_PCK_SCANCODE = 0x8000,

	SKCODE_MAX = 0xffff,
};


// Joystick limits (should be way more than enough)
// The event loop maintains a table of SKDEV_ID_MAX * MAX_JS_AXES + SKDEV_ID_MAX * MAX_JS_HATS
// in order to identify keyup and repeat events.

#define MAX_JS_BUTTONS 256
#define MAX_JS_AXES 64
#define MAX_JS_HATS 64
#define MAX_JS_BALLS 64

// Threshold values from ZSNES
#define JS_AXIS_THRESHOLD 16384
#define JS_BALL_THRESHOLD 100

enum { JS_AXIS_NEG, JS_AXIS_POS }; // for axes
enum { JS_DIR_UP, JS_DIR_DOWN, JS_DIR_LEFT, JS_DIR_RIGHT }; // for hats/balls
#define JS_BUTTON_TO_KEYCODE(n)     (n)
#define JS_AXIS_TO_KEYCODE(n, dir)  (2 * (n) + (dir) + MAX_JS_BUTTONS)
#define JS_HAT_TO_KEYCODE(n, dir)   (4 * (n) + (dir) + MAX_JS_BUTTONS + 2 * MAX_JS_AXES)
#define JS_BALL_TO_KEYCODE(n, dir)  (4 * (n) + (dir) + MAX_JS_BUTTONS + 2 * MAX_JS_AXES + 4 * MAX_JS_HATS)
#if (JS_BALL_TO_KEYCODE(MAX_JS_BALLS, 0) > 65535)
# error Joystick limits are too large!
#endif

// 8 chars max
static const char *skdev_names[] = {
	"keyboard",
	"midi",
	"joystick",
};

// ------------------------------------------------------------------------------------------------------------

// this struct sucks
typedef struct keytab {
	int code;
	const char *name;
	struct keytab *next;
} keytab_t;

static keytab_t *keytab = NULL;

static void key_add(int code, const char *name)
{
	keytab_t *k = malloc(sizeof(keytab_t));
	k->code = code;
	k->name = name;
	k->next = keytab;
	keytab = k;
}


static const char *keytab_code_to_name(int keycode)
{
	keytab_t *k;

	if (!(keycode & SKCODE_PCK_SCANCODE))
		for (k = keytab; k; k = k->next)
			if (k->code == keycode)
				return k->name;
	return NULL;
}

static int keytab_name_to_code(const char *keyname)
{
	keytab_t *k;

	if (!keyname[0])
		return 0;
	for (k = keytab; k; k = k->next)
		if (strcasecmp(k->name, keyname) == 0)
			return k->code;
	return 0;
}


static void keytab_free(void)
{
	keytab_t *k, *prev = NULL;
	for (k = keytab; k; k = k->next) {
		if (prev)
			free(prev);
		prev = k;
	}
	if (prev)
		free(prev);
}

static void keytab_init(void)
{
	int n;
	
	// these strings should be < 15 chars, and should not start with a hash mark ('#')
	static struct {
		int code;
		const char *name;
	} keys[] = {
		{SDLK_BACKSPACE, "Backspace"},
		{SDLK_TAB, "Tab"},
		{SDLK_CLEAR, "Clear"},
		{SDLK_RETURN, "Return"},
		{SDLK_PAUSE, "Pause"},
		{SDLK_ESCAPE, "Escape"},
		{SDLK_SPACE, "Space"},
		{SDLK_EXCLAIM, "Exclaim"},
		{SDLK_QUOTEDBL, "QuoteDbl"},
		{SDLK_HASH, "Hash"},
		{SDLK_DOLLAR, "Dollar"},
		{SDLK_AMPERSAND, "Ampersand"},
		{SDLK_QUOTE, "Quote"},
		{SDLK_LEFTPAREN, "LeftParen"},
		{SDLK_RIGHTPAREN, "RightParen"},
		{SDLK_ASTERISK, "Asterisk"},
		{SDLK_PLUS, "Plus"},
		{SDLK_COMMA, "Comma"},
		{SDLK_MINUS, "Minus"},
		{SDLK_PERIOD, "Period"},
		{SDLK_SLASH, "Slash"},
		{SDLK_0, "0"},
		{SDLK_1, "1"},
		{SDLK_2, "2"},
		{SDLK_3, "3"},
		{SDLK_4, "4"},
		{SDLK_5, "5"},
		{SDLK_6, "6"},
		{SDLK_7, "7"},
		{SDLK_8, "8"},
		{SDLK_9, "9"},
		{SDLK_COLON, "Colon"},
		{SDLK_SEMICOLON, "Semicolon"},
		{SDLK_LESS, "Less"},
		{SDLK_EQUALS, "Equals"},
		{SDLK_GREATER, "Greater"},
		{SDLK_QUESTION, "Question"},
		{SDLK_AT, "At"},

		// Skip uppercase letters

		{SDLK_LEFTBRACKET, "LeftBracket"},
		{SDLK_BACKSLASH, "Backslash"},
		{SDLK_RIGHTBRACKET, "RightBracket"},
		{SDLK_CARET, "Caret"},
		{SDLK_UNDERSCORE, "Underscore"},
		{SDLK_BACKQUOTE, "Backquote"},
		{SDLK_a, "A"},
		{SDLK_b, "B"},
		{SDLK_c, "C"},
		{SDLK_d, "D"},
		{SDLK_e, "E"},
		{SDLK_f, "F"},
		{SDLK_g, "G"},
		{SDLK_h, "H"},
		{SDLK_i, "I"},
		{SDLK_j, "J"},
		{SDLK_k, "K"},
		{SDLK_l, "L"},
		{SDLK_m, "M"},
		{SDLK_n, "N"},
		{SDLK_o, "O"},
		{SDLK_p, "P"},
		{SDLK_q, "Q"},
		{SDLK_r, "R"},
		{SDLK_s, "S"},
		{SDLK_t, "T"},
		{SDLK_u, "U"},
		{SDLK_v, "V"},
		{SDLK_w, "W"},
		{SDLK_x, "X"},
		{SDLK_y, "Y"},
		{SDLK_z, "Z"},
		{SDLK_DELETE, "Delete"},
		// End of ASCII mapped keysyms

		// International keyboard syms
		{SDLK_WORLD_0, "World_0"},
		{SDLK_WORLD_1, "World_1"},
		{SDLK_WORLD_2, "World_2"},
		{SDLK_WORLD_3, "World_3"},
		{SDLK_WORLD_4, "World_4"},
		{SDLK_WORLD_5, "World_5"},
		{SDLK_WORLD_6, "World_6"},
		{SDLK_WORLD_7, "World_7"},
		{SDLK_WORLD_8, "World_8"},
		{SDLK_WORLD_9, "World_9"},
		{SDLK_WORLD_10, "World_10"},
		{SDLK_WORLD_11, "World_11"},
		{SDLK_WORLD_12, "World_12"},
		{SDLK_WORLD_13, "World_13"},
		{SDLK_WORLD_14, "World_14"},
		{SDLK_WORLD_15, "World_15"},
		{SDLK_WORLD_16, "World_16"},
		{SDLK_WORLD_17, "World_17"},
		{SDLK_WORLD_18, "World_18"},
		{SDLK_WORLD_19, "World_19"},
		{SDLK_WORLD_20, "World_20"},
		{SDLK_WORLD_21, "World_21"},
		{SDLK_WORLD_22, "World_22"},
		{SDLK_WORLD_23, "World_23"},
		{SDLK_WORLD_24, "World_24"},
		{SDLK_WORLD_25, "World_25"},
		{SDLK_WORLD_26, "World_26"},
		{SDLK_WORLD_27, "World_27"},
		{SDLK_WORLD_28, "World_28"},
		{SDLK_WORLD_29, "World_29"},
		{SDLK_WORLD_30, "World_30"},
		{SDLK_WORLD_31, "World_31"},
		{SDLK_WORLD_32, "World_32"},
		{SDLK_WORLD_33, "World_33"},
		{SDLK_WORLD_34, "World_34"},
		{SDLK_WORLD_35, "World_35"},
		{SDLK_WORLD_36, "World_36"},
		{SDLK_WORLD_37, "World_37"},
		{SDLK_WORLD_38, "World_38"},
		{SDLK_WORLD_39, "World_39"},
		{SDLK_WORLD_40, "World_40"},
		{SDLK_WORLD_41, "World_41"},
		{SDLK_WORLD_42, "World_42"},
		{SDLK_WORLD_43, "World_43"},
		{SDLK_WORLD_44, "World_44"},
		{SDLK_WORLD_45, "World_45"},
		{SDLK_WORLD_46, "World_46"},
		{SDLK_WORLD_47, "World_47"},
		{SDLK_WORLD_48, "World_48"},
		{SDLK_WORLD_49, "World_49"},
		{SDLK_WORLD_50, "World_50"},
		{SDLK_WORLD_51, "World_51"},
		{SDLK_WORLD_52, "World_52"},
		{SDLK_WORLD_53, "World_53"},
		{SDLK_WORLD_54, "World_54"},
		{SDLK_WORLD_55, "World_55"},
		{SDLK_WORLD_56, "World_56"},
		{SDLK_WORLD_57, "World_57"},
		{SDLK_WORLD_58, "World_58"},
		{SDLK_WORLD_59, "World_59"},
		{SDLK_WORLD_60, "World_60"},
		{SDLK_WORLD_61, "World_61"},
		{SDLK_WORLD_62, "World_62"},
		{SDLK_WORLD_63, "World_63"},
		{SDLK_WORLD_64, "World_64"},
		{SDLK_WORLD_65, "World_65"},
		{SDLK_WORLD_66, "World_66"},
		{SDLK_WORLD_67, "World_67"},
		{SDLK_WORLD_68, "World_68"},
		{SDLK_WORLD_69, "World_69"},
		{SDLK_WORLD_70, "World_70"},
		{SDLK_WORLD_71, "World_71"},
		{SDLK_WORLD_72, "World_72"},
		{SDLK_WORLD_73, "World_73"},
		{SDLK_WORLD_74, "World_74"},
		{SDLK_WORLD_75, "World_75"},
		{SDLK_WORLD_76, "World_76"},
		{SDLK_WORLD_77, "World_77"},
		{SDLK_WORLD_78, "World_78"},
		{SDLK_WORLD_79, "World_79"},
		{SDLK_WORLD_80, "World_80"},
		{SDLK_WORLD_81, "World_81"},
		{SDLK_WORLD_82, "World_82"},
		{SDLK_WORLD_83, "World_83"},
		{SDLK_WORLD_84, "World_84"},
		{SDLK_WORLD_85, "World_85"},
		{SDLK_WORLD_86, "World_86"},
		{SDLK_WORLD_87, "World_87"},
		{SDLK_WORLD_88, "World_88"},
		{SDLK_WORLD_89, "World_89"},
		{SDLK_WORLD_90, "World_90"},
		{SDLK_WORLD_91, "World_91"},
		{SDLK_WORLD_92, "World_92"},
		{SDLK_WORLD_93, "World_93"},
		{SDLK_WORLD_94, "World_94"},
		{SDLK_WORLD_95, "World_95"},

		// Numeric keypad
		{SDLK_KP0, "KP_0"},
		{SDLK_KP1, "KP_1"},
		{SDLK_KP2, "KP_2"},
		{SDLK_KP3, "KP_3"},
		{SDLK_KP4, "KP_4"},
		{SDLK_KP5, "KP_5"},
		{SDLK_KP6, "KP_6"},
		{SDLK_KP7, "KP_7"},
		{SDLK_KP8, "KP_8"},
		{SDLK_KP9, "KP_9"},
		{SDLK_KP_PERIOD, "KP_Period"},
		{SDLK_KP_DIVIDE, "KP_Divide"},
		{SDLK_KP_MULTIPLY, "KP_Multiply"},
		{SDLK_KP_MINUS, "KP_Minus"},
		{SDLK_KP_PLUS, "KP_Plus"},
		{SDLK_KP_ENTER, "KP_Enter"},
		{SDLK_KP_EQUALS, "KP_Equals"},

		// Arrows + Home/End pad
		{SDLK_UP, "Up"},
		{SDLK_DOWN, "Down"},
		{SDLK_RIGHT, "Right"},
		{SDLK_LEFT, "Left"},
		{SDLK_INSERT, "Insert"},
		{SDLK_HOME, "Home"},
		{SDLK_END, "End"},
		{SDLK_PAGEUP, "PageUp"},
		{SDLK_PAGEDOWN, "PageDown"},

		// Function keys
		{SDLK_F1, "F1"},
		{SDLK_F2, "F2"},
		{SDLK_F3, "F3"},
		{SDLK_F4, "F4"},
		{SDLK_F5, "F5"},
		{SDLK_F6, "F6"},
		{SDLK_F7, "F7"},
		{SDLK_F8, "F8"},
		{SDLK_F9, "F9"},
		{SDLK_F10, "F10"},
		{SDLK_F11, "F11"},
		{SDLK_F12, "F12"},
		{SDLK_F13, "F13"},
		{SDLK_F14, "F14"},
		{SDLK_F15, "F15"},

		// Key state modifier keys
		{SDLK_NUMLOCK, "NumLock"},
		{SDLK_CAPSLOCK, "CapsLock"},
		{SDLK_SCROLLOCK, "ScrollLock"},
		{SDLK_RSHIFT, "RightShift"},
		{SDLK_LSHIFT, "LeftShift"},
		{SDLK_RCTRL, "RightCtrl"},
		{SDLK_LCTRL, "LeftCtrl"},
		{SDLK_RALT, "RightAlt"},
		{SDLK_LALT, "LeftAlt"},
		{SDLK_RMETA, "RightMeta"},
		{SDLK_LMETA, "LeftMeta"},
		{SDLK_LSUPER, "LeftSuper"},
		{SDLK_RSUPER, "RightSuper"},
		{SDLK_MODE, "Mode"},
		{SDLK_COMPOSE, "Compose"},

		// Miscellaneous function keys
		{SDLK_HELP, "Help"},
		{SDLK_PRINT, "Print"},
		{SDLK_SYSREQ, "SysRq"},
		{SDLK_BREAK, "Break"},
		{SDLK_MENU, "Menu"},
		{SDLK_POWER, "Power"},
		{SDLK_EURO, "Euro"},
		{SDLK_UNDO, "Undo"},
		{0, NULL},
	};
	
	for (n = 0; keys[n].name; n++)
		key_add(keys[n].code, keys[n].name);
}

// ------------------------------------------------------------------------------------------------------------

typedef void (*ev_handler) (isysev_t ev, void *data);

typedef struct kmapnode kmapnode_t;

typedef struct kmap {
	kmapnode_t *root;
	char *name;
} kmap_t;

struct kmapnode {
	kmapnode_t *left, *right;
	isysev_t ev;
	ev_handler handler;
	void *data;
};


static void kmapnode_free(kmapnode_t *node)
{
	free(node);
}


static kmapnode_t *kmapnode_lookup(kmapnode_t *node, isysev_t ev)
{
	while (node && node->ev.ival != ev.ival) {
		if (ev.ival < node->ev.ival)
			node = node->left;
		else
			node = node->right;
	}
	return node;
}

static kmapnode_t *kmapnode_insert(kmapnode_t *node, kmapnode_t *new)
{
	if (!node)
		return new;
	if (new->ev.ival < node->ev.ival)
		node->left = kmapnode_insert(node->left, new);
	else
		node->right = kmapnode_insert(node->right, new);
	return node;
}


static void kmapnode_walk(kmapnode_t *node, void (*f)(kmapnode_t *))
{
	if (!node)
		return;
	if (node->left)
		kmapnode_walk(node->left, f);
	if (node->right)
		kmapnode_walk(node->right, f);
	f(node);
}

static void kmapnode_print(kmapnode_t *node)
{
	printf("ev=%08x binding=%p(%p)\n", node->ev.ival, node->handler, node->data);
}



static kmap_t *kmap_alloc(const char *name)
{
	kmap_t *m = malloc(sizeof(kmap_t));
	m->root = NULL;
	m->name = strdup(name);
	return m;
}

static void kmap_free(kmap_t *m)
{
	kmapnode_walk(m->root, kmapnode_free);
	free(m->name);
	free(m);
}


static void kmap_bind(kmap_t *m, isysev_t ev, ev_handler handler, void *data)
{
	kmapnode_t *node = kmapnode_lookup(m->root, ev);

	if (!node) {
		node = malloc(sizeof(kmapnode_t));
		node->ev = ev;
		node->left = node->right = NULL;
		m->root = kmapnode_insert(m->root, node);
	}
	node->handler = handler;
	node->data = data;
}

static int kmap_run_binding(kmap_t *m, isysev_t ev)
{
	kmapnode_t *node;

	isysev_t sev = ev; // simplified event

	// Most of the time, the key-repeat behavior is desired (e.g. arrow keys), and in the rare cases
	// where it isn't, the function that handles the event can check the flag itself.
	// Unicode is probably never useful.
	sev.bits.repeat = 0;
	sev.bits.unicode = 0;

	// If a binding was found, we're done
	node = kmapnode_lookup(m->root, sev);
	if (node) {
		node->handler(ev, node->data);
		return 1;
	}

	// If the event couldn't be found in the keymap as is, clear the dev_id and look it up again.
	// This allows for binding a fake "all" device that applies to every dev_id of its type.
	sev.bits.dev_id = 0;
	node = kmapnode_lookup(m->root, sev);
	if (node) {
		node->handler(ev, node->data);
		return 1;
	}

	// Oh well.
	return 0;
}

static void kmap_print(kmap_t *m)
{
	kmapnode_walk(m->root, kmapnode_print);
}

// ------------------------------------------------------------------------------------------------------------

static isysev_t event_parse(const char *s)
{
	int n;
	size_t len;
	char *e;
	char tmp[16];
	isysev_t ev;

	ev.ival = 0;

	// skip leading spaces
	s += strspn(s, " \t");

	// first read the device type, then optionally a slash
	
	if (*s == '@') {
		// numeric device type
		s++;
		n = strtol(s, &e, 10);
		if (s == e) {
			printf("event_parse: what kind of rubbish is this?\n");
			return (isysev_t) 0u;
		}
		ev.bits.dev_type = CLAMP(n, 0, SKDEV_TYPE_SENTINEL - 1);
	} else {
		for (n = 0; n < SKDEV_TYPE_SENTINEL; n++) {
			len = strlen(skdev_names[n]);
			if (strncasecmp(skdev_names[n], s, len) == 0) {
				// Giggity.
				ev.bits.dev_type = n;
				s += len;
				break;
			}
		}
	}

	// check for slash + number
	if (*s == '/') {
		s++;
		n = strtol(s, &e, 10);
		if (s != e) {
			ev.bits.dev_id = CLAMP(n, 0, SKDEV_ID_MAX);
			s = e;
		}
		// if (s == e) it's just a random trailing slash
		// -- let's ignore it and pretend it was a zero
	}

	len = strspn(s, " \t");
	if (n == SKDEV_TYPE_SENTINEL) {
		// none of the device types matched -- it's probably a key on the keyboard.
		ev.bits.dev_type = SKDEV_TYPE_PCKEYBOARD;
		ev.bits.dev_id = SKDEV_ID_ANY;
	} else {
		// This MIGHT be a match! Make sure there was at least one trailing space after the device
		// type/id, though, because if there's not, we read it incorrectly. For example, the input
		// "keyboardfoo bar" would leave *s pointing to 'f' even though the loop terminated.
		if (!len) {
			// Argh, this isn't an event descriptor at all, it's just junk. Time to bail.
			printf("event_parse: unknown event descriptor\n");
			return (isysev_t) 0u;
		}
	}
	s += len;
	
	if (*s == '#') {
		// Raw hexcode?
		s++;
		n = strtol(s, &e, 16);
		if (s == e) {
			// Wait, no.
			printf("event_parse: hexcode is not hex\n");
			return (isysev_t) 0u;
		}
		ev.bits.keycode = CLAMP(n, 0, SKCODE_MAX);
		s = e;
	} else if (ev.bits.dev_type == SKDEV_TYPE_PCKEYBOARD) {
		// Might be a key. Check for modifier prefixes.
		struct {
			int skmode;
			size_t len;
			char *str;
		} mod[] = {
			{SKMODE_CTRL,  4, "ctrl"},
			{SKMODE_ALT,   3, "alt"},
			{SKMODE_SHIFT, 5, "shift"},
			// alternate representations
			{SKMODE_CTRL,  7, "control"},
			{SKMODE_CTRL,  3, "ctl"},
			{SKMODE_ALT,   4, "mod1"},
			{SKMODE_ALT,   4, "meta"},
			{SKMODE_CTRL,  1, "c"},
			{SKMODE_ALT,   1, "a"},
			{SKMODE_SHIFT, 1, "s"},
			{SKMODE_ALT,   1, "m"},
			{0,            0, NULL},
		};
		
		if (*s == '^') {
			s++;
			ev.bits.modifier |= SKMODE_CTRL;
		}
		len = strcspn(s, "+-");
		n = 0;
		while (s[len] && mod[n].len) {
			if (len == mod[n].len
			    && (s[len] == '+' || s[len] == '-')
			    && strncasecmp(s, mod[n].str, len) == 0) {
			    	s += 1 + len;
				ev.bits.modifier |= mod[n].skmode;
				len = strcspn(s, "+-");
				n = 0;
			} else {
				n++;
			}
		}

		// At this point we SHOULD be looking at the key name.
		strncpy(tmp, s, 15);
		tmp[15] = 0;
		e = strpbrk(tmp, " \t");
		if (e)
			*e = 0;
		n = keytab_name_to_code(tmp);

		if (n) {
			ev.bits.keycode = n;
		} else {
			// Argh! All this work and it's not a valid key.
			printf("event_parse: unknown key \"%s\"\n", tmp);
			return (isysev_t) 0u;
		}

		s += strlen(tmp);
	} else {
		// Give up!
		printf("event_parse: invalid event descriptor for device\n");
		return (isysev_t) 0u;
	}

	len = strspn(s, " \t");
	if (len) {
		s += len;
		// If there's other junk at the end, just ignore it. ("down", maybe?)
		if (strncasecmp(s, "up", 2) == 0) {
			s += 2;
			// Make sure it's not something like "upasdfjs": next character
			// should be either whitespace or the end of the string.
			if (*s == '\0' || *s == ' ' || *s == '\t')
				ev.bits.release = 1;
		}
	}

	return ev;
}

// 'buf' should be at least 64 chars
// return: length of event string
static int event_describe(char *buf, isysev_t ev)
{
	const char *keyname;
	int len = 0;

	if (ev.bits.dev_type < SKDEV_TYPE_SENTINEL) {
		len += sprintf(buf, "%s/%d ", skdev_names[ev.bits.dev_type], ev.bits.dev_id);
	} else {
		// It's a weird mystery device!
		len += sprintf(buf, "@%d/%d ", ev.bits.dev_type, ev.bits.dev_id);
	}
	// len <= 13

	if (ev.bits.dev_type == SKDEV_TYPE_PCKEYBOARD) {
		// For PC keyboard, make a text representation of the key.
		// Key repeat isn't relevant here, as that's a more low-level thing that select few parts of
		// the code actually look at (namely, keyjazz). Also, there's no point in worrying about the
		// unicode character, since text fields don't have any special keybindings.
		if (ev.bits.modifier & SKMODE_CTRL)
			len += sprintf(buf + len, "Ctrl-");
		if (ev.bits.modifier & SKMODE_ALT)
			len += sprintf(buf + len, "Alt-");
		if (ev.bits.modifier & SKMODE_SHIFT)
			len += sprintf(buf + len, "Shift-");
		// len <= 27

		// If we have a name for this key, use it...
		keyname = keytab_code_to_name(ev.bits.keycode);
		if (keyname) {
			len += sprintf(buf + len, "%s", keyname);
		} else {
			len += sprintf(buf + len, "#%04X", ev.bits.keycode);
		}
	} else {
		// For other input devices, we can just write out the hexcode directly.
		len += sprintf(buf + len, "#%04X", ev.bits.keycode);
	}

	if (ev.bits.release) {
		len += sprintf(buf + len, " up");
	}

	return len;
}

// ------------------------------------------------------------------------------------------------------------

enum {
	KMAP_PREFIX,
	KMAP_LOCAL,
	KMAP_WIDGET,
	KMAP_WIDGETCLASS,
	KMAP_GLOBAL,

	KMAP_NUM_MAPS,
};

static kmap_t *keymaps[5];

//- local map, changes based on current page
//  keys like alt-a on sample editor
//- widget map, based on current focus
//  most custom widgets (e.g. pattern editor, envelopes) bind to this map
//- widget-class map, also based on focus
//  left/right on thumbbars
//- global map, always active
//  contains keys that didn't get overriden by the page, such as Escape
//  (the sample load page traps this key, as does the instrument envelope editor)


static void event_handle(isysev_t ev)
{
	int n;
	char buf[64];

	printf("\r%78s\r", "");
	for (n = 0; n < KMAP_NUM_MAPS; n++) {
		if (keymaps[n] && kmap_run_binding(keymaps[n], ev)) {
			printf("-- key handled by kmap #%d %s\n", n, keymaps[n]->name);
			return;
		}
	}
	// no one picked it up - fallback
	// (XXX if prefix map is set, and this isn't a modifier key, clear the prefix map)
	event_describe(buf, ev);
	printf("ev=%08x  %s\r", ev.ival, buf);
	fflush(stdout);
}


static void event_loop(void)
{
	SDL_Event sdlev;
	SDLKey lastsym = 0;
	isysev_t ev;

	while (SDL_WaitEvent(&sdlev)) {
		// Transform the SDL event into a single number
		ev.ival = 0;

		switch (sdlev.type) {

		case SDL_KEYUP:
			lastsym = 0;
			ev.bits.release = 1;
			// fall through
		case SDL_KEYDOWN:
			if (sdlev.key.which > SKDEV_ID_MAX)
				break;

			ev.bits.dev_type = SKDEV_TYPE_PCKEYBOARD;
			ev.bits.dev_id = 1 + sdlev.key.which;
			ev.bits.repeat = (sdlev.key.keysym.sym && sdlev.key.keysym.sym == lastsym);
			if (sdlev.key.state == SDL_PRESSED)
				lastsym = sdlev.key.keysym.sym;
			if (sdlev.key.keysym.unicode >= 32)
				ev.bits.unicode = 1; // XXX need to save the unicode value somewhere...

			// Scancodes are 8-bit values. Keysyms are 16-bit, but SDL only uses 9 bits of them.
			// Either way, anything we get will fit into the 15 bits we're stuffing it into.
			ev.bits.keycode = sdlev.key.keysym.sym
				? (sdlev.key.keysym.sym & ~SKCODE_PCK_SCANCODE)
				: (sdlev.key.keysym.scancode | SKCODE_PCK_SCANCODE);

			if (sdlev.key.keysym.mod & KMOD_CTRL)   ev.bits.modifier |= SKMODE_CTRL;
			if (sdlev.key.keysym.mod & KMOD_ALT)    ev.bits.modifier |= SKMODE_ALT;
			if (sdlev.key.keysym.mod & KMOD_SHIFT)  ev.bits.modifier |= SKMODE_SHIFT;

			event_handle(ev);
			break;


		case SDL_JOYBALLMOTION:
			// XXX calculate velocity from xrel/yrel and save it.
			// Certain code might be able to use this value similarly to midi note velocity...
			if (sdlev.jball.which > SKDEV_ID_MAX || sdlev.jball.ball > MAX_JS_BALLS)
				break;

			ev.bits.dev_type = SKDEV_TYPE_JOYSTICK;
			ev.bits.dev_id = 1 + sdlev.jball.which;
			if (sdlev.jball.xrel < -JS_BALL_THRESHOLD) {
				ev.bits.keycode = JS_BALL_TO_KEYCODE(sdlev.jball.ball, JS_DIR_LEFT);
				event_handle(ev);
			} else if (sdlev.jball.xrel > JS_BALL_THRESHOLD) {
				ev.bits.keycode = JS_BALL_TO_KEYCODE(sdlev.jball.ball, JS_DIR_RIGHT);
				event_handle(ev);
			}
			if (sdlev.jball.yrel < -JS_BALL_THRESHOLD) {
				ev.bits.keycode = JS_BALL_TO_KEYCODE(sdlev.jball.ball, JS_DIR_UP);
				event_handle(ev);
			} else if (sdlev.jball.yrel > JS_BALL_THRESHOLD) {
				ev.bits.keycode = JS_BALL_TO_KEYCODE(sdlev.jball.ball, JS_DIR_DOWN);
				event_handle(ev);
			}
			
			break;


		case SDL_JOYHATMOTION:
			// XXX save hat direction; handle repeat when held down; issue release events.
			if (sdlev.jhat.which > SKDEV_ID_MAX || sdlev.jhat.hat > MAX_JS_HATS)
				break;
			
			ev.bits.dev_type = SKDEV_TYPE_JOYSTICK;
			ev.bits.dev_id = 1 + sdlev.jhat.which;
			switch (sdlev.jhat.value) {
			default:
				break;
			case SDL_HAT_LEFTUP:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_LEFT);
				event_handle(ev);
				// fall through
			case SDL_HAT_UP:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_UP);
				event_handle(ev);
				break;
			case SDL_HAT_RIGHTUP:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_UP);
				event_handle(ev);
				// fall through
			case SDL_HAT_RIGHT:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_RIGHT);
				event_handle(ev);
				break;
			case SDL_HAT_LEFTDOWN:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_DOWN);
				event_handle(ev);
				// fall through
			case SDL_HAT_LEFT:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_LEFT);
				event_handle(ev);
				break;
			case SDL_HAT_RIGHTDOWN:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_RIGHT);
				event_handle(ev);
				// fall through
			case SDL_HAT_DOWN:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_DOWN);
				event_handle(ev);
				break;
			}

			break;


		case SDL_JOYAXISMOTION:
			// XXX save axis direction; handle repeat when held down; issue release events.
			if (sdlev.jbutton.which > SKDEV_ID_MAX || sdlev.jaxis.axis > MAX_JS_AXES)
				break;

			ev.bits.dev_type = SKDEV_TYPE_JOYSTICK;
			ev.bits.dev_id = 1 + sdlev.jaxis.which;
			//ev.bits.release = 0;
			if (sdlev.jaxis.value < -JS_AXIS_THRESHOLD) {
				ev.bits.keycode = JS_AXIS_TO_KEYCODE(sdlev.jaxis.axis, JS_AXIS_NEG);
				event_handle(ev);
			} else if (sdlev.jaxis.value > JS_AXIS_THRESHOLD) {
				ev.bits.keycode = JS_AXIS_TO_KEYCODE(sdlev.jaxis.axis, JS_AXIS_POS);
				event_handle(ev);
			}

			break;


		case SDL_JOYBUTTONUP:
			ev.bits.release = 1;
			// fall through
		case SDL_JOYBUTTONDOWN:
			if (sdlev.jbutton.which > SKDEV_ID_MAX || sdlev.jbutton.button > MAX_JS_BUTTONS)
				break;

			ev.bits.dev_type = SKDEV_TYPE_JOYSTICK;
			ev.bits.dev_id = 1 + sdlev.jbutton.which;
			ev.bits.keycode = JS_BUTTON_TO_KEYCODE(sdlev.jbutton.button);
			event_handle(ev);

			break;


		// Need to get midi-in events routed through here somehow.


		case SDL_QUIT:
			return;

		default:
			break;
		}
	}
}

// ------------------------------------------------------------------------------------------------------------

void ev_debug_print(isysev_t ev, void *data)
{
	printf("ev=%08x  %s\n", ev.ival, (char *) data);
}

// ------------------------------------------------------------------------------------------------------------

typedef struct dbg {
	const char *k;
	const char *s;
} dbg_t;

int main(int argc, char **argv)
{
	int n, jn;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(125, 25);

	n = SDL_NumJoysticks();
	if (n > SKDEV_ID_MAX) {
		printf("warning: %d of your %d joysticks will be ignored\n", SKDEV_ID_MAX - n, n);
		n = SKDEV_ID_MAX;
	}
	for (jn = 0; jn < n; jn++) {
		SDL_Joystick *js = SDL_JoystickOpen(jn);
		printf("Joystick #%d [%s]\n\taxes:%d buttons:%d hats:%d balls:%d\n",
			jn, SDL_JoystickName(jn),
			SDL_JoystickNumAxes(js), SDL_JoystickNumButtons(js),
			SDL_JoystickNumHats(js), SDL_JoystickNumBalls(js));
	}

	keytab_init();
	keymaps[KMAP_GLOBAL] = kmap_alloc("(global)");

	dbg_t debug[] = {
		{"q", "C-1"},
		{"2", "C#1"},
		{"w", "D-1"},
		{"3", "D#1"},
		{"e", "E-1"},
		{"r", "F-1"},
		{"5", "F#1"},
		{"t", "G-1"},
		{"6", "G#1"},
		{"y", "A-1"},
		{"7", "A#1"},
		{"u", "B-1"},
		{"i", "C-2"},
		{"Alt-Q", "Raise notes by a semitone"},
		{"Alt-A", "Lower notes by a semitone"},
		{"Alt-Shift-Q", "Raise notes by an octave"},
		{"Alt-Shift-A", "Lower notes by an octave"},
		{NULL, NULL},
	};
	for (n = 0; debug[n].k; n++)
		kmap_bind(keymaps[KMAP_GLOBAL], event_parse(debug[n].k), ev_debug_print, (void *) debug[n].s);
	
	kmap_print(keymaps[KMAP_GLOBAL]);

	SDL_JoystickEventState(SDL_ENABLE);
	SDL_SetVideoMode(200, 200, 0, 0);
	event_loop();

	kmap_free(keymaps[KMAP_GLOBAL]);
	keytab_free();

	SDL_Quit();

	return 0;
}

