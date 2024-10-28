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

/* wee....

this is used to do some schism-on-macosx customization
and get access to cocoa stuff

pruned up some here :) -mrsb

 */

/*      SDLMain.m - main entry point for our Cocoa-ized SDL app
	Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
	Non-NIB-Code & other changes: Max Horn <max@quendi.de>

	Feel free to customize this file to suit your needs
*/

extern char *initial_song;

#include <SDL.h> /* necessary here */
#include "event.h"
#include "osdefs.h"
#include "keybinds.h"

#include <crt_externs.h>

/* Old versions of SDL define "vector" as a GCC
 * builtin; this causes importing Cocoa to fail,
 * as one struct Cocoa needs is named "vector".
 *
 * This only happens on PowerPC. */
#ifdef vector
#undef vector
#endif

#import <Cocoa/Cocoa.h>

@interface SchismTrackerDelegate : NSObject
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
@end

#import <sys/param.h> /* for MAXPATHLEN */
#import <unistd.h>

/* Portions of CPS.h */
typedef struct CPSProcessSerNum
{
	UInt32 lo;
	UInt32 hi;
} CPSProcessSerNum;

extern OSErr CPSGetCurrentProcess(CPSProcessSerNum *psn);
extern OSErr CPSEnableForegroundOperation(CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr CPSSetProcessName(CPSProcessSerNum *psn, char *processname);
extern OSErr CPSSetFrontProcess(CPSProcessSerNum *psn);

static int macosx_did_finderlaunch;

@interface NSApplication(OtherMacOSXExtensions)
-(void)setAppleMenu:(NSMenu*)m;
@end

@interface SchismTracker : NSApplication
@end

@implementation SchismTracker
/* Invoked from the Quit menu item */
- (void)terminate:(id)sender
{
	/* Post a SDL_QUIT event */
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

- (void)_menu_callback:(id)sender
{
	SDL_Event e;
	NSString *px;
	const char *po;

	/* little hack to ignore keydown events here */
	NSEvent* event = [NSApp currentEvent];
	if (!event)
		return;

	if ([event type] == NSKeyDown)
		return;

	px = [sender representedObject];
	po = [px UTF8String];
	if (po) {
		e.type = SCHISM_EVENT_NATIVE;
		e.user.code = SCHISM_EVENT_NATIVE_SCRIPT;
		e.user.data1 = strdup(po);
		SDL_PushEvent(&e);
	}
}

@end

/* The main class of the application, the application's delegate */
@implementation SchismTrackerDelegate

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	SDL_Event e;
	const char *po;

	if (!filename) return NO;

	po = [filename UTF8String];
	if (po) {
		e.type = SCHISM_EVENT_NATIVE;
		e.user.code = SCHISM_EVENT_NATIVE_OPEN;
		e.user.data1 = strdup(po);
		/* if we started as a result of a doubleclick on
		 * a document, then Main still hasn't really started yet. */
		initial_song = strdup(po);
		SDL_PushEvent(&e);
		return YES;
	} else {
		return NO;
	}
}

/* other interesting ones:
- (BOOL)application:(NSApplication *)theApplication printFile:(NSString *)filename
- (BOOL)applicationOpenUntitledFile:(NSApplication *)theApplication
*/
/* Set the working directory to the .app's parent directory */
- (void) setupWorkingDirectory:(BOOL)shouldChdir
{
	if (shouldChdir)
	{
		char parentdir[MAXPATHLEN];
		CFURLRef url = CFBundleCopyBundleURL(CFBundleGetMainBundle());
		CFURLRef url2 = CFURLCreateCopyDeletingLastPathComponent(0, url);
		if (CFURLGetFileSystemRepresentation(url2, true, (unsigned char *) parentdir, MAXPATHLEN)) {
			assert ( chdir (parentdir) == 0 );   /* chdir to the binary app's parent */
		}
		CFRelease(url);
		CFRelease(url2);
	}
}

/* Called when the internal event loop has just started running */
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
	/* Set the working directory to the .app's parent directory */
	[self setupWorkingDirectory: (BOOL)macosx_did_finderlaunch];

	exit(SDL_main(*_NSGetArgc(), *_NSGetArgv()));
}

@end /* @implementation SchismTracker */

/* ------------------------------------------------------- */

/* building the menus */

