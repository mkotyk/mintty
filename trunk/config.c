// config.c (part of mintty)
// Copyright 2008-10 Andy Koppe
// Based on code from PuTTY-0.60 by Simon Tatham and team.
// Licensed under the terms of the GNU General Public License v3 or later.

#include "config.h"
#include "ctrls.h"
#include "print.h"
#include "charset.h"
#include "win.h"

#include <sys/cygwin.h>

const char *log_file = 0;
bool utmp_enabled = false;
hold_t hold = HOLD_DEFAULT;

#if CYGWIN_VERSION_API_MINOR >= 222
static wchar *rc_filename = 0;
#else
static char *rc_filename = 0;
#endif

config new_cfg;
config cfg = {
  // Looks
  .fg_colour = 0xBFBFBF,
  .bg_colour = 0x000000,
  .cursor_colour = 0xBFBFBF,
  .transparency = 0,
  .opaque_when_focused = false,
  .cursor_type = CUR_LINE,
  .cursor_blinks = true,
  // Text
  .font = {.name = "Lucida Console", .isbold = false, .size = 9},
  .font_quality = FQ_DEFAULT,
  .bold_as_colour = true,
  .allow_blinking = false,
  .locale = "",
  .charset = "",
  // Keys
  .backspace_sends_bs = CYGWIN_VERSION_DLL_MAJOR < 1007,
  .ctrl_alt_is_altgr = false,
  .window_shortcuts = true,
  .zoom_shortcuts = true,
  .switch_shortcuts = true,
  .scroll_mod = MDK_SHIFT,
  .pgupdn_scroll = false,
  // Mouse
  .copy_on_select = true,
  .copy_as_rtf = true,
  .clicks_place_cursor = false,
  .right_click_action = RC_SHOWMENU,
  .clicks_target_app = true,
  .click_target_mod = MDK_SHIFT,
  // Window
  .cols = 80,
  .rows = 24,
  .scrollbar = 1,
  .scrollback_lines = 10000,
  .confirm_exit = true,
  // Terminal
  .term = "xterm",
  .answerback = "",
  .bell_sound = false,
  .bell_flash = false,
  .bell_taskbar = true,
  .printer = "",
  // Hidden
  .col_spacing = 0,
  .row_spacing = 0,
  .word_chars = "",
  .use_system_colours = false,
  .ime_cursor_colour = DEFAULT_COLOUR,
  .ansi_colours = {
    0x000000, 0x0000BF, 0x00BF00, 0x00BFBF,
    0xBF0000, 0xBF00BF, 0xBFBF00, 0xBFBFBF,
    0x404040, 0x4040FF, 0x40FF40, 0x40FFFF,
    0xFF4040, 0xFF40FF, 0xFFFF40, 0xFFFFFF
  }
};

#define offcfg(option) offsetof(config, option)
#define cfg_field(option) sizeof(cfg.option), offcfg(option)

typedef enum { OPT_BOOL, OPT_INT, OPT_STRING, OPT_COLOUR, OPT_COMPAT } opt_type;

