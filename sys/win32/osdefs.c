/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Predominantly this file is keyboard crap, but we also get the network configured here */

#include "headers.h"

#include "config.h"
#include "sdlmain.h"
#include "it.h"
#include "osdefs.h"
#include "fmt.h"
#include "charset.h"

#include <windows.h>
#include <ws2tcpip.h>
#include <process.h>
#include <shlobj.h>

#define IDM_FILE_NEW  101
#define IDM_FILE_LOAD 102
#define IDM_FILE_SAVE_CURRENT 103
#define IDM_FILE_SAVE_AS 104
#define IDM_FILE_EXPORT 105
#define IDM_FILE_MESSAGE_LOG 106
#define IDM_FILE_QUIT 107
#define IDM_PLAYBACK_SHOW_INFOPAGE 201
#define IDM_PLAYBACK_PLAY_SONG 202
#define IDM_PLAYBACK_PLAY_PATTERN 203
#define IDM_PLAYBACK_PLAY_FROM_ORDER 204
#define IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR 205
#define IDM_PLAYBACK_STOP 206
#define IDM_PLAYBACK_CALCULATE_LENGTH 207
#define IDM_SAMPLES_SAMPLE_LIST 301
#define IDM_SAMPLES_SAMPLE_LIBRARY 302
#define IDM_SAMPLES_RELOAD_SOUNDCARD 303
#define IDM_INSTRUMENTS_INSTRUMENT_LIST 401
#define IDM_INSTRUMENTS_INSTRUMENT_LIBRARY 402
#define IDM_VIEW_HELP 501
#define IDM_VIEW_VIEW_PATTERNS 502
#define IDM_VIEW_ORDERS_PANNING 503
#define IDM_VIEW_VARIABLES 504
#define IDM_VIEW_MESSAGE_EDITOR 505
#define IDM_VIEW_TOGGLE_FULLSCREEN 506
#define IDM_SETTINGS_PREFERENCES 601
#define IDM_SETTINGS_MIDI_CONFIGURATION 602
#define IDM_SETTINGS_PALETTE_EDITOR 603
#define IDM_SETTINGS_FONT_EDITOR 604
#define IDM_SETTINGS_SYSTEM_CONFIGURATION 605

/* global menu object */
static HMENU menu = NULL;

/* eek... */
void win32_get_modkey(int *mk)
{
	BYTE ks[256];
	if (GetKeyboardState(ks) == 0) return;

	if (ks[VK_CAPITAL] & 128) {
		status.flags |= CAPS_PRESSED;
	} else {
		status.flags &= ~CAPS_PRESSED;
	}

	(*mk) = ((*mk) & ~(KMOD_NUM|KMOD_CAPS))
		| ((ks[VK_NUMLOCK]&1) ? KMOD_NUM : 0)
		| ((ks[VK_CAPITAL]&1) ? KMOD_CAPS : 0);
}

void win32_sysinit(UNUSED int *pargc, UNUSED char ***pargv)
{
	static WSADATA ignored = {0};

	if (WSAStartup(0x202, &ignored) == SOCKET_ERROR) {
		WSACleanup(); /* ? */
		status.flags |= NO_NETWORK;
	}

#ifdef USE_MEDIAFOUNDATION
	win32mf_init();
#endif
}

void win32_sysexit(void)
{
#ifdef USE_MEDIAFOUNDATION
	win32mf_quit();
#endif
}

void win32_sdlinit(void)
{
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
}