static NSString *get_key_equivalent(keybind_bind_t *bind)
{
	const keybind_shortcut_t* shortcut = &bind->shortcuts[0];
	const SDL_Keycode kc = (shortcut->keycode != SDLK_UNKNOWN)
		? (shortcut->keycode)
		: (shortcut->scancode != SDL_SCANCODE_UNKNOWN)
			? SDL_GetKeyFromScancode(shortcut->scancode)
			: SDLK_UNKNOWN;

	if (kc == SDLK_UNKNOWN)
		return @"";

	switch (kc) {
#define KEQ_FN(n) [NSString stringWithFormat:@"%C", (unichar)(NSF##n##FunctionKey)]
	case SDLK_F1:  return KEQ_FN(1);
	case SDLK_F2:  return KEQ_FN(2);
	case SDLK_F3:  return KEQ_FN(3);
	case SDLK_F4:  return KEQ_FN(4);
	case SDLK_F5:  return KEQ_FN(5);
	case SDLK_F6:  return KEQ_FN(6);
	case SDLK_F7:  return KEQ_FN(7);
	case SDLK_F8:  return KEQ_FN(8);
	case SDLK_F9:  return KEQ_FN(9);
	case SDLK_F10: return KEQ_FN(10);
	case SDLK_F11: return KEQ_FN(11);
	case SDLK_F12: return KEQ_FN(12);
#undef KEQ_FN
	default:
		/* unichar is 16-bit, can't hold more than that */
		if (kc & 0x80000000 || kc > 0xFFFF)
			return @"";

		return [NSString stringWithFormat:@"%C", (unichar)kc];
	}
}

static int get_key_equivalent_modifier(keybind_bind_t *bind)
{
	const enum keybind_modifier mod = bind->shortcuts[0].modifier;
	int modifiers = 0;

	// TODO L and R variants

	if (mod & KEYBIND_MOD_CTRL)
		modifiers |= NSControlKeyMask;

	if (mod & KEYBIND_MOD_SHIFT)
		modifiers |= NSShiftKeyMask;

	if (mod & KEYBIND_MOD_ALT)
		modifiers |= NSAlternateKeyMask;

	return modifiers;
}

static void setApplicationMenu(NSMenu *menu)
{
	/* warning: this code is very odd */
	NSMenu *appleMenu;
	NSMenu *otherMenu;
	NSMenuItem *menuItem;

	appleMenu = [[NSMenu alloc] init];

	/* Add menu items */
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Help"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.help)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.help)];
	[menuItem setRepresentedObject: @"help"];

	[appleMenu addItem:[NSMenuItem separatorItem]];
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"View Patterns"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.pattern_edit)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.pattern_edit)];
	[menuItem setRepresentedObject: @"pattern"];
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Orders/Panning"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.order_list)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.order_list)];
	[menuItem setRepresentedObject: @"orders"];
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Variables"
					action:@selector(_menu_callback:)
				 keyEquivalent:get_key_equivalent(&global_keybinds_list.global.song_variables)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.song_variables)];
	[menuItem setRepresentedObject: @"variables"];
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Message Editor"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.message_editor)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.message_editor)];
	[menuItem setRepresentedObject: @"message_edit"];

	[appleMenu addItem:[NSMenuItem separatorItem]];

	[appleMenu addItemWithTitle:@"Hide Schism Tracker" action:@selector(hide:) keyEquivalent:@"h"];

	menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

	[appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

	[appleMenu addItem:[NSMenuItem separatorItem]];

	[appleMenu addItemWithTitle:@"Quit Schism Tracker" action:@selector(terminate:) keyEquivalent:@"q"];

	/* Put menu into the menubar */
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:appleMenu];
	[menu addItem:menuItem];

	/* File menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"File"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"New..."
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.new_song)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.new_song)];
	[menuItem setRepresentedObject: @"new"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Load..."
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.load_module)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.load_module)];
	[menuItem setRepresentedObject: @"load"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Save Current"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.save)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.save)];
	[menuItem setRepresentedObject: @"save"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Save As..."
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.save_module)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.save_module)];
	[menuItem setRepresentedObject: @"save_as"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Export..."
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.export_module)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.export_module)];
	[menuItem setRepresentedObject: @"export_song"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Message Log"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.schism_logging)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.schism_logging)];
	[menuItem setRepresentedObject: @"logviewer"];
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[menu addItem:menuItem];

	/* Playback menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"Playback"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Show Infopage"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.play_information_or_play_song)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.play_information_or_play_song)];
	[menuItem setRepresentedObject: @"info"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play Song"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.play_song)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.play_song)];
	[menuItem setRepresentedObject: @"play"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play Pattern"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.play_current_pattern)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.play_current_pattern)];
	[menuItem setRepresentedObject: @"play_pattern"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play from Order"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.play_song_from_order)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.play_song_from_order)];
	[menuItem setRepresentedObject: @"play_order"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play from Mark/Cursor"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.play_song_from_mark)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.play_song_from_mark)];
	[menuItem setRepresentedObject: @"play_mark"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Stop"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.stop_playback)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.stop_playback)];
	[menuItem setRepresentedObject: @"stop"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Calculate Length"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.calculate_song_length)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.calculate_song_length)];
	[menuItem setRepresentedObject: @"calc_length"];
	menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[menu addItem:menuItem];

	/* Sample menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"Samples"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Sample List"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.sample_list)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.sample_list)];
	[menuItem setRepresentedObject: @"sample_page"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Sample Library"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.sample_library)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.sample_library)];
	[menuItem setRepresentedObject: @"sample_library"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Reload Soundcard"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.audio_reset)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.audio_reset)];
	[menuItem setRepresentedObject: @"init_sound"];
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[menu addItem:menuItem];

	/* Instrument menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"Instruments"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Instrument List"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.instrument_list)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.instrument_list)];
	[menuItem setRepresentedObject: @"inst_page"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Instrument Library"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.instrument_library)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.instrument_library)];
	[menuItem setRepresentedObject: @"inst_library"];
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[menu addItem:menuItem];

	/* Settings menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"Settings"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Preferences"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.preferences)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.preferences)];
	[menuItem setRepresentedObject: @"preferences"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"MIDI Configuration"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.midi)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.midi)];
	[menuItem setRepresentedObject: @"midi_config"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Palette Editor"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.palette_config)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.palette_config)];
	[menuItem setRepresentedObject: @"palette_page"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Font Editor"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.font_editor)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.font_editor)];
	[menuItem setRepresentedObject: @"font_editor"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"System Configuration"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.system_configure)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.system_configure)];
	[menuItem setRepresentedObject: @"system_config"];

	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Toggle Fullscreen"
				action:@selector(_menu_callback:)
				keyEquivalent:get_key_equivalent(&global_keybinds_list.global.fullscreen)];
	[menuItem setKeyEquivalentModifierMask:get_key_equivalent(&global_keybinds_list.global.fullscreen)];
	[menuItem setRepresentedObject: @"fullscreen"];
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[menu addItem:menuItem];

	/* Tell the application object that this is now the application menu */
	[NSApp setAppleMenu:appleMenu];

	/* Finally give up our references to the objects */
	[appleMenu release];
	[otherMenu release];
	[menuItem release];
}

