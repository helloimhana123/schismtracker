
#include "it.h"
#include "page.h"
#include "dmoz.h"
#include "keybinds_codes.c"

#define MAX_BINDS 200
#define MAX_SHORTCUTS 3
static int current_binds_count = 0;
static keybind_bind* current_binds[MAX_BINDS];
keybind_list global_keybinds_list;
static int has_init_problem = 0;

static int check_mods(SDL_Keymod needed, SDL_Keymod current, int lenient)
{
    int ralt_needed = needed & KMOD_RALT;
    int ctrl_needed = (needed & KMOD_LCTRL) || (needed & KMOD_RCTRL);
    int shift_needed = (needed & KMOD_LSHIFT) || (needed & KMOD_RSHIFT);
    int alt_needed = needed & KMOD_LALT;

    int has_ralt = current & KMOD_RALT;
    int has_ctrl = !has_ralt && ((current & KMOD_LCTRL) || (current & KMOD_RCTRL));
    int has_shift = (current & KMOD_LSHIFT) || (current & KMOD_RSHIFT);
    int has_alt = current & KMOD_LALT;

    if(lenient) {
        if (ctrl_needed && !has_ctrl) return 0;
        if (shift_needed && !has_shift) return 0;
        if (alt_needed && !has_alt) return 0;
        if (ralt_needed && !has_ralt) return 0;

        return 1;
    } else
        return (ctrl_needed == has_ctrl) && (shift_needed == has_shift) && (alt_needed == has_alt) && (ralt_needed == has_ralt);
}

static void update_bind(keybind_bind* bind, SDL_KeyCode kcode, SDL_Scancode scode, SDL_Keymod mods, const char* text, int is_down, int is_repeat)
{
    keybind_shortcut* sc;

    if (bind->section_info->page != status.current_page) return;

    for(int i = 0; i < bind->shortcuts_count; i++) {
        sc = &bind->shortcuts[i];
        int mods_correct = 0;
        bind->pressed = 0;
        bind->released = 0;
        bind->repeated = 0;

        if(sc->character[0] && text) {
            if (strcmp(text, sc->character) != 0) {
                continue;
            }
            mods_correct = check_mods(sc->modifier, mods, 1);
        } else if(sc->keycode != SDLK_UNKNOWN) {
            if(sc->keycode != kcode)
                continue;
            mods_correct = 1;
        } else if(sc->scancode != SDL_SCANCODE_UNKNOWN) {
            if(sc->scancode != scode)
                continue;
            mods_correct = check_mods(sc->modifier, mods, 0);
        }

        if(is_down) {
            if(mods_correct) {
                bind->pressed = !is_repeat;
                bind->repeated = is_repeat;
            }
        } else {
            bind->released = 1;
        }

        return;
    }
}

void keybinds_add_bind_shortcut(keybind_bind* bind, SDL_Keycode keycode, SDL_Scancode scancode, const char* character, SDL_Keymod modifier)
{
    if(bind->shortcuts_count == MAX_SHORTCUTS) {
        log_appendf(5, " %s/%s: Trying to bind too many shortcuts. Max is %i.", bind->section_info->name, bind->name, MAX_SHORTCUTS);
        has_init_problem = 1;
        return;
    }

    if(keycode == SDLK_UNKNOWN && scancode == SDL_SCANCODE_UNKNOWN && (!character || !character[0])) {
        printf("Attempting to bind shortcut with no key. Skipping.\n");
        return;
    }

    int i = bind->shortcuts_count;
    bind->shortcuts[i].keycode = keycode;
    bind->shortcuts[i].scancode = scancode;
    bind->shortcuts[i].modifier = modifier;
    bind->shortcuts[i].character = character;
    bind->shortcuts_count++;
}

static int string_to_mod_length = ARRAY_SIZE(string_to_mod);

static int parse_shortcut_mods(const char* shortcut, SDL_Keymod* mods)
{
    char* shortcut_dup = strdup(shortcut);
    str_to_upper(shortcut_dup);

    for (int i = 0; i < string_to_mod_length; i++) {
        if (strcmp(shortcut_dup, string_to_mod[i].name) == 0) {
            *mods |= string_to_mod[i].mod;
            return 1;
        }
    }

    return 0;
}

static int string_to_scancode_length = ARRAY_SIZE(string_to_scancode);

static int parse_shortcut_scancode(keybind_bind* bind, const char* shortcut, SDL_Scancode* code)
{
    char* shortcut_dup = strdup(shortcut);
    str_to_upper(shortcut_dup);

    for (int i = 0; i < string_to_scancode_length; i++) {
        if (strcmp(shortcut_dup, string_to_scancode[i].name) == 0) {
            *code = string_to_scancode[i].code;
            return 1;
        }
    }

    if(shortcut[0] == 'U' && shortcut[1] == 'S' && shortcut[2] == '_') {
        log_appendf(5, " %s/%s: Unknown code '%s'", bind->section_info->name, bind->name, shortcut);
        has_init_problem = 1;
        return -1;
    }

    return 0;
}

static int parse_shortcut_character(keybind_bind* bind, const char* shortcut, char** character)
{
    if(!shortcut || !shortcut[0]) return 0;

    if(utf8_length(shortcut) > 1) {
        log_appendf(5, " %s/%s: Too many characters in '%s'", bind->section_info->name, bind->name, shortcut);
        has_init_problem = 1;
        return -1;
    }

    *character = strdup(shortcut);
    return 1;
}