int win32_sdlevent(SDL_Event* event)
{
	if (event->type != SDL_SYSWMEVENT)
		return 1;

	if (event->syswm.msg->msg.win.msg == WM_COMMAND) {
		SDL_Event e;
		e.type = SCHISM_EVENT_NATIVE;
		e.user.code = SCHISM_EVENT_NATIVE_SCRIPT;
		switch (LOWORD(event->syswm.msg->msg.win.wParam)) {
			case IDM_FILE_NEW:
				e.user.data1 = "new";
				break;
			case IDM_FILE_LOAD:
				e.user.data1 = "load";
				break;
			case IDM_FILE_SAVE_CURRENT:
				e.user.data1 = "save";
				break;
			case IDM_FILE_SAVE_AS:
				e.user.data1 = "save_as";
				break;
			case IDM_FILE_EXPORT:
				e.user.data1 = "export_song";
				break;
			case IDM_FILE_MESSAGE_LOG:
				e.user.data1 = "logviewer";
				break;
			case IDM_FILE_QUIT:
				e.type = SDL_QUIT;
				break;
			case IDM_PLAYBACK_SHOW_INFOPAGE:
				e.user.data1 = "info";
				break;
			case IDM_PLAYBACK_PLAY_SONG:
				e.user.data1 = "play";
				break;
			case IDM_PLAYBACK_PLAY_PATTERN:
				e.user.data1 = "play_pattern";
				break;
			case IDM_PLAYBACK_PLAY_FROM_ORDER:
				e.user.data1 = "play_order";
				break;
			case IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR:
				e.user.data1 = "play_mark";
				break;
			case IDM_PLAYBACK_STOP:
				e.user.data1 = "stop";
				break;
			case IDM_PLAYBACK_CALCULATE_LENGTH:
				e.user.data1 = "calc_length";
				break;
			case IDM_SAMPLES_SAMPLE_LIST:
				e.user.data1 = "sample_page";
				break;
			case IDM_SAMPLES_SAMPLE_LIBRARY:
				e.user.data1 = "sample_library";
				break;
			case IDM_SAMPLES_RELOAD_SOUNDCARD:
				e.user.data1 = "init_sound";
				break;
			case IDM_INSTRUMENTS_INSTRUMENT_LIST:
				e.user.data1 = "inst_page";
				break;
			case IDM_INSTRUMENTS_INSTRUMENT_LIBRARY:
				e.user.data1 = "inst_library";
				break;
			case IDM_VIEW_HELP:
				e.user.data1 = "help";
				break;
			case IDM_VIEW_VIEW_PATTERNS:
				e.user.data1 = "pattern";
				break;
			case IDM_VIEW_ORDERS_PANNING:
				e.user.data1 = "orders";
				break;
			case IDM_VIEW_VARIABLES:
				e.user.data1 = "variables";
				break;
			case IDM_VIEW_MESSAGE_EDITOR:
				e.user.data1 = "message_edit";
				break;
			case IDM_VIEW_TOGGLE_FULLSCREEN:
				e.user.data1 = "fullscreen";
				break;
			case IDM_SETTINGS_PREFERENCES:
				e.user.data1 = "preferences";
				break;
			case IDM_SETTINGS_MIDI_CONFIGURATION:
				e.user.data1 = "midi_config";
				break;
			case IDM_SETTINGS_PALETTE_EDITOR:
				e.user.data1 = "palette_page";
				break;
			case IDM_SETTINGS_FONT_EDITOR:
				e.user.data1 = "font_editor";
				break;
			case IDM_SETTINGS_SYSTEM_CONFIGURATION:
				e.user.data1 = "system_config";
				break;
			default:
				break;
		}
		*event = e;
	}

	return 1;
}

static wchar_t* str_to_wchar(char* string, int free_inputs)
{
	wchar_t* out = NULL;
	charset_error_t result = charset_iconv(string, (uint8_t**)&out, CHARSET_UTF8, CHARSET_WCHAR_T);

	if (result != CHARSET_ERROR_SUCCESS) {
		printf("Failed converting \"%s\" to wchar. Error: %s.\n", string, charset_iconv_error_lookup(result));
		return L"";
	}

	if (free_inputs)
		free(string);

	return out;
}

void win32_toggle_menu(SDL_Window* window, int yes)
{
	const int flags = SDL_GetWindowFlags(window);
	int width, height;

	const int cache_size = !(flags & SDL_WINDOW_MAXIMIZED);
	if (cache_size)
		SDL_GetWindowSize(window, &width, &height);

	/* Get the HWND */
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if (!SDL_GetWindowWMInfo(window, &wm_info))
		return;

	SetMenu(wm_info.info.win.window, (cfg_video_want_menu_bar && !(flags & SDL_WINDOW_FULLSCREEN)) ? menu : NULL);
	DrawMenuBar(wm_info.info.win.window);

	if (cache_size)
		SDL_SetWindowSize(window, width, height);
}