static const struct {
  const char *name;
  uchar type;
  uchar size;
  ushort offset;
}
options[] = {
  // Looks
  {"ForegroundColour", OPT_COLOUR, cfg_field(fg_colour)},
  {"BackgroundColour", OPT_COLOUR, cfg_field(bg_colour)},
  {"CursorColour", OPT_COLOUR, cfg_field(cursor_colour)},
  {"Transparency", OPT_INT, cfg_field(transparency)},
  {"OpaqueWhenFocused", OPT_BOOL, cfg_field(opaque_when_focused)},
  {"CursorType", OPT_INT, cfg_field(cursor_type)},
  {"CursorBlinks", OPT_BOOL, cfg_field(cursor_blinks)},

  // Text
  {"Font", OPT_STRING, cfg_field(font.name)},
  {"FontIsBold", OPT_BOOL, cfg_field(font.isbold)},
  {"FontHeight", OPT_INT, cfg_field(font.size)},
  {"FontQuality", OPT_INT, cfg_field(font_quality)},
  {"BoldAsColour", OPT_BOOL, cfg_field(bold_as_colour)},
  {"AllowBlinking", OPT_BOOL, cfg_field(allow_blinking)},
  {"Locale", OPT_STRING, cfg_field(locale)},
  {"Charset", OPT_STRING, cfg_field(charset)},

  // Keys
  {"BackspaceSendsBS", OPT_BOOL, cfg_field(backspace_sends_bs)},
  {"CtrlAltIsAltGr", OPT_BOOL, cfg_field(ctrl_alt_is_altgr)},
  {"WindowShortcuts", OPT_BOOL, cfg_field(window_shortcuts)},
  {"ZoomShortcuts", OPT_BOOL, cfg_field(zoom_shortcuts)},
  {"SwitchShortcuts", OPT_BOOL, cfg_field(switch_shortcuts)},
  {"ScrollMod", OPT_INT, cfg_field(scroll_mod)},
  {"PgUpDnScroll", OPT_BOOL, cfg_field(pgupdn_scroll)},

  // Mouse
  {"CopyOnSelect", OPT_BOOL, cfg_field(copy_on_select)},
  {"CopyAsRTF", OPT_BOOL, cfg_field(copy_as_rtf)},
  {"ClicksPlaceCursor", OPT_BOOL, cfg_field(clicks_place_cursor)},
  {"RightClickAction", OPT_INT, cfg_field(right_click_action)},
  {"ClicksTargetApp", OPT_INT, cfg_field(clicks_target_app)},
  {"ClickTargetMod", OPT_INT, cfg_field(click_target_mod)},

  // Window
  {"Columns", OPT_INT, cfg_field(cols)},
  {"Rows", OPT_INT, cfg_field(rows)},
  {"Scrollbar", OPT_INT, cfg_field(scrollbar)},
  {"ScrollbackLines", OPT_INT, cfg_field(scrollback_lines)},
  {"ConfirmExit", OPT_BOOL, cfg_field(confirm_exit)},

  // Terminal
  {"Term", OPT_STRING, cfg_field(term)},
  {"Answerback", OPT_STRING, cfg_field(answerback)},
  {"BellSound", OPT_BOOL, cfg_field(bell_sound)},
  {"BellFlash", OPT_BOOL, cfg_field(bell_flash)},
  {"BellTaskbar", OPT_BOOL, cfg_field(bell_taskbar)},
  {"Printer", OPT_STRING, cfg_field(printer)},

  // Hidden
  
  // Character spaceing
  {"ColSpacing", OPT_INT, cfg_field(col_spacing)},
  {"RowSpacing", OPT_INT, cfg_field(row_spacing)},
  
  // Word selection characters
  {"WordChars", OPT_STRING, cfg_field(word_chars)},
  
  // IME cursor colour
  {"IMECursorColour", OPT_COLOUR, cfg_field(ime_cursor_colour)},
  
  // ANSI colours
  {"Black", OPT_COLOUR, cfg_field(ansi_colours[BLACK_I])},
  {"Red", OPT_COLOUR, cfg_field(ansi_colours[RED_I])},
  {"Green", OPT_COLOUR, cfg_field(ansi_colours[GREEN_I])},
  {"Yellow", OPT_COLOUR, cfg_field(ansi_colours[YELLOW_I])},
  {"Blue", OPT_COLOUR, cfg_field(ansi_colours[BLUE_I])},
  {"Magenta", OPT_COLOUR, cfg_field(ansi_colours[MAGENTA_I])},
  {"Cyan", OPT_COLOUR, cfg_field(ansi_colours[CYAN_I])},
  {"White", OPT_COLOUR, cfg_field(ansi_colours[WHITE_I])},
  {"BoldBlack", OPT_COLOUR, cfg_field(ansi_colours[BOLD_BLACK_I])},
  {"BoldRed", OPT_COLOUR, cfg_field(ansi_colours[BOLD_RED_I])},
  {"BoldGreen", OPT_COLOUR, cfg_field(ansi_colours[BOLD_GREEN_I])},
  {"BoldYellow", OPT_COLOUR, cfg_field(ansi_colours[BOLD_YELLOW_I])},
  {"BoldBlue", OPT_COLOUR, cfg_field(ansi_colours[BOLD_BLUE_I])},
  {"BoldMagenta", OPT_COLOUR, cfg_field(ansi_colours[BOLD_MAGENTA_I])},
  {"BoldCyan", OPT_COLOUR, cfg_field(ansi_colours[BOLD_CYAN_I])},
  {"BoldWhite", OPT_COLOUR, cfg_field(ansi_colours[BOLD_WHITE_I])},

  // Backward compatibility
  {"UseSystemColours", OPT_BOOL | OPT_COMPAT, cfg_field(use_system_colours)},
  {"BoldAsBright", OPT_BOOL | OPT_COMPAT, cfg_field(bold_as_colour)}
};