static void keybinds_parse_shortcut_splitted(keybind_bind* bind, const char* shortcut)
{
    char* shortcut_dup = strdup(shortcut);
    char *strtok_ptr;
    const char* delim = "+";
    char* trimmed = NULL;
    int has_problem = 0;

    SDL_Keymod mods = KMOD_NONE;
    SDL_Scancode scan_code = SDL_SCANCODE_UNKNOWN;
    SDL_Keycode key_code = SDLK_UNKNOWN;
    char* character = "\0";

    for(int i = 0;; i++) {
        const char* next = strtok_r(i == 0 ? shortcut_dup : NULL, delim, &strtok_ptr);

        if (trimmed) free(trimmed);
        trimmed = strdup(next);
        trim_string(trimmed);

        if (next == NULL) break;

        if (parse_shortcut_mods(trimmed, &mods)) continue;

        int scan_result = parse_shortcut_scancode(bind, trimmed, &scan_code);
        if (scan_result == 1) break;
        if (scan_result == -1) { has_problem = 1; break; }

        int char_result = parse_shortcut_character(bind, trimmed, &character);
        if (char_result == 1) break;
        if (char_result == -1) { has_problem = 1; break; }
    }

    if (trimmed) free(trimmed);
    free(shortcut_dup);
    if(has_problem) return;
    keybinds_add_bind_shortcut(bind, key_code, scan_code, character, mods);
}

void keybinds_parse_shortcut(keybind_bind* bind, const char* shortcut)
{
    char* shortcut_dup = strdup(shortcut);
    char *strtok_ptr;
    const char* delim = ",";

    for(int i = 0; i < 10; i++) {
        const char* next = strtok_r(i == 0 ? shortcut_dup : NULL, delim, &strtok_ptr);
        if (next == NULL) break;
        keybinds_parse_shortcut_splitted(bind, next);
    }

    free(shortcut_dup);
}

static void init_section(keybind_section_info* section_info, const char* name, const char* title, enum page_numbers page)
{
    section_info->is_active = 0;
    section_info->name = name;
    section_info->title = title;
    section_info->page = page;
}

static void set_shortcut_text(keybind_bind* bind)
{
    char* out[MAX_SHORTCUTS];
    int is_first_shortcut = 1;

    for(int i = 0; i < MAX_SHORTCUTS; i++) {
        out[i] = NULL;
        if(i >= bind->shortcuts_count) continue;

        keybind_shortcut* sc = &bind->shortcuts[i];

        const char* ctrl_text = (sc->modifier & KMOD_CTRL) ? "Ctrl-" : "";
        const char* alt_text = (sc->modifier & KMOD_LALT) ? "Alt-" : "";
        const char* shift_text = (sc->modifier & KMOD_SHIFT) ? "Shift-" : "";

        char* key_text = NULL;

        if(sc->character[0]) {
            key_text = strdup(sc->character);
        } else if(sc->scancode != SDL_SCANCODE_UNKNOWN) {
            switch(sc->scancode) {
                case SDL_SCANCODE_RETURN:
                    key_text = strdup("Enter");
                    break;
                case SDL_SCANCODE_SPACE:
                    key_text = strdup("Spacebar");
                    break;
                default:
                    SDL_Keycode code = SDL_GetKeyFromScancode(sc->scancode);
                    key_text = strdup(SDL_GetKeyName(code));
            }
        } else {
            continue;
        }

        int key_text_length = strlen(key_text);

        if (key_text_length == 0)
            continue;

        int length = strlen(ctrl_text) + strlen(alt_text) + strlen(shift_text) + key_text_length;
        char* strings[4] = { (char*)ctrl_text, (char*)alt_text, (char*)shift_text, (char*)key_text };
        // char* next_out = str_concat(ctrl_text, alt_text, shift_text, key_text, NULL);
        char* next_out = str_concat_array(4, strings, 0);

        out[i] = next_out;

        if(is_first_shortcut) {
            is_first_shortcut = 0;
            bind->first_shortcut_text = strdup(next_out);
            bind->first_shortcut_text_parens = str_concat_three(" (", next_out, ")", 0);
        }

        free(key_text);
    }

    char* shortcut_text = str_concat_with_delim(MAX_SHORTCUTS, out, ", ", 1);
    bind->shortcut_text = shortcut_text;
    if(shortcut_text[0])
        bind->shortcut_text_parens = str_concat_three(" (", shortcut_text, ")", 0);
}

static void init_bind(keybind_bind* bind, keybind_section_info* section_info, const char* name, const char* description, const char* shortcut) //, SDL_Keycode keycode, SDL_Scancode scancode, SDL_Keymod modifier)
{
    if(current_binds_count >= MAX_BINDS) {
        log_appendf(5, " Keybinds exceeding max bind count. Max is %i.", MAX_BINDS);
        has_init_problem = 1;
        return;
    }

    current_binds[current_binds_count] = bind;
    current_binds_count++;

    bind->pressed = 0;
    bind->released = 0;
    bind->shortcuts = malloc(sizeof(keybind_shortcut) * 3);

    for(int i = 0; i < 3; i++) {
        bind->shortcuts[i].keycode = SDLK_UNKNOWN;
        bind->shortcuts[i].scancode = SDL_SCANCODE_UNKNOWN;
        bind->shortcuts[i].modifier = KMOD_NONE;
    }

    bind->shortcuts_count = 0;
    bind->section_info = section_info;
    bind->name = name;
    bind->description = description;
    bind->shortcut_text = "";
    bind->first_shortcut_text = "";
    bind->shortcut_text_parens = "";
    bind->first_shortcut_text_parens = "";

    keybinds_parse_shortcut(bind, shortcut);
    set_shortcut_text(bind);
}

void keybinds_handle_event(struct key_event* event)
{
	if (event->mouse != MOUSE_NONE) {
		return;
	}

    int is_down = event->state == KEY_PRESS;
    int is_repeat = event->is_repeat;
    SDL_Keymod mods = SDL_GetModState();

    fflush(stdout);

    for (int i = 0; i < current_binds_count; i++) {
        update_bind(current_binds[i], event->orig_sym, event->scancode, mods, event->orig_text, is_down, is_repeat);
    }
}