void win32_create_menu(void) {
	menu = CreateMenu();

#define append_menu(MENU, MENU_ITEM, NAME, KEYBIND_NAME) \
	AppendMenuW(MENU, MF_STRING, MENU_ITEM, \
		str_to_wchar(STR_CONCAT(2, ("&" NAME "\t"), \
			global_keybinds_list.global.KEYBIND_NAME.shortcut_text), 1));

	{
		HMENU file = CreatePopupMenu();
		append_menu(file, IDM_FILE_NEW, "New", new_song);
		append_menu(file, IDM_FILE_LOAD, "Load", load_module);
		append_menu(file, IDM_FILE_SAVE_CURRENT, "Save", save);
		append_menu(file, IDM_FILE_SAVE_AS, "Save &As...", save_module);
		append_menu(file, IDM_FILE_EXPORT, "Export...", export_module);
		append_menu(file, IDM_FILE_MESSAGE_LOG, "Message Log", schism_logging);
		AppendMenuA(file, MF_SEPARATOR, 0, NULL);
		append_menu(file, IDM_FILE_QUIT, "Quit", quit);
		AppendMenuW(menu, MF_POPUP, (uintptr_t)file, L"&File");
	}
	{
		/* this is equivalent to the "Schism Tracker" menu on Mac OS X */
		HMENU view = CreatePopupMenu();
		append_menu(view, IDM_VIEW_HELP, "Help", help);
		AppendMenuW(view, MF_SEPARATOR, 0, NULL);
		append_menu(view, IDM_VIEW_VIEW_PATTERNS, "View Patterns", pattern_edit);
		append_menu(view, IDM_VIEW_ORDERS_PANNING, "Orders/Panning", order_list);
		append_menu(view, IDM_VIEW_VARIABLES, "Variables", song_variables);
		append_menu(view, IDM_VIEW_MESSAGE_EDITOR, "Message Editor", message_editor);
		AppendMenuW(view, MF_SEPARATOR, 0, NULL);
		append_menu(view, IDM_VIEW_TOGGLE_FULLSCREEN, "Toggle Fullscreen", fullscreen);
		AppendMenuW(menu, MF_POPUP, (uintptr_t)view, L"&View");
	}
	{
		HMENU playback = CreatePopupMenu();
		append_menu(playback, IDM_PLAYBACK_SHOW_INFOPAGE, "Show Infopage", play_information_or_play_song);
		append_menu(playback, IDM_PLAYBACK_PLAY_SONG, "Play Song", play_song);
		append_menu(playback, IDM_PLAYBACK_PLAY_PATTERN, "Play Pattern", play_current_pattern);
		append_menu(playback, IDM_PLAYBACK_PLAY_FROM_ORDER, "Play From Order", play_song_from_order);
		append_menu(playback, IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR, "Play From Mark / Cursor", play_song_from_mark);
		append_menu(playback, IDM_PLAYBACK_STOP, "Stop", stop_playback);
		append_menu(playback, IDM_PLAYBACK_CALCULATE_LENGTH, "Calculate Length", calculate_song_length);
		AppendMenuW(menu, MF_POPUP, (uintptr_t)playback, L"&Playback");
	}
	{
		HMENU samples = CreatePopupMenu();
		append_menu(samples, IDM_SAMPLES_SAMPLE_LIST, "Sample List", sample_list);
		append_menu(samples, IDM_SAMPLES_SAMPLE_LIBRARY, "Sample &Library", sample_library);
		append_menu(samples, IDM_SAMPLES_RELOAD_SOUNDCARD, "Reload Soundcard", audio_reset);
		AppendMenuW(menu, MF_POPUP, (uintptr_t)samples, L"&Samples");
	}
	{
		HMENU instruments = CreatePopupMenu();
		append_menu(instruments, IDM_INSTRUMENTS_INSTRUMENT_LIST, "Instrument List", instrument_list);
		append_menu(instruments, IDM_INSTRUMENTS_INSTRUMENT_LIBRARY, "Instrument Library", instrument_library);
		AppendMenuW(menu, MF_POPUP, (uintptr_t)instruments, L"&Instruments");
	}
	{
		HMENU settings = CreatePopupMenu();
		append_menu(settings, IDM_SETTINGS_PREFERENCES, "Preferences", preferences);
		append_menu(settings, IDM_SETTINGS_MIDI_CONFIGURATION, "MIDI Configuration", midi);
		append_menu(settings, IDM_SETTINGS_PALETTE_EDITOR, "Palette Editor", palette_config);
		append_menu(settings, IDM_SETTINGS_FONT_EDITOR, "Font Editor", font_editor);
		append_menu(settings, IDM_SETTINGS_SYSTEM_CONFIGURATION, "System Configuration", system_configure);
		AppendMenuW(menu, MF_POPUP, (uintptr_t)settings, L"S&ettings");
	}

#undef append_menu
}