static uchar option_order[lengthof(options)];
static uint option_order_len;

static int
find_option(char *name)
{
  for (uint i = 0; i < lengthof(options); i++) {
    if (!strcasecmp(name, options[i].name))
      return i;
  }
  return -1;
}

int
parse_option(char *option)
{
  char *eq= strchr(option, '=');
  if (!eq)
    return -1;
  
  uint name_len = eq - option;
  char name[name_len + 1];
  memcpy(name, option, name_len);
  name[name_len] = 0;
  
  int i = find_option(name);
  if (i < 0)
    return i;
  
  char *val = eq + 1;
  uint offset = options[i].offset;
  switch (options[i].type & ~OPT_COMPAT) {
    when OPT_BOOL:
      atoffset(bool, &cfg, offset) = atoi(val);
    when OPT_INT:
      atoffset(int, &cfg, offset) = atoi(val);
    when OPT_STRING:
      strlcpy(&atoffset(char, &cfg, offset), val, options[i].size);
    when OPT_COLOUR: {
      uint r, g, b;
      if (sscanf(val, "%u,%u,%u", &r, &g, &b) == 3)
        atoffset(colour, &cfg, offset) = make_colour(r, g, b);
    }
  }
  return i;
}

static void
remember_option(int i)
{
  if (!memchr(option_order, i, option_order_len))
    option_order[option_order_len++] = i;
}

void
load_config(char *filename)
{
  option_order_len = 0;

  free(rc_filename);
#if CYGWIN_VERSION_API_MINOR >= 222
  rc_filename = cygwin_create_path(CCP_POSIX_TO_WIN_W, filename);
#else
  rc_filename = strdup(filename);
#endif

  FILE *file = fopen(filename, "r");
  if (file) {
    char line[256];
    while (fgets(line, sizeof line, file)) {
      line[strcspn(line, "\r\n")] = 0;  /* trim newline */
      int i = parse_option(line);
      if (i >= 0)
        remember_option(i);
    }
    fclose(file);
  }
}

void
finish_config(void)
{
  // Ignore charset setting if we haven't got a locale.
  if (!*cfg.locale)
    *cfg.charset = 0;
  
  if (cfg.use_system_colours) {
    // Translate 'UseSystemColours' to colour settings.
    cfg.fg_colour = cfg.cursor_colour = win_get_sys_colour(true);
    cfg.bg_colour = win_get_sys_colour(false);

    // Make sure they're written to the config file.
    // This assumes that the colour options are the first three in options[].
    remember_option(0);
    remember_option(1);
    remember_option(2);
  }
}

static void
save_config(void)
{
  char *filename;

#if CYGWIN_VERSION_API_MINOR >= 222
  filename = cygwin_create_path(CCP_WIN_W_TO_POSIX, rc_filename);
#else
  filename = rc_filename;
#endif

  FILE *file = fopen(filename, "w");

  if (!file) {
    char *msg;
    int len = asprintf(&msg, "Could not save options to '%s':\n%s.",
                       filename, strerror(errno));
    if (len > 0) {
      wchar wmsg[len + 1];
      if (cs_mbstowcs(wmsg, msg, lengthof(wmsg)) >= 0)
        win_show_error(wmsg);
      free(msg);
    }
  }
  else {
    for (uint j = 0; j < option_order_len; j++) {
      uint i = option_order[j];
      if (!(options[i].type & OPT_COMPAT)) {
        fprintf(file, "%s=", options[i].name);
        uint offset = options[i].offset;
        switch (options[i].type) {
          when OPT_BOOL:
            fprintf(file, "%i\n", atoffset(bool, &cfg, offset));
          when OPT_INT:
            fprintf(file, "%i\n", atoffset(int, &cfg, offset));
          when OPT_STRING:
            fprintf(file, "%s\n", &atoffset(char, &cfg, offset));
          when OPT_COLOUR: {
            colour c = atoffset(colour, &cfg, offset);
            fprintf(file, "%u,%u,%u\n", red(c), green(c), blue(c));
          }
        }
      }
    }
    fclose(file);
  }

#if CYGWIN_VERSION_API_MINOR >= 222
  free(filename);
#endif
}


static control *cols_box, *rows_box, *locale_box, *charset_box;

