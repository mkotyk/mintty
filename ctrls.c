// ctrls.c (part of mintty)
// Copyright 2008-10 Andy Koppe
// Adapted from code from PuTTY-0.60 by Simon Tatham and team.
// Licensed under the terms of the GNU General Public License v3 or later.

#include "ctrls.h"

/*
 * ctrls.c - a reasonably platform-independent mechanism for
 * describing window controls.
 */

int
ctrl_path_elements(char *path)
{
  int i = 1;
  while (*path) {
    if (*path == '/')
      i++;
    path++;
  }
  return i;
}

/* Return the number of matching path elements at the starts of p1 and p2,
 * or INT_MAX if the paths are identical. */
int
ctrl_path_compare(char *p1, char *p2)
{
  int i = 0;
  while (*p1 || *p2) {
    if ((*p1 == '/' || *p1 == '\0') && (*p2 == '/' || *p2 == '\0'))
      i++;      /* a whole element matches, ooh */
    if (*p1 != *p2)
      return i; /* mismatch */
    p1++, p2++;
  }
  return INT_MAX;       /* exact match */
}

controlbox *
ctrl_new_box(void)
{
  controlbox *ret = new(controlbox);

  ret->nctrlsets = ret->ctrlsetsize = 0;
  ret->ctrlsets = null;
  ret->nfrees = ret->freesize = 0;
  ret->frees = null;

  return ret;
}

void
ctrl_free_box(controlbox * b)
{
  for (int i = 0; i < b->nctrlsets; i++)
    ctrl_free_set(b->ctrlsets[i]);
  for (int i = 0; i < b->nfrees; i++)
    free(b->frees[i]);
  free(b->ctrlsets);
  free(b->frees);
  free(b);
}

void
ctrl_free_set(controlset *s)
{
  free(s->pathname);
  free(s->boxtitle);
  for (int i = 0; i < s->ncontrols; i++) {
    ctrl_free(s->ctrls[i]);
  }
  free(s->ctrls);
  free(s);
}

/*
 * Find the index of first controlset in a controlbox for a given
 * path. If that path doesn't exist, return the index where it
 *should be inserted.
 */
static int
ctrl_find_set(controlbox * b, char *path)
{
  int i, last, thisone;

  last = 0;
  for (i = 0; i < b->nctrlsets; i++) {
    thisone = ctrl_path_compare(path, b->ctrlsets[i]->pathname);
   /*
    * If `start' is true and there exists a controlset with
    * exactly the path we've been given, we should return the
    * index of the first such controlset we find. Otherwise,
    * we should return the index of the first entry in which
    * _fewer_ path elements match than they did last time.
    */
    if (thisone == INT_MAX || thisone < last)
      return i;
    last = thisone;
  }
  return b->nctrlsets;  /* insert at end */
}

/*
 * Find the index of next controlset in a controlbox for a given
 * path, or -1 if no such controlset exists. If -1 is passed as
 * input, finds the first.
 */
int
ctrl_find_path(controlbox * b, char *path, int index)
{
  if (index < 0)
    index = ctrl_find_set(b, path);
  else
    index++;

  if (index < b->nctrlsets && !strcmp(path, b->ctrlsets[index]->pathname))
    return index;
  else
    return -1;
}

static void
insert_controlset(controlbox *b, int index, controlset *s)
{
  if (b->nctrlsets >= b->ctrlsetsize) {
    b->ctrlsetsize = b->nctrlsets + 32;
    b->ctrlsets = renewn(b->ctrlsets, b->ctrlsetsize);
  }
  if (index < b->nctrlsets)
    memmove(&b->ctrlsets[index + 1], &b->ctrlsets[index],
            (b->nctrlsets - index) * sizeof(*b->ctrlsets));
  b->ctrlsets[index] = s;
  b->nctrlsets++;
}