/* -------------------------------------------------------------------- */

int win32_wstat(const wchar_t* path, struct stat* st)
{
	struct _stat mstat;

	int ws = _wstat(path, &mstat);
	if (ws < 0)
		return ws;

	/* copy all the values */
	st->st_gid = mstat.st_gid;
	st->st_atime = mstat.st_atime;
	st->st_ctime = mstat.st_ctime;
	st->st_dev = mstat.st_dev;
	st->st_ino = mstat.st_ino;
	st->st_mode = mstat.st_mode;
	st->st_mtime = mstat.st_mtime;
	st->st_nlink = mstat.st_nlink;
	st->st_rdev = mstat.st_rdev;
	st->st_size = mstat.st_size;
	st->st_uid = mstat.st_uid;

	return ws;
}

/* you may wonder: why is this needed? can't we just use
 * _mktemp() even on UTF-8 encoded strings?
 *
 * well, you *can*, but it will bite you in the ass once
 * you get a string that has a mysterious "X" stored somewhere
 * in the filename; better to just give it as a wide string */
int win32_mktemp(char* template, size_t size)
{
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)template, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	if (!_wmktemp(wc)) {
		free(wc);
		return -1;
	}

	/* still have to WideCharToMultiByte here */
	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wc, -1, template, size, NULL, NULL)) {
		free(wc);
		return -1;
	}

	free(wc);
	return 0;
}

int win32_stat(const char* path, struct stat* st)
{
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	int ret = win32_wstat(wc, st);
	free(wc);
	return ret;
}

int win32_open(const char* path, int flags)
{
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	int ret = _wopen(wc, flags);
	free(wc);
	return ret;
}

FILE* win32_fopen(const char* path, const char* flags)
{
	wchar_t* wc = NULL, *wc_flags = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T)
		|| charset_iconv((const uint8_t*)flags, (uint8_t**)&wc_flags, CHARSET_UTF8, CHARSET_WCHAR_T))
		return NULL;

	FILE* ret = _wfopen(wc, wc_flags);
	free(wc);
	free(wc_flags);
	return ret;
}

int win32_mkdir(const char *path, UNUSED mode_t mode)
{
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	int ret = _wmkdir(wc);
	free(wc);
	return ret;
}

/* ------------------------------------------------------------------------------- */
/* run hook */

int win32_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
	wchar_t cwd[PATH_MAX] = {L'\0'};
	const wchar_t *cmd = NULL;
	wchar_t batch_file[PATH_MAX] = {L'\0'};
	struct stat sb = {0};
	int r;

	if (!GetCurrentDirectoryW(PATH_MAX-1, cwd))
		return 0;

	wchar_t* name_w = NULL;
	if (charset_iconv((const uint8_t*)name, (uint8_t**)&name_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return 0;

	size_t name_len = wcslen(name_w);
	wcsncpy(batch_file, name_w, name_len);
	wcscpy(&batch_file[name_len], L".bat");

	free(name_w);

	wchar_t* dir_w = NULL;
	if (charset_iconv((const uint8_t*)dir, (uint8_t**)&dir_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return 0;

	if (_wchdir(dir_w) == -1) {
		free(dir_w);
		return 0;
	}

	free(dir_w);

	wchar_t* maybe_arg_w = NULL;
	if (charset_iconv((const uint8_t*)maybe_arg, (uint8_t**)&maybe_arg_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return 0;

	if (win32_wstat(batch_file, &sb) == -1) {
		r = 0;
	} else {
		cmd = _wgetenv(L"COMSPEC");
		if (!cmd)
			cmd = L"command.com";

		r = _wspawnlp(_P_WAIT, cmd, cmd, "/c", batch_file, maybe_arg_w, 0);
	}

	free(maybe_arg_w);

	_wchdir(cwd);
	if (r == 0) return 1;
	return 0;
}