static void
apply_config(void)
{
  // Record what's changed
  for (uint i = 0; i < lengthof(options); i++) {
    uint offset = options[i].offset, size = options[i].size;
    if (memcmp((char *)&cfg + offset, (char *)&new_cfg + offset, size) &&
        !memchr(option_order, i, option_order_len))
      option_order[option_order_len++] = i;
  }
  
  win_reconfig();
  save_config();
}

static void
ok_handler(control *unused(ctrl), void *unused(data), int event)
{
  if (event == EVENT_ACTION) {
    apply_config();
    dlg_end();
  }
}

static void
cancel_handler(control *unused(ctrl), void *unused(data), int event)
{
  if (event == EVENT_ACTION)
    dlg_end();
}

static void
apply_handler(control *unused(ctrl), void *unused(data), int event)
{
  if (event == EVENT_ACTION)
    apply_config();
}

static void
about_handler(control *unused(ctrl), void *unused(data), int event)
{
  if (event == EVENT_ACTION)
    win_show_about();
}

static void
current_size_handler(control *unused(ctrl), void *unused(data), int event)
{
  if (event == EVENT_ACTION) {
    new_cfg.cols = term.cols;
    new_cfg.rows = term.rows;
    dlg_refresh(cols_box);
    dlg_refresh(rows_box);
  }
}

const char PRINTER_DISABLED_STRING[] = "None (printing disabled)";

static void
printerbox_handler(control *ctrl, void *unused(data), int event)
{
  if (event == EVENT_REFRESH) {
    dlg_listbox_clear(ctrl);
    dlg_listbox_add(ctrl, PRINTER_DISABLED_STRING);
    uint num = printer_start_enum();
    for (uint i = 0; i < num; i++)
      dlg_listbox_add(ctrl, printer_get_name(i));
    printer_finish_enum();
    dlg_editbox_set(
      ctrl, *new_cfg.printer ? new_cfg.printer : PRINTER_DISABLED_STRING
    );
  }
  else if (event == EVENT_VALCHANGE || event == EVENT_SELCHANGE) {
    dlg_editbox_get(ctrl, new_cfg.printer, sizeof (cfg.printer));
    if (strcmp(new_cfg.printer, PRINTER_DISABLED_STRING) == 0)
      *new_cfg.printer = '\0';
  }
}

static void
set_charset(char *charset)
{
  strcpy(new_cfg.charset, charset);
  dlg_editbox_set(charset_box, charset);
}

static void
locale_handler(control *ctrl, void *unused(data), int event)
{
  char *locale = new_cfg.locale;
  switch (event) {
    when EVENT_REFRESH:
      dlg_listbox_clear(ctrl);
      const char *l;
      for (int i = 0; (l = locale_menu[i]); i++)
        dlg_listbox_add(ctrl, l);
      dlg_editbox_set(ctrl, locale);
    when EVENT_UNFOCUS:
      dlg_editbox_set(ctrl, locale);
      if (!*locale)
        set_charset("");
    when EVENT_VALCHANGE:
      dlg_editbox_get(ctrl, locale, sizeof cfg.locale);
    when EVENT_SELCHANGE:
      dlg_editbox_get(ctrl, locale, sizeof cfg.locale);
      if (*locale == '(' || !*locale) {
        *locale = 0;
        set_charset("");
      }
#if HAS_LOCALES
      else if (!*new_cfg.charset)
        set_charset("UTF-8");
#endif
  }
}

static void
check_locale(void)
{
  if (!*new_cfg.locale) {
    strcpy(new_cfg.locale, "C");
    dlg_editbox_set(locale_box, "C");
  }
}

static void
charset_handler(control *ctrl, void *unused(data), int event)
{
  char *charset = new_cfg.charset;
  switch (event) {
    when EVENT_REFRESH:
      dlg_listbox_clear(ctrl);
      const char *cs;
      for (int i = 0; (cs = charset_menu[i]); i++)
        dlg_listbox_add(ctrl, cs);
      dlg_editbox_set(ctrl, charset);
    when EVENT_UNFOCUS:
      dlg_editbox_set(ctrl, charset);
      if (*charset)
        check_locale();
    when EVENT_VALCHANGE:
      dlg_editbox_get(ctrl, charset, sizeof cfg.charset);
    when EVENT_SELCHANGE:
      dlg_editbox_get(ctrl, charset, sizeof cfg.charset);
      if (*charset == '(')
        *charset = 0;
      else {
        *strchr(charset, ' ') = 0;
        check_locale();
      }
  }
}