/* Create a controlset. */
controlset *
ctrl_new_set(controlbox *b, char *path, char *title)
{
  // See whether this path exists already
  int index = ctrl_find_set(b, path);

  // If not, and it's not an empty path, set up a title.
  if (index == b->nctrlsets && *path) {
    char *title = strrchr(path, '/');
    title = title ? title + 1 : path;
    controlset *s = new(controlset);
    s->pathname = strdup(path);
    s->boxtitle = strdup(title);
    s->ncontrols = s->ctrlsize = 0;
    s->ncolumns = 0;      /* this is a title! */
    s->ctrls = null;
    insert_controlset(b, index, s);
    index++;
  }

  // Skip existing sets for the same path.
  while (index < b->nctrlsets && !strcmp(b->ctrlsets[index]->pathname, path))
    index++;
  
  controlset *s = new(controlset);
  s->pathname = strdup(path);
  s->boxtitle = title ? strdup(title) : null;
  s->ncolumns = 1;
  s->ncontrols = s->ctrlsize = 0;
  s->ctrls = null;
  insert_controlset(b, index, s);

  return s;
}

/* Allocate some private data in a controlbox. */
void *
ctrl_alloc(controlbox * b, size_t size)
{
  void *p;
 /*
  * This is an internal allocation routine, so it's allowed to
  * use malloc directly.
  */
  p = malloc(size);
  if (b->nfrees >= b->freesize) {
    b->freesize = b->nfrees + 32;
    b->frees = renewn(b->frees, b->freesize);
  }
  b->frees[b->nfrees++] = p;
  return p;
}

static control *
ctrl_new(controlset *s, int type, handler_fn handler, intorptr context)
{
  control *c = new(control);
  if (s->ncontrols >= s->ctrlsize) {
    s->ctrlsize = s->ncontrols + 32;
    s->ctrls = renewn(s->ctrls, s->ctrlsize);
  }
  s->ctrls[s->ncontrols++] = c;
 /*
  * Fill in the standard fields.
  */
  c->type = type;
  c->column = COLUMN_FIELD(0, s->ncolumns);
  c->handler = handler;
  c->context = context;
  c->label = null;
  c->plat_ctrl = null;
  return c;
}

/* `ncolumns' is followed by that many percentages, as integers. */
control *
ctrl_columns(controlset *s, int ncolumns, ...)
{
  control *c = ctrl_new(s, CTRL_COLUMNS, null, P(null));
  assert(s->ncolumns == 1 || ncolumns == 1);
  c->columns.ncols = ncolumns;
  s->ncolumns = ncolumns;
  if (ncolumns == 1) {
    c->columns.percentages = null;
  }
  else {
    va_list ap;
    int i;
    c->columns.percentages = newn(int, ncolumns);
    va_start(ap, ncolumns);
    for (i = 0; i < ncolumns; i++)
      c->columns.percentages[i] = va_arg(ap, int);
    va_end(ap);
  }
  return c;
}

control *
ctrl_editbox(controlset *s, char *label, int percentage,
             handler_fn handler, intorptr context, intorptr context2)
{
  control *c = ctrl_new(s, CTRL_EDITBOX, handler, context);
  c->label = label ? strdup(label) : null;
  c->editbox.percentwidth = percentage;
  c->editbox.password = 0;
  c->editbox.has_list = 0;
  c->editbox.context2 = context2;
  return c;
}

control *
ctrl_combobox(controlset *s, char *label, int percentage,
              handler_fn handler, intorptr context, intorptr context2)
{
  control *c = ctrl_new(s, CTRL_EDITBOX, handler, context);
  c->label = label ? strdup(label) : null;
  c->editbox.percentwidth = percentage;
  c->editbox.password = 0;
  c->editbox.has_list = 1;
  c->editbox.context2 = context2;
  return c;
}

/*
 * `ncolumns' is followed by (alternately) radio button titles and
 * intorptrs, until a null in place of a title string is seen.
 */
control *
ctrl_radiobuttons(controlset *s, char *label, int ncolumns,
                  handler_fn handler, intorptr context, ...)
{
  va_list ap;
  int i;
  control *c = ctrl_new(s, CTRL_RADIO, handler, context);
  c->label = label ? strdup(label) : null;
  c->radio.ncolumns = ncolumns;
 /*
  * Initial pass along variable argument list to count the
  * buttons.
  */
  va_start(ap, context);
  i = 0;
  while (va_arg(ap, char *) != null) {
    i++;
    (void) va_arg(ap, intorptr);
  }
  va_end(ap);
  c->radio.nbuttons = i;
  c->radio.buttons = newn(char *, c->radio.nbuttons);
  c->radio.buttondata = newn(intorptr, c->radio.nbuttons);
 /*
  *second pass along variable argument list to actually fill in
  * the structure.
  */
  va_start(ap, context);
  for (i = 0; i < c->radio.nbuttons; i++) {
    c->radio.buttons[i] = strdup(va_arg(ap, char *));
   /* char promotes to int in arg lists */
    c->radio.buttondata[i] = va_arg(ap, intorptr);
  }
  va_end(ap);
  return c;
}