char* keybinds_get_help_text(enum page_numbers page)
{
    const char* current_title = NULL;
    char* out = strdup("");

    for (int i = 0; i < MAX_BINDS; i++) {
        // lines[i] = NULL;

        if (i >= current_binds_count)
            continue;

        keybind_bind* bind = current_binds[i];
        const char* bind_title = bind->section_info->title;
        enum page_numbers bind_page = bind->section_info->page;

        if (bind_page != PAGE_ANY && bind_page != page)
            continue;

        char* strings[2] = { (char*)bind->description, (char*)bind->shortcut_text };

        if (current_title != bind_title) {
            char* prev_out = out;
            if(current_title)
                out = str_concat_four(out, "\n  ", (char*)bind_title, "\n", 0);
            else
                out = str_concat_four(out, "\n \n  ", (char*)bind_title, "\n", 0);
            current_title = bind_title;
            free(prev_out);
        }

        char* shortcut = str_pad_between((char*)bind->shortcut_text, "", ' ', 18, 1, 0);
        char* prev_out = out;
        out = str_concat_five(out, "    ", shortcut, (char*)bind->description, "\n", 0);
        free(shortcut);
        free(prev_out);
    }

    return out;
}

#define init_section_macro(SECTION, TITLE, PAGE) \
    init_section(&global_keybinds_list.SECTION##_info, #SECTION, TITLE, PAGE); \
    current_section_name = #SECTION; \
    current_section_info = &global_keybinds_list.SECTION##_info;

#define init_bind_macro(SECTION, BIND, DESCRIPTION, DEFAULT_KEYBIND) \
	cfg_get_string(cfg, current_section_name, #BIND, current_shortcut, 255, DEFAULT_KEYBIND); \
	cfg_set_string(cfg, current_section_name, #BIND, current_shortcut); \
    init_bind(&global_keybinds_list.SECTION.BIND, current_section_info, #BIND, DESCRIPTION, current_shortcut);

keybind_section_info* current_section_info = NULL;
static const char* current_section_name = "";
static char current_shortcut[256] = "";

static void init_instrument_list_keybinds(cfg_file_t* cfg)
{
    init_section_macro(instrument_list, "Instrument List Keys.", PAGE_INSTRUMENT_LIST);
    init_bind_macro(instrument_list, load_instrument, "Load new instrument", "US_ENTER");
    init_bind_macro(instrument_list, move_instrument_up, "Move instrument up (when not on list)", "Ctrl+US_PAGEUP");
    init_bind_macro(instrument_list, move_instrument_down, "Move instrument down", "Ctrl+US_PAGEDOWN");
    init_bind_macro(instrument_list, clear_name_and_filename, "Clean instrument name & filename", "Alt+US_C");
    init_bind_macro(instrument_list, wipe_data, "Wipe instrument data", "Alt+US_W");
    init_bind_macro(instrument_list, edit_name, "Edit instrument name (ESC to exit)\n ", "US_SPACEBAR");

    init_bind_macro(instrument_list, delete_instrument_and_samples, "Delete instrument & all related samples", "Alt+US_D");
    init_bind_macro(instrument_list, delete_instrument_and_unused_samples, "Delete instrument & all related unused samples", "Alt+Shift+US_D");
    init_bind_macro(instrument_list, post_loop_cut, "Post-loop cut envelope", "Alt+US_L");
    init_bind_macro(instrument_list, toggle_multichannel, "Toggle multichannel playback", "Alt+US_N");
    init_bind_macro(instrument_list, save_to_disk, "Save current instrument to disk", "Alt+US_O");
    init_bind_macro(instrument_list, copy, "Copy instrument", "Alt+US_P");
    init_bind_macro(instrument_list, replace_in_song, "Replace current instrument in song", "Alt+US_R");
    init_bind_macro(instrument_list, swap, "Swap instruments (in song also)", "Alt+US_S");
    init_bind_macro(instrument_list, update_pattern_data, "Update pattern data", "Alt+US_U");
    init_bind_macro(instrument_list, exchange, "Exchange instruments (only in instrument list)\n ", "Alt+US_X");

    init_bind_macro(instrument_list, insert_slot, "Insert instrument slot (updates pattern data)", "Alt+US_INSERT");
    init_bind_macro(instrument_list, remove_slot, "Remove instrument slot (updates pattern data)", "Alt+US_DELETE");
    init_bind_macro(instrument_list, increase_playback_channel, "Increase playback channel", "Shift+US_PERIOD");
    init_bind_macro(instrument_list, decrease_playback_channel, "Decrease playback channel", "Shift+US_COMMA");

    init_section_macro(instrument_note_translation, "Note Translation.", PAGE_INSTRUMENT_LIST);
    init_bind_macro(instrument_note_translation, pickup_sample_number_and_default_play_note, "Pickup sample number & default play note", "US_ENTER");
    init_bind_macro(instrument_note_translation, increase_sample_number, "Increase sample number", "Shift+US_PERIOD");
    init_bind_macro(instrument_note_translation, decrease_sample_number, "Decrease sample number\n ", "Shift+US_COMMA");

    init_bind_macro(instrument_note_translation, change_all_samples, "Change all samples", "Alt+US_A");
    init_bind_macro(instrument_note_translation, enter_next_note, "Enter next note", "Alt+US_N");
    init_bind_macro(instrument_note_translation, enter_previous_note, "Enter previous note", "Alt+US_P");
    init_bind_macro(instrument_note_translation, transpose_all_notes_semitone_up, "Transpose all notes a semitone up", "Alt+US_UP");
    init_bind_macro(instrument_note_translation, transpose_all_notes_semitone_down, "Transpose all notes a semitone down", "Alt+US_DOWN");
    init_bind_macro(instrument_note_translation, insert_row_from_table, "Insert a row from the table", "Alt+US_INSERT");
    init_bind_macro(instrument_note_translation, delete_row_from_table, "Delete a row from the table", "Alt+US_DELETE");
    init_bind_macro(instrument_note_translation, toggle_edit_mask, "Toggle edit mask for current field", "US_COMMA");

    init_section_macro(instrument_envelope, "Envelope Keys.", PAGE_INSTRUMENT_LIST);
    init_bind_macro(instrument_envelope, pick_up_or_drop_current_node, "Pick up/drop current node", "US_ENTER");
    init_bind_macro(instrument_envelope, add_node, "Add node", "US_INSERT");
    init_bind_macro(instrument_envelope, delete_node, "Delete node", "US_DELETE");
    init_bind_macro(instrument_envelope, move_node_left, "Move node left (fast)", "Alt+US_LEFT");
    init_bind_macro(instrument_envelope, move_node_right, "Move node right (fast)", "Alt+US_RIGHT");
    init_bind_macro(instrument_envelope, move_node_up, "Move node up (fast)", "Alt+US_UP");
    init_bind_macro(instrument_envelope, move_node_down, "Move node down (fast)", "Alt+US_DOWN");
    init_bind_macro(instrument_envelope, pre_loop_cut_envelope, "Pre-loop cut envelope", "Alt+US_B");
    init_bind_macro(instrument_envelope, double_envelope_length, "Double envelope length", "Alt+US_F");
    init_bind_macro(instrument_envelope, halve_envelope_length, "Halve envelope length", "Alt+US_G");
    init_bind_macro(instrument_envelope, resize_envelope, "Resize envelope", "Alt+US_E");
    init_bind_macro(instrument_envelope, generate_envelope_from_ADSR_values, "Generate envelope frome ADSR values\n ", "Alt+US_Z");

    init_bind_macro(instrument_envelope, play_default_note, "Play default note", "US_SPACEBAR");
    // init_bind_macro(instrument_envelope, note_off, "Note off command", "");
}

static void init_sample_list_keybinds(cfg_file_t* cfg)
{
    init_section_macro(sample_list, "Sample List Keys.", PAGE_SAMPLE_LIST);
    init_bind_macro(sample_list, load_new_sample, "Load new sample", "US_ENTER");
    init_bind_macro(sample_list, move_between_options, "Move between options", "US_TAB");
    init_bind_macro(sample_list, move_up, "Move up (when not on list)", "US_PAGEUP");
    init_bind_macro(sample_list, move_down, "Move down (when not on list)", "US_PAGEDOWN");
    init_bind_macro(sample_list, focus_sample_list, "Focus on sample list\n ", "Shift+US_ESCAPE");

    init_bind_macro(sample_list, convert_signed_unsigned, "Convert signed to/from unsigned samples", "Alt+US_A");
    init_bind_macro(sample_list, pre_loop_cut, "Pre-loop cut sample", "Alt+US_B");
    init_bind_macro(sample_list, clear_name_and_filename, "Clear sample name & filename (used in sample name window)", "Alt+US_C");
    init_bind_macro(sample_list, delete_sample, "Delete sample", "Alt+US_D");
    init_bind_macro(sample_list, downmix_to_mono, "Downmix stero sample to mono", "Alt+Shift+US_D");
    init_bind_macro(sample_list, resize_sample_with_interpolation, "Resize sample (with interpolation)", "Alt+US_E");
    init_bind_macro(sample_list, resize_sample_without_interpolation, "Resize sample (without interpolation)", "Alt+US_F");
    init_bind_macro(sample_list, reverse_sample, "Reverse sample", "Alt+US_G");
    init_bind_macro(sample_list, centralise_sample, "Centralise sample", "Alt+US_H");
    init_bind_macro(sample_list, invert_sample, "Invert sample", "Alt+US_I");
    init_bind_macro(sample_list, post_loop_cut, "Post-loop cut sample", "Alt+US_L");
    init_bind_macro(sample_list, sample_amplifier, "Sample amplifier", "ALT+US_M");
    init_bind_macro(sample_list, toggle_multichannel_playback, "Toggle multichannel playback", "Alt+US_N");
    init_bind_macro(sample_list, save_sample_to_disk_it, "Save current sample to disk (IT format)", "Alt+US_O");
    init_bind_macro(sample_list, copy_sample, "Copy sample", "Alt+US_P");
    init_bind_macro(sample_list, toggle_sample_quality, "Toggle sample quality", "Alt+US_Q");
    init_bind_macro(sample_list, replace_current_sample, "Replace current sample in song", "Alt+US_R");
    init_bind_macro(sample_list, swap_sample, "Swap sample (in song also)", "Alt+US_S");
    init_bind_macro(sample_list, save_sample_to_disk_format_select, "Save current sample to disk (Choose format)", "Alt+US_T");
    init_bind_macro(sample_list, save_sample_to_disk_raw, "Save current sample to disk (RAW Format)", "Alt+US_W");
    init_bind_macro(sample_list, exchange_sample, "Exchange sample (only in Sample List)", "Alt+US_X");
    init_bind_macro(sample_list, text_to_sample, "Text to sample data", "Alt+US_Y");
    init_bind_macro(sample_list, edit_create_adlib_sample, "Edit/create AdLib (FM) sample", "Alt+US_Z");
    init_bind_macro(sample_list, load_adlib_sample_by_midi_patch_number, "Load predefined AdLib sample by MIDI patch number", "Alt+Shift+US_Z");

    init_bind_macro(sample_list, insert_sample_slot, "Insert sample slot (updates pattern data)", "Alt+US_INSERT");
    init_bind_macro(sample_list, remove_sample_slot, "Remove sample slot (updates pattern data)", "Alt+US_DELETE");
    init_bind_macro(sample_list, swap_sample_with_previous, "Swap sample with previous", "Alt+US_UP");
    init_bind_macro(sample_list, swap_sample_with_next, "Swap sample with next", "Alt+US_DOWN");

    init_bind_macro(sample_list, toggle_current_sample, "Toggle current sample", "Alt+US_F9");
    init_bind_macro(sample_list, solo_current_sample, "Solo current sample", "Alt+US_F10");

    init_bind_macro(sample_list, decrease_playback_channel, "Decrease playback channel", "Alt+Shift+US_COMMA");
    init_bind_macro(sample_list, increase_playback_channel, "Increase playback channel", "Alt+Shift+US_PERIOD");

    init_bind_macro(sample_list, increase_c5_frequency_1_octave, "Increase C-5 frequency by 1 octave", "Alt+US_KP_PLUS");
    init_bind_macro(sample_list, decrease_c5_frequency_1_octave, "Decrease C-5 frequency by 1 octave", "Alt+US_KP_MINUS");
    init_bind_macro(sample_list, increase_c5_frequency_1_semitone, "Increase C-5 frequency by 1 semitone", "Ctrl+US_KP_PLUS");
    init_bind_macro(sample_list, decrease_c5_frequency_1_semitone, "Decrease C-5 frequency by 1 semitone", "Ctrl+US_KP_MINUS");
}

static void init_pattern_edit_keybinds(cfg_file_t* cfg)
{
    init_section_macro(pattern_edit, "Pattern Edit Keys.", PAGE_PATTERN_EDITOR);
    init_bind_macro(pattern_edit, next_pattern, "Next pattern (*)", "US_KP_PLUS");
    init_bind_macro(pattern_edit, previous_pattern, "Previous pattern (*)", "US_KP_MINUS");
    init_bind_macro(pattern_edit, next_4_pattern, "Next 4 pattern (*)", "Shift+US_KP_PLUS");
    init_bind_macro(pattern_edit, previous_4_pattern, "Previous 4 pattern (*)", "Shift+US_KP_MINUS");
    init_bind_macro(pattern_edit, next_order_pattern, "Next order's pattern (*)", "Ctrl+US_KP_PLUS");
    init_bind_macro(pattern_edit, previous_order_pattern, "Previous order's pattern (*)", "Ctrl+US_KP_MINUS");
    init_bind_macro(pattern_edit, use_last_value, "Use last note/instrument/volume/effect/effect value\n ", "US_SPACE");
    // init_bind_macro(pattern_edit, preview_note, "Preview note", "US_CAPSLOCK");

    init_bind_macro(pattern_edit, get_default_value, "Get default note/instrument/volume/effect", "US_ENTER");
    init_bind_macro(pattern_edit, decrease_instrument, "Decrease instrument", "Shift+US_COMMA,Ctrl+US_UP");
    init_bind_macro(pattern_edit, increase_instrument, "Increase instrument", "Shift+US_PERIOD,Ctrl+US_DOWN");
    init_bind_macro(pattern_edit, decrease_octave, "Decrease octave", "US_KP_DIVIDE,Alt+US_HOME");
    init_bind_macro(pattern_edit, increase_octave, "Increase octave", "US_KP_MULTIPLY,Alt+US_END");
    init_bind_macro(pattern_edit, toggle_edit_mask, "Toggle edit mask for current field\n ", "US_COMMA");

    init_bind_macro(pattern_edit, insert_row, "Insert a row to current channel", "US_INSERT");
    init_bind_macro(pattern_edit, delete_row, "Delete a rom from current channel", "US_DELETE");
    init_bind_macro(pattern_edit, insert_pattern_row, "Insert an entire row to pattern (*)", "Alt+US_INSERT");
    init_bind_macro(pattern_edit, delete_pattern_row, "Delete an entire row from pattern (*)", "Alt+US_DELETE");
    init_bind_macro(pattern_edit, up_by_skip, "Move up by the skip value (set with Alt 1-9)", "US_UP");
    init_bind_macro(pattern_edit, down_by_skip, "Move down by the skip value", "US_DOWN");
    init_bind_macro(pattern_edit, set_skip_1, "Set skip value to 1", "Alt+US_1");
    init_bind_macro(pattern_edit, set_skip_2, "Set skip value to 2", "Alt+US_2");
    init_bind_macro(pattern_edit, set_skip_3, "Set skip value to 3", "Alt+US_3");
    init_bind_macro(pattern_edit, set_skip_4, "Set skip value to 4", "Alt+US_4");
    init_bind_macro(pattern_edit, set_skip_5, "Set skip value to 5", "Alt+US_5");
    init_bind_macro(pattern_edit, set_skip_6, "Set skip value to 6", "Alt+US_6");
    init_bind_macro(pattern_edit, set_skip_7, "Set skip value to 7", "Alt+US_7");
    init_bind_macro(pattern_edit, set_skip_8, "Set skip value to 8", "Alt+US_8");
    init_bind_macro(pattern_edit, set_skip_9, "Set skip value to 9", "Alt+US_9");

    init_bind_macro(pattern_edit, up_one_row, "Move up by 1 row", "Ctrl+US_HOME");
    init_bind_macro(pattern_edit, down_one_row, "Move down by 1 row", "Ctrl+US_END");
    init_bind_macro(pattern_edit, slide_pattern_up, "Slide pattern up by 1 row", "Alt+US_UP");
    init_bind_macro(pattern_edit, slide_pattern_down, "Slide pattern down by 1 row", "Alt+US_DOWN");
    init_bind_macro(pattern_edit, move_cursor_left, "Move cursor left", "US_LEFT");
    init_bind_macro(pattern_edit, move_cursor_right, "Move cursor right", "US_RIGHT");
    init_bind_macro(pattern_edit, move_forwards_channel, "Move forwards one channel", "Alt+US_RIGHT"); // Check correct?
    init_bind_macro(pattern_edit, move_backwards_channel, "Move backwards one channel", "Alt+US_LEFT");
    init_bind_macro(pattern_edit, move_forwards_note_column, "Move forwards to note column", "US_TAB");
    init_bind_macro(pattern_edit, move_backwards_note_column, "Move backwards to note column", "Shift+US_TAB");
    init_bind_macro(pattern_edit, move_up_n_lines, "Move up n lines (n=Row Hilight Major)", "US_PAGEUP");
    init_bind_macro(pattern_edit, move_down_n_lines, "Move down n lines", "US_PAGEDOWN");
    init_bind_macro(pattern_edit, move_pattern_top, "Move to top of pattern", "Ctrl+US_PAGEUP");
    init_bind_macro(pattern_edit, move_pattern_bottom, "Move to bottom of pattern", "Ctrl+US_PAGEDOWN");
    init_bind_macro(pattern_edit, move_start, "Move to start of column/start of line/start of pattern", "US_HOME");
    init_bind_macro(pattern_edit, move_end, "Move to end of column/end of line/end of pattern", "US_END");
    init_bind_macro(pattern_edit, move_previous_position, "Move to previous position (accounts for Multichannel)", "US_BACKSPACE");
    init_bind_macro(pattern_edit, move_previous, "Move to previous note/instrument/volume/effect", "Shift+US_A");
    init_bind_macro(pattern_edit, move_next, "Move to next note/instrument/volume/effect\n ", "Shift+US_F");

    init_bind_macro(pattern_edit, toggle_multichannel,
        "Toggle Multichannel mode for current channel\nPress 2x: Multichannel selection menu\n ", "Alt+US_N");

    init_bind_macro(pattern_edit, store_pattern_data, "Store pattern data", "Alt+US_ENTER");
    init_bind_macro(pattern_edit, revert_pattern_data, "Revert pattern data (*)", "Alt+US_BACKSPACE");
    init_bind_macro(pattern_edit, undo, "Undo - any function with (*) can be undone\n ", "Ctrl+US_BACKSPACE");

    init_bind_macro(pattern_edit, toggle_centralise_cursor, "Toggle centralise cursor", "Ctrl+US_C");
    init_bind_macro(pattern_edit, toggle_highlight_row, "Toggle current row highlight", "Ctrl+US_H");
    init_bind_macro(pattern_edit, toggle_volume_display, "Toggle default volume display\n ", "Ctrl+US_V");

    init_bind_macro(pattern_edit, set_pattern_length, "Set pattern length", "Ctrl+US_F2");

    init_section_macro(track_view, " Track View Functions.", PAGE_PATTERN_EDITOR);
    init_bind_macro(track_view, cycle_view, "Cycle current track's view", "Alt+US_T");
    init_bind_macro(track_view, clear_track_views, "Clear all track views", "Alt+US_R");
    init_bind_macro(track_view, toggle_track_view_divisions, "Toggle track view divisions", "Alt+US_H");
    init_bind_macro(track_view, deselect_track, "Deselect current track\n ", "Ctrl+US_0");

    init_bind_macro(track_view, track_scheme_1, "View current track in scheme 1", "Ctrl+US_1");
    init_bind_macro(track_view, track_scheme_2, "View current track in scheme 2", "Ctrl+US_2");
    init_bind_macro(track_view, track_scheme_3, "View current track in scheme 3", "Ctrl+US_3");
    init_bind_macro(track_view, track_scheme_4, "View current track in scheme 4", "Ctrl+US_4");
    init_bind_macro(track_view, track_scheme_5, "View current track in scheme 5", "Ctrl+US_5");
    init_bind_macro(track_view, track_scheme_6, "View current track in scheme 6\n ", "Ctrl+US_6");

    init_bind_macro(track_view, move_column_left, "Move left between track view columns", "Ctrl+US_LEFT");
    init_bind_macro(track_view, move_column_right, "Move right between track view columns\n ", "Ctrl+US_RIGHT");

    // This doesn't work in previous version?
    init_bind_macro(track_view, quick_view_scheme_1, "Quick view scheme setup 1", "Ctrl+Shift+US_1");
    init_bind_macro(track_view, quick_view_scheme_2, "Quick view scheme setup 2", "Ctrl+Shift+US_2");
    init_bind_macro(track_view, quick_view_scheme_3, "Quick view scheme setup 3", "Ctrl+Shift+US_3");
    init_bind_macro(track_view, quick_view_scheme_4, "Quick view scheme setup 4", "Ctrl+Shift+US_4");
    init_bind_macro(track_view, quick_view_scheme_5, "Quick view scheme setup 5", "Ctrl+Shift+US_5");
    init_bind_macro(track_view, quick_view_scheme_6, "Quick view scheme setup 6\n ", "Ctrl+Shift+US_6");

    init_bind_macro(track_view, toggle_cursor_tracking, "Toggle View-Channel cursor-tracking\n ", "Ctrl+US_T");

    init_section_macro(block_functions, " Block Functions.", PAGE_PATTERN_EDITOR);
    init_bind_macro(block_functions, mark_beginning_block, "Mark beginning of block", "Alt+US_B");
    init_bind_macro(block_functions, mark_end_block, "Mark end of block", "Alt+US_E");
    init_bind_macro(block_functions, quick_mark_lines, "Quick mark n/2n/4n/... lines (n=Row Highlight Major)", "Alt+US_D");
    init_bind_macro(block_functions, mark_column_or_pattern, "Mark entire column/patter", "Alt+US_L");
    init_bind_macro(block_functions, mark_block_left, "Mark block left", "Shift+US_LEFT");
    init_bind_macro(block_functions, mark_block_right, "Mark block right", "Shift+US_RIGHT");
    init_bind_macro(block_functions, mark_block_up, "Mark block up", "Shift+US_UP");
    init_bind_macro(block_functions, mark_block_down, "Mark block down\n ", "Shift+US_DOWN");
    init_bind_macro(block_functions, mark_block_start_row, "Mark block start of row/rows/pattern", "Shift+US_HOME");
    init_bind_macro(block_functions, mark_block_end_row, "Mark block end of row/rows/pattern", "Shift+US_END");
    init_bind_macro(block_functions, mark_block_page_up, "Mark block up one page", "Shift+US_PAGEUP");
    init_bind_macro(block_functions, mark_block_page_down, "Mark block down one page\n ", "Shift+US_PAGEDOWN");

    init_bind_macro(block_functions, unmark, "Unmark block/Release clipboard memory\n ", "Alt+US_U");

    init_bind_macro(block_functions, raise_notes_semitone, "Raise notes by a semitone (*)", "Alt+US_Q");
    init_bind_macro(block_functions, raise_notes_octave, "Raise notes by an octave (*)", "Alt+Shift+US_Q");
    init_bind_macro(block_functions, lower_notes_semitone, "Lower notes by a semitone (*)", "Alt+US_A");
    init_bind_macro(block_functions, lower_notes_octave, "Lower notes by an octave (*)", "Alt+Shift+US_A");
    init_bind_macro(block_functions, set_instrument, "Set Instrument (*)", "Alt+US_S");
    init_bind_macro(block_functions, set_volume_or_panning, "Set volume/panning (*)", "Alt+US_V");
    init_bind_macro(block_functions, wipe_volume_or_panning, "Wipe vol/pan not associated with a note/instrument (*)", "Alt+US_W");
    init_bind_macro(block_functions, slide_volume_or_panning,
        "Slide volume/panning column (*)\nPress 2x: Wipe all volume/panning controls (*)", "Alt+US_K");
    // init_bind_macro(block_functions, wipe_all_volume_or_panning, "Wipe all volume/panning controls (*)", "");
    init_bind_macro(block_functions, volume_amplifier, "Volume amplifier (*) / Fast volume attenuate (*)", "Alt+US_J");
    init_bind_macro(block_functions, cut_block, "Cut block (*)", "Alt+US_Z");
    init_bind_macro(block_functions, swap_block, "Swap block (*)", "Alt+US_Y");
    init_bind_macro(block_functions, slide_effect_value,
        "Slide effect value (*)\nPress 2x: Wipe all effect data (*)\n ", "Alt+US_X");
    // init_bind_macro(block_functions, wipe_all_effect_data, "", "");

    init_bind_macro(block_functions, roll_block_down, "Roll block down", "Ctrl+US_INSERT");
    init_bind_macro(block_functions, roll_block_up, "Roll block up", "Ctrl+US_DELETE");

    init_bind_macro(block_functions, copy_block, "Copy block into clipboard", "Alt+US_C");
    init_bind_macro(block_functions, copy_block_with_mute, "Copy block to clipboard honoring current mute-settings", "Shift+US_L");
    init_bind_macro(block_functions, paste_data, "Paste data from clipboard (*)", "Alt+US_P");
    init_bind_macro(block_functions, paste_and_overwrite,
        "Overwrite with data from clipboard (*)\nPress 2x:Grow pattern to clipboard length", "Alt+US_O");
    // init_bind_macro(block_functions, grow_pattern_from_clipboard_length, "", "");
    init_bind_macro(block_functions, paste_and_mix,
        "Mix each row from clipboard with pattern data (*)\nPress 2x: Mix each field from clipboard with pattern data\n ", "Alt+US_M");
    // init_bind_macro(block_functions, paste_and_mix_field, "", "");

    init_bind_macro(block_functions, double_block_length, "Double block length (*)", "Alt+US_F");
    init_bind_macro(block_functions, halve_block_length, "Halve block length (*)", "Alt+US_G");

    init_bind_macro(block_functions, select_template_mode, "Select template mode / Fast volume amplify (*)", "Alt+US_I");
    init_bind_macro(block_functions, disable_template_mode, "Disable template mode", "Alt+Shift+US_I");
    init_bind_macro(block_functions, toggle_fast_volume, "Toggle fast volume mode", "Ctrl+US_J");
    init_bind_macro(block_functions, selection_volume_vary, "Selection volume vary / Fast volume vary (*)", "Ctrl+US_U");
    init_bind_macro(block_functions, selection_panning_vary, "Selection panning vary / Fast panning vary (*)", "Ctrl+US_Y");
    init_bind_macro(block_functions, selection_effect_vary, "Selection effect vary / Fast effect vary (*)", "Ctrl+US_K");

    init_section_macro(block_functions, " Playback Functions.", PAGE_PATTERN_EDITOR);
    init_bind_macro(playback_functions, play_note_cursor, "Play note under cursor", "US_4");
    init_bind_macro(playback_functions, play_row, "Play row", "US_8");

    init_bind_macro(playback_functions, play_from_row, "Play from current row", "Ctrl+US_F6");
    init_bind_macro(playback_functions, toggle_playback_mark, "Set/Clear playback mark (for use with F7)", "Ctrl+US_F7");

    init_bind_macro(playback_functions, toggle_current_channel, "Toggle current channel", "Alt+US_F9");
    init_bind_macro(playback_functions, solo_current_channel, "Solo current channel", "Alt+US_F10");

    init_bind_macro(playback_functions, toggle_playback_tracing, "Toggle playback tracing (also Ctrl-F)", "US_SCROLLLOCK");
    init_bind_macro(playback_functions, toggle_midi_input, "Toggle MIDI input", "Alt+US_SCROLLLOCK");
}

static void init_global_keybinds(cfg_file_t* cfg)
{
    init_section_macro(global, "Global Keys.", PAGE_ANY);
    init_bind_macro(global, help, "Help (Context Sensitive!)", "US_F1 ,US_DDD,sdf,b,c,d");
    init_bind_macro(global, midi, "MIDI Screen", "Shift+US_F1");
    init_bind_macro(global, system_configure, "System Configuration", "Ctrl+US_F1");
    init_bind_macro(global, pattern_edit, "Pattern Editor / Pattern Editor Options", "US_F2");
    init_bind_macro(global, pattern_edit_length, "Edit Pattern Length", "Ctrl+US_F2");
    init_bind_macro(global, sample_list, "Sample List", "US_F3");
    init_bind_macro(global, sample_library, "Sample Library", "Ctrl+US_F3");
    init_bind_macro(global, instrument_list, "Instrument List", "US_F4");
    init_bind_macro(global, instrument_library, "Instrument Library", "Ctrl+US_F4");
    init_bind_macro(global, play_information_or_play_song, "Play Information / Play Song", "US_F5");
    init_bind_macro(global, play_song, "Play Song", "Ctrl+US_F5");
    init_bind_macro(global, preferences, "Preferences", "Shift+US_F5");
    init_bind_macro(global, play_current_pattern, "Play Current Pattern", "US_F6");
    init_bind_macro(global, play_song_from_order, "Play Song From Current Order", "Shift+US_F6");
    init_bind_macro(global, play_song_from_mark, "Play From Mark / Current Row", "US_F7");
    init_bind_macro(global, stop_playback, "Stop Playback", "US_F8");
    init_bind_macro(global, toggle_playback, "Pause / Resume Playback", "Shift+US_F8");
    init_bind_macro(global, load_module, "Load Module", "US_F9,Ctrl+US_L");
    init_bind_macro(global, message_editor, "Message Editor", "Shift+US_F9");
    init_bind_macro(global, save_module, "Save Module", "US_F10,Ctrl+W");
    init_bind_macro(global, export_module, "Export Module (to WAV, AIFF)", "Shift+US_F10");
    init_bind_macro(global, order_list, "Order List and Panning / Channel Volume", "US_F11");
    init_bind_macro(global, schism_logging, "Schism Logging", "Ctrl+US_F11");
    init_bind_macro(global, order_list_lock, "Schism Logging", "Alt+US_F11");
    init_bind_macro(global, song_variables, "Song Variables & Directory Configuration", "US_F12");
    init_bind_macro(global, palette_config, "Palette Configuration", "Ctrl+US_F12");
    init_bind_macro(global, font_editor, "Font Editor", "Shift+US_F12");
    init_bind_macro(global, waterfall, "Waterfall\n ", "Alt+US_F12");

    init_bind_macro(global, octave_decrease, "Decrease Octave", "US_HOME");
    init_bind_macro(global, octave_increase, "Increase Octave", "US_END");
    init_bind_macro(global, decrease_playback_speed, "Decrease Playback Speed", "Shift+US_LEFTBRACKET");
    init_bind_macro(global, decrease_playback_speed, "Increase Playback Speed", "Shift+US_RIGHTBRACKET");
    init_bind_macro(global, decrease_playback_tempo, "Decrease Playback Tempo", "Ctrl+US_LEFTBRACKET");
    init_bind_macro(global, increase_playback_tempo, "Increase Playback Tempo", "Ctrl+US_RIGHTBRACKET");
    init_bind_macro(global, decrease_global_volume, "Decrease Global Volume", "US_LEFTBRACKET");
    init_bind_macro(global, increase_global_volume, "Increase Global Volume\n ", "US_RIGHTBRACKET");

    init_bind_macro(global, toggle_channel_1, "Toggle Channel 1", "Alt+US_F1");
    init_bind_macro(global, toggle_channel_2, "Toggle Channel 2", "Alt+US_F2");
    init_bind_macro(global, toggle_channel_3, "Toggle Channel 3", "Alt+US_F3");
    init_bind_macro(global, toggle_channel_4, "Toggle Channel 4", "Alt+US_F4");
    init_bind_macro(global, toggle_channel_5, "Toggle Channel 5", "Alt+US_F5");
    init_bind_macro(global, toggle_channel_6, "Toggle Channel 6", "Alt+US_F6");
    init_bind_macro(global, toggle_channel_7, "Toggle Channel 7", "Alt+US_F7");
    init_bind_macro(global, toggle_channel_8, "Toggle Channel 8\n ", "Alt+US_F8");

    init_bind_macro(global, mouse_grab, "Toggle Mouse / Keyboard Grab", "Ctrl+US_D");
    init_bind_macro(global, display_reset, "Refresh Screen And Reset Chache Identification", "Ctrl+US_E");
    init_bind_macro(global, go_to_time, "Go To Order / Pattern / Row Given Time", "Ctrl+US_G");
    init_bind_macro(global, audio_reset, "Reinitialize Sound Driver", "Ctrl+US_I");
    init_bind_macro(global, mouse, "Toggle Mouse Cursor", "Ctrl+US_M");
    init_bind_macro(global, new_song, "New Song", "Ctrl+US_N");
    init_bind_macro(global, calculate_song_length, "Calculate Approximate Song Length", "Ctrl+US_P");
    init_bind_macro(global, quit, "Quit Schism Tracker", "Ctrl+US_Q");
    init_bind_macro(global, quit_no_confirm, "Quit Without Confirmation", "Ctrl+Shift+US_Q");
    init_bind_macro(global, save, "Save Current Song\n ", "Ctrl+US_S");

    init_bind_macro(global, fullscreen, "Toggle Fullscreen\n ", "Ctrl+Alt+US_ENTER");
}

void init_keybinds(void)
{
    if (current_binds_count != 0) return;

	log_append(2, 0, "Custom Key Bindings");
	log_underline(19);

	char* path = dmoz_path_concat(cfg_dir_dotschism, "keybinds.ini");
	cfg_file_t cfg;
	cfg_init(&cfg, path);
    init_sample_list_keybinds(&cfg);
    init_pattern_edit_keybinds(&cfg);
    init_global_keybinds(&cfg);
    cfg_write(&cfg);
    cfg_free(&cfg);

    if(!has_init_problem)
        log_appendf(5, " No issues");
    log_nl();
}