static void
colour_handler(control *ctrl, void *unused(data), int event)
{
  colour *colour_p = ctrl->context.p;
  if (event == EVENT_ACTION) {
   /*
    * Start a colour selector, which will send us an
    * EVENT_CALLBACK when it's finished and allow us to
    * pick up the results.
    */
    dlg_coloursel_start(*colour_p);
  }
  else if (event == EVENT_CALLBACK) {
   /*
    * Collect the results of the colour selector. Will
    * return nonzero on success, or zero if the colour
    * selector did nothing (user hit Cancel, for example).
    */
    colour result;
    if (dlg_coloursel_results(&result))
      *colour_p = result;
  }
}

static void
term_handler(control *ctrl, void *unused(data), int event)
{
  switch (event) {
    when EVENT_REFRESH:
      dlg_listbox_clear(ctrl);
      dlg_listbox_add(ctrl, "xterm");
      dlg_listbox_add(ctrl, "xterm-256color");
      dlg_listbox_add(ctrl, "xterm-vt220");
      dlg_listbox_add(ctrl, "vt100");
      dlg_listbox_add(ctrl, "vt220");
      dlg_editbox_set(ctrl, new_cfg.term);
    when EVENT_VALCHANGE or EVENT_SELCHANGE:
      dlg_editbox_get(ctrl, new_cfg.term, sizeof cfg.term);
  }
}

static void
int_handler(control *ctrl, void *data, int event)
{
  int offset = ctrl->context.i;
  int limit = ctrl->editbox.context2.i;
  int *field = &atoffset(int, data, offset);
  char buf[16];
  switch (event) {
    when EVENT_VALCHANGE:
      dlg_editbox_get(ctrl, buf, lengthof(buf));
      *field = max(0, min(atoi(buf), limit));
    when EVENT_REFRESH:
      sprintf(buf, "%i", *field);
      dlg_editbox_set(ctrl, buf);
  }
}

static void
string_handler(control *ctrl, void *data, int event)
{
  int offset = ctrl->context.i;
  int size = ctrl->editbox.context2.i;
  char *buf = &atoffset(char, data, offset);
  switch (event) {
    when EVENT_VALCHANGE:
      dlg_editbox_get(ctrl, buf, size);
    when EVENT_REFRESH:
      dlg_editbox_set(ctrl, buf);
  }
}