control *
ctrl_pushbutton(controlset *s, char *label,
                handler_fn handler, intorptr context)
{
  control *c = ctrl_new(s, CTRL_BUTTON, handler, context);
  c->label = label ? strdup(label) : null;
  c->button.isdefault = 0;
  c->button.iscancel = 0;
  return c;
}

control *
ctrl_fontsel(controlset *s, char *label,
             handler_fn handler, intorptr context)
{
  control *c = ctrl_new(s, CTRL_FONTSELECT, handler, context);
  c->label = label ? strdup(label) : null;
  return c;
}

control *
ctrl_checkbox(controlset *s, char *label,
              handler_fn handler, intorptr context)
{
  control *c = ctrl_new(s, CTRL_CHECKBOX, handler, context);
  c->label = label ? strdup(label) : null;
  return c;
}

void
ctrl_free(control *ctrl)
{
  free(ctrl->label);
  switch (ctrl->type) {
    when CTRL_RADIO:
      for (int i = 0; i < ctrl->radio.nbuttons; i++)
        free(ctrl->radio.buttons[i]);
      free(ctrl->radio.buttons);

      free(ctrl->radio.buttondata);
    when CTRL_COLUMNS:
      free(ctrl->columns.percentages);
  }
  free(ctrl);
}

void
dlg_stdradiobutton_handler(control *ctrl, void *data, int event)
{
  int button;
 /*
  * For a standard radio button set, the context parameter gives
  * offsetof(targetfield, Config), and the extra data per button
  * gives the value the target field should take if that button
  * is the one selected.
  */
  if (event == EVENT_REFRESH) {
    for (button = 0; button < ctrl->radio.nbuttons; button++)
      if (atoffset(int, data, ctrl->context.i) ==
          ctrl->radio.buttondata[button].i)
        break;
   /* We expected that `break' to happen, in all circumstances. */
    assert(button < ctrl->radio.nbuttons);
    dlg_radiobutton_set(ctrl, button);
  }
  else if (event == EVENT_VALCHANGE) {
    button = dlg_radiobutton_get(ctrl);
    assert(button >= 0 && button < ctrl->radio.nbuttons);
    atoffset(int, data, ctrl->context.i) =
      ctrl->radio.buttondata[button].i;
  }
}

void
dlg_stdcheckbox_handler(control *ctrl, void *data, int event)
{
  int offset, invert;

 /*
  * For a standard checkbox, the context parameter gives
  * offsetof(targetfield, Config), optionally ORed with
  * CHECKBOX_INVERT.
  */
  offset = ctrl->context.i;
  if (offset & CHECKBOX_INVERT) {
    offset &= ~CHECKBOX_INVERT;
    invert = 1;
  }
  else
    invert = 0;

 /*
  * C lacks a logical XOR, so the following code uses the idiom
  * (!a ^ !b) to obtain the logical XOR of a and b. (That is, 1
  * iff exactly one of a and b is nonzero, otherwise 0.)
  */

  if (event == EVENT_REFRESH) {
    dlg_checkbox_set(ctrl, !atoffset(bool, data, offset) ^ !invert);
  }
  else if (event == EVENT_VALCHANGE) {
    atoffset(bool, data, offset) = !dlg_checkbox_get(ctrl) ^ !invert;
  }
}

void
dlg_stdfontsel_handler(control *ctrl, void *data, int event)
{
 /*
  * The standard font selector handler expects the `context'
  * field to contain the `offsetof' a font_spec field in the
  *structure pointed to by `data'.
  */
  int offset = ctrl->context.i;

  if (event == EVENT_REFRESH) {
    dlg_fontsel_set(ctrl, &atoffset(font_spec, data, offset));
  }
  else if (event == EVENT_VALCHANGE) {
    dlg_fontsel_get(ctrl, &atoffset(font_spec, data, offset));
  }
}