/* Create a window menu */
static void setupWindowMenu(NSMenu *menu)
{
	NSMenu      *windowMenu;
	NSMenuItem  *windowMenuItem;
	NSMenuItem  *menuItem;

	windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

	/* "Minimize" item */
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
	[windowMenu addItem:menuItem];
	[menuItem release];

	/* Put menu into the menubar */
	windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent: @""];
	[windowMenuItem setSubmenu:windowMenu];
	[menu addItem:windowMenuItem];

	/* Tell the application object that this is now the window menu */
	[NSApp setWindowsMenu:windowMenu];

	/* Finally give up our references to the objects */
	[windowMenu release];
	[windowMenuItem release];
}

#ifdef main
#  undef main
#endif

void macosx_create_menu(void)
{
	NSMenu *menu = [[NSMenu alloc] init];
	setApplicationMenu(menu);
	setupWindowMenu(menu);
	[NSApp setMainMenu: menu];
}

/* Main entry point to executable - should *not* be SDL_main! */
int main(int argc, char **argv)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	/* Copy the arguments into a global variable */
	/* This is passed if we are launched by double-clicking */
	macosx_did_finderlaunch = (argc >= 2 && strncmp (argv[1], "-psn", 4) == 0);

	{
		CPSProcessSerNum PSN;
		/* Tell the dock about us */
		if (!CPSGetCurrentProcess(&PSN)) {
			if (!macosx_did_finderlaunch)
				CPSSetProcessName(&PSN,"Schism Tracker");

			if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
				if (!CPSSetFrontProcess(&PSN))
					[SchismTracker sharedApplication];
		}
	}

	/* Ensure the application object is initialised */
	[SchismTracker sharedApplication];

	/* Create the app delegate */
	SchismTrackerDelegate *delegate = [[SchismTrackerDelegate alloc] init];
	[NSApp setDelegate: delegate];

	/* Start the main event loop */
	[NSApp run];

	[delegate release];
	[pool release];

	return 0;
}