void
setup_config_box(controlbox * b)
{
  controlset *s;
  control *c;

 /*
  * The standard panel that appears at the bottom of all panels:
  * Open, Cancel, Apply etc.
  */
  s = ctrl_new_set(b, "", "");
  ctrl_columns(s, 5, 20, 20, 20, 20, 20);
  c = ctrl_pushbutton(s, "About...", 0, P(0), about_handler, P(0));
  c->column = 0;
  c = ctrl_pushbutton(s, "OK", 0, P(0), ok_handler, P(0));
  c->button.isdefault = true;
  c->column = 2;
  c = ctrl_pushbutton(s, "Cancel", 0, P(0), cancel_handler, P(0));
  c->button.iscancel = true;
  c->column = 3;
  c = ctrl_pushbutton(s, "Apply", 0, P(0), apply_handler, P(0));
  c->column = 4;

 /*
  * The Looks panel.
  */
  s = ctrl_new_set(b, "Looks", "Colours");
  ctrl_columns(s, 3, 33, 33, 33);
  ctrl_pushbutton(
    s, "Foreground...", 'f', P(0), colour_handler, P(&new_cfg.fg_colour)
  )->column = 0;
  ctrl_pushbutton(
    s, "Background...", 'b', P(0), colour_handler, P(&new_cfg.bg_colour)
  )->column = 1;
  ctrl_pushbutton(
    s, "Cursor...", 'c', P(0), colour_handler, P(&new_cfg.cursor_colour)
  )->column = 2;
  
  s = ctrl_new_set(b, "Looks", "Transparency");
  bool with_glass = win_is_glass_available();
  ctrl_radiobuttons(
    s, null, '\0', 4 + with_glass, P(0), dlg_stdradiobutton_handler,
    I(offcfg(transparency)),
    "Off", 'o', I(0),
    "Low", 'l', I(1),
    with_glass ? "Med." : "Medium", 'm', I(2), 
    "High", 'h', I(3), 
    with_glass ? "Glass" : null, 'g', I(-1), 
    null
  );
  ctrl_checkbox(
    s, "Opaque when focused", 'p', P(0),
    dlg_stdcheckbox_handler, I(offcfg(opaque_when_focused))
  );

  s = ctrl_new_set(b, "Looks", "Cursor");
  ctrl_radiobuttons(
    s, null, '\0', 4 + with_glass, P(0), dlg_stdradiobutton_handler,
    I(offcfg(cursor_type)),
    "Line", 'n', I(CUR_LINE), 
    "Block", 'k', I(CUR_BLOCK),
    "Underscore", 'u', I(CUR_UNDERSCORE),
    null
  );
  ctrl_checkbox(
    s, "Blinking", 'g', P(0), dlg_stdcheckbox_handler, I(offcfg(cursor_blinks))
  );

 /*
  * The Text panel.
  */
  s = ctrl_new_set(b, "Text", "Font");
  ctrl_fontsel(
    s, null, '\0', P(0), dlg_stdfontsel_handler, I(offcfg(font))
  );
  ctrl_radiobuttons(
    s, "Smoothing", '\0', 4, P(0), dlg_stdradiobutton_handler, 
    I(offcfg(font_quality)),
    "Default", 'd', I(FQ_DEFAULT),
    "None", 'n', I(FQ_NONANTIALIASED),
    "Partial", 'p', I(FQ_ANTIALIASED),
    "Full", 'f', I(FQ_CLEARTYPE),
    null
  );

  s = ctrl_new_set(b, "Text", null);
  ctrl_columns(s, 2, 50, 50);
  ctrl_checkbox(
    s, "Show bold as colour", 'b', P(0), dlg_stdcheckbox_handler,
    I(offcfg(bold_as_colour))
  )->column = 0;
  ctrl_checkbox(
    s, "Allow blinking", 'a', P(0),
    dlg_stdcheckbox_handler, I(offcfg(allow_blinking))
  )->column = 1;

  s = ctrl_new_set(b, "Text", null);
  ctrl_columns(s, 2, 29, 71);
  (locale_box = ctrl_combobox(
    s, "Locale", 'l', 100, P(0), locale_handler, P(0), P(0)
  ))->column = 0;
  (charset_box = ctrl_combobox(
    s, "Character set", 'c', 100, P(0), charset_handler, P(0), P(0)
  ))->column = 1;

 /*
  * The Keys panel.
  */
  s = ctrl_new_set(b, "Keys", null);
  ctrl_columns(s, 2, 50, 50);
  ctrl_checkbox(
    s, "Ctrl+LeftAlt is AltGr", 'g', P(0),
    dlg_stdcheckbox_handler, I(offcfg(ctrl_alt_is_altgr))
  )->column = 0;
  ctrl_checkbox(
    s, "Backspace sends ^H", 'b', P(0),
    dlg_stdcheckbox_handler, I(offcfg(backspace_sends_bs))
  )->column = 1;

  s = ctrl_new_set(b, "Keys", "Shortcuts");
  ctrl_checkbox(
    s, "Menu and Full Screen (Alt+Space/Enter)", 'm', P(0),
    dlg_stdcheckbox_handler, I(offcfg(window_shortcuts))
  );
  ctrl_checkbox(
    s, "Switch window (Ctrl+[Shift+]Tab)", 'w', P(0),
    dlg_stdcheckbox_handler, I(offcfg(switch_shortcuts))
  );
  ctrl_checkbox(
    s, "Zoom (Ctrl+plus/minus/zero)", 'z', P(0),
    dlg_stdcheckbox_handler, I(offcfg(zoom_shortcuts))
  );
  
  s = ctrl_new_set(b, "Keys", "Modifier for scrolling");
  ctrl_radiobuttons(
    s, null, '\0', 4, P(0),      
    dlg_stdradiobutton_handler, I(offcfg(scroll_mod)),
    "Off", 'o', I(0),
    "Shift", 's', I(MDK_SHIFT),
    "Ctrl", 'c', I(MDK_CTRL),
    "Alt", 'a', I(MDK_ALT),
    null
  );
  ctrl_checkbox(
    s, "PgUp and PgDn scroll without modifier", 'p', P(0),
    dlg_stdcheckbox_handler, I(offcfg(pgupdn_scroll))
  );

 /*
  * The Mouse panel.
  */
  s = ctrl_new_set(b, "Mouse", null);
  ctrl_columns(s, 2, 50, 50);
  ctrl_checkbox(
    s, "Copy on select", 'y', P(0),
    dlg_stdcheckbox_handler, I(offcfg(copy_on_select))
  )->column = 0;
  ctrl_checkbox(
    s, "Copy as rich text", 'r', P(0),
    dlg_stdcheckbox_handler, I(offcfg(copy_as_rtf))
  )->column = 1;
  ctrl_checkbox(
    s, "Clicks place command line cursor", 'k', P(0),
    dlg_stdcheckbox_handler, I(offcfg(clicks_place_cursor))
  );

  s = ctrl_new_set(b, "Mouse", "Right click action");
  ctrl_radiobuttons(
    s, null, '\0', 4, P(0), dlg_stdradiobutton_handler,
    I(offcfg(right_click_action)),
    "Paste", 'p', I(RC_PASTE),
    "Extend", 'x', I(RC_EXTEND),
    "Show menu", 'm', I(RC_SHOWMENU),
    null
  );
  
  s = ctrl_new_set(b, "Mouse", "Application mouse mode");
  ctrl_radiobuttons(
    s, "Default click target", '\0', 4, P(0), dlg_stdradiobutton_handler,
    I(offcfg(clicks_target_app)),
    "Window", 'w', I(0),
    "Application", 'n', I(1),
    null
  );
  ctrl_radiobuttons(
    s, "Modifier for overriding default", '\0', 4, P(0),
    dlg_stdradiobutton_handler, I(offcfg(click_target_mod)),
    "Off", 'o', I(0),
    "Shift", 's', I(MDK_SHIFT),
    "Ctrl", 'c', I(MDK_CTRL),
    "Alt", 'a', I(MDK_ALT),
    null
  );
  
 /*
  * The Window panel.
  */
  s = ctrl_new_set(b, "Window", "Default size");
  ctrl_columns(s, 5, 35, 3, 28, 4, 30);
  (cols_box = ctrl_editbox(
    s, "Columns", 'c', 44, P(0), int_handler, I(offcfg(cols)), I(256)
  ))->column = 0;
  (rows_box = ctrl_editbox(
    s, "Rows", 'w', 55, P(0), int_handler, I(offcfg(rows)), I(256)
  ))->column = 2;
  ctrl_pushbutton(
    s, "Current size", 'u', P(0), current_size_handler, P(0)
  )->column = 4;

  s = ctrl_new_set(b, "Window", "Scrollback");
  ctrl_columns(s, 2, 45, 55);
  ctrl_editbox(
    s, "Lines", 's', 57, P(0),
    int_handler, I(offcfg(scrollback_lines)), I(1000000)
  )->column = 0;
  ctrl_radiobuttons(
    s, "Scrollbar", '\0', 5, P(0),
    dlg_stdradiobutton_handler, I(offcfg(scrollbar)),
    "Left", 'l', I(-1),
    "None", 'n', I(0),
    "Right", 'r', I(1),
    null
  );

  s = ctrl_new_set(b, "Window", null);
  ctrl_checkbox(
    s, "Ask for exit confirmation", 'x', P(0),
    dlg_stdcheckbox_handler, I(offcfg(confirm_exit))
  );

 /*
  * The Emulation panel.
  */
  s = ctrl_new_set(b, "Terminal", null);
  ctrl_columns(s, 2, 50, 50);
  ctrl_combobox(
    s, "Type", 't', 100, P(0), term_handler, P(0), P(0)
  )->column = 0;
  ctrl_editbox(
    s, "Answerback", 'a', 100, P(0),
    string_handler, I(offcfg(answerback)), I(sizeof cfg.answerback)
  )->column = 1;

  s = ctrl_new_set(b, "Terminal", "Bell");
  ctrl_columns(s, 3, 25, 25, 50);
  ctrl_checkbox(
    s, "Sound", 's', P(0),
    dlg_stdcheckbox_handler, I(offcfg(bell_sound))
  )->column = 0;
  ctrl_checkbox(
    s, "Flash", 'f', P(0),
    dlg_stdcheckbox_handler, I(offcfg(bell_flash))
  )->column = 1;
  ctrl_checkbox(
    s, "Taskbar highlight", 'h', P(0),
    dlg_stdcheckbox_handler, I(offcfg(bell_taskbar))
  )->column = 2;

  s = ctrl_new_set(b, "Terminal", "Printer");
  ctrl_combobox(
    s, null, '\0', 100, P(0), printerbox_handler, P(0), P(0)
  );
}
