/* -*- c -*- */

/* Copyright (C) 2004-2015 Alexander Chernov <cher@ejudge.ru> */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ejudge/ej_limits.h"
#include "ejudge/team_extra.h"
#include "ejudge/pathutl.h"
#include "ejudge/errlog.h"
#include "ejudge/prepare.h"
#include "ejudge/common_plugin.h"
#include "ejudge/xuser_plugin.h"

#include "ejudge/xalloc.h"
#include "ejudge/logger.h"
#include "ejudge/osdeps.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined __GNUC__ && defined __MINGW32__
#include <malloc.h>
#endif

#define MAX_USER_ID_32DIGITS 4
#define BPE (CHAR_BIT * sizeof(((struct team_extra*)0)->clar_map[0]))

struct team_extra_state
{
  unsigned char *team_extra_dir;
  size_t team_map_size;
  struct team_extra **team_map;
};

static const unsigned char b32_digits[]=
"0123456789ABCDEFGHIJKLMNOPQRSTUV";
static void
b32_number(unsigned num, size_t size, unsigned char buf[])
{
  int i;

  ASSERT(size > 1);

  memset(buf, '0', size - 1);
  buf[size - 1] = 0;
  i = size - 2;
  while (num > 0 && i >= 0) {
    buf[i] = b32_digits[num & 0x1f];
    i--;
    num >>= 5;
  }
  ASSERT(!num);
}

struct team_extra *
team_extra_free(struct team_extra *te)
{
  if (te) {
    int j;
    struct team_warning *tw;

    for (j = 0; j < te->warn_u; j++) {
      if (!(tw = te->warns[j])) continue;
      xfree(tw->text);
      xfree(tw->comment);
      xfree(tw);
    }
    xfree(te->warns);
    xfree(te->clar_map);
    xfree(te->disq_comment);
    xfree(te);
  }
  return NULL;
}

team_extra_state_t
team_extra_destroy(team_extra_state_t state)
{
  int i;

  if (!state) return 0;
  xfree(state->team_extra_dir);
  for (i = 0; i < state->team_map_size; i++) {
    team_extra_free(state->team_map[i]);
  }
  xfree(state->team_map);
  memset(state, 0, sizeof(*state));
  xfree(state);
  return 0;
}

static int
make_read_path(team_extra_state_t state, unsigned char *path, size_t size,
               int user_id)
{
  unsigned char b32[16];

  ASSERT(user_id > 0 && user_id <= EJ_MAX_USER_ID);
  b32_number(user_id, MAX_USER_ID_32DIGITS + 1, b32);
  return snprintf(path, size, "%s/%c/%c/%c/%06d.xml",
                  state->team_extra_dir, b32[0], b32[1], b32[2], user_id);
}

static int
make_write_path(team_extra_state_t state, unsigned char *path, size_t size,
                int user_id)
{
  unsigned char b32[16];
  unsigned char *mpath = 0, *p;
  //struct stat sb;
  int i;

  ASSERT(user_id > 0 && user_id <= EJ_MAX_USER_ID);
  b32_number(user_id, MAX_USER_ID_32DIGITS + 1, b32);

  mpath = alloca(strlen(state->team_extra_dir) + 32);
  strcpy(mpath, state->team_extra_dir);
  p = mpath + strlen(mpath);
  for (i = 0; i < MAX_USER_ID_32DIGITS - 1; i++) {
    *p++ = '/';
    *p++ = b32[i];
    *p = 0;
    if (os_MakeDir(mpath, 0770) < 0) {
      if (errno != EEXIST) {
        err("team_extra: %s: mkdir failed: %s", mpath, os_ErrorMsg());
        return -1;
      }
      /*
      if (lstat(mpath, &sb) < 0) {
        err("team_extra: %s: lstat failed: %s", mpath, os_ErrorMsg());
        return -1;
      }
      if (!S_ISDIR(sb.st_mode)) {
        err("team_extra: %s: is not a directory", mpath);
        return -1;
      }
      */
    }
  }

  return snprintf(path, size, "%s/%c/%c/%c/%06d.xml",
                  state->team_extra_dir, b32[0], b32[1], b32[2], user_id);
}

static void
extend_team_map(team_extra_state_t state, int user_id)
{
  size_t new_size = state->team_map_size;
  struct team_extra **new_map = 0;

  if (!new_size) new_size = 32;
  while (new_size <= user_id) new_size *= 2;
  XCALLOC(new_map, new_size);
  if (state->team_map_size > 0) {
    memcpy(new_map, state->team_map, state->team_map_size * sizeof(new_map[0]));
    xfree(state->team_map);
  }
  state->team_map = new_map;
  state->team_map_size = new_size;
}

static struct team_extra *
get_entry(team_extra_state_t state, int user_id, int try_flag)
{
  struct team_extra *te = state->team_map[user_id];
  path_t rpath;
  //struct stat sb;

  if (te) return te;

  make_read_path(state, rpath, sizeof(rpath), user_id);
  if (os_CheckAccess(rpath, REUSE_F_OK) < 0) {
    if (try_flag) return NULL;
    XCALLOC(te, 1);
    te->user_id = user_id;
    state->team_map[user_id] = te;
    return te;
  }
  /*    
  if (lstat(rpath, &sb) < 0) {
    XCALLOC(te, 1);
    te->user_id = user_id;
    state->team_map[user_id] = te;
    return te;
  }
  if (!S_ISREG(sb.st_mode)) {
    err("team_extra: %s: not a regular file", rpath);
    state->team_map[user_id] = (struct team_extra*) -1;
    return (struct team_extra*) -1;
  }
  */
  if (team_extra_parse_xml(rpath, &te) < 0) {
    state->team_map[user_id] = (struct team_extra*) -1;
    return (struct team_extra*) -1;
  }
  if (te->user_id != user_id) {
    err("team_extra: %s: user_id mismatch: %d, %d",
        rpath, te->user_id, user_id);
    state->team_map[user_id] = (struct team_extra*) -1;
    return (struct team_extra*) -1;
  }
  state->team_map[user_id] = te;
  return te;
}

const struct team_extra*
team_extra_get_entry(team_extra_state_t state, int user_id)
{
  struct team_extra *tmpval;

  ASSERT(user_id > 0 && user_id <= EJ_MAX_USER_ID);
  if (user_id >= state->team_map_size) extend_team_map(state, user_id);

  tmpval = get_entry(state, user_id, 0);
  if (tmpval == (struct team_extra*) -1) tmpval = 0;
  return tmpval;
}

void
team_extra_extend_clar_map(struct team_extra *te, int clar_id)
{
  int new_size = te->clar_map_size;
  int new_alloc;
  unsigned long *new_map = 0;

  if (!new_size) new_size = 128;
  while (new_size <= clar_id) new_size *= 2;
  new_alloc = new_size / BPE;
  XCALLOC(new_map, new_alloc);
  if (te->clar_map_size > 0) {
    memcpy(new_map, te->clar_map, sizeof(new_map[0]) * te->clar_map_alloc);
    xfree(te->clar_map);
  }
  te->clar_map_size = new_size;
  te->clar_map_alloc = new_alloc;
  te->clar_map = new_map;
}

int
team_extra_set_clar_status(team_extra_state_t state, int user_id, int clar_id)
{
  struct team_extra *te;

  ASSERT(user_id > 0 && user_id <= EJ_MAX_USER_ID);
  ASSERT(clar_id >= 0 && clar_id <= EJ_MAX_CLAR_ID);

  if (user_id >= state->team_map_size) extend_team_map(state, user_id);
  te = get_entry(state, user_id, 0);
  if (te == (struct team_extra*) -1) return -1;
  ASSERT(te->user_id == user_id);
  if (clar_id >= te->clar_map_size) team_extra_extend_clar_map(te, clar_id);
  if ((te->clar_map[clar_id / BPE] & (1UL << clar_id % BPE)))
    return 1;
  te->clar_map[clar_id / BPE] |= (1UL << clar_id % BPE);
  te->is_dirty = 1;
  return 0;
}

void
team_extra_flush(team_extra_state_t state)
{
  int i;
  path_t wpath;
  FILE *f;

  for (i = 1; i < state->team_map_size; i++) {
    if (!state->team_map[i]) continue;
    if (state->team_map[i] == (struct team_extra*) -1) continue;
    ASSERT(state->team_map[i]->user_id == i);
    if (!state->team_map[i]->is_dirty) continue;
    if (make_write_path(state, wpath, sizeof(wpath), i) < 0) continue;
    if (!(f = fopen(wpath, "w"))) {
      unlink(wpath);
      continue;
    }
    team_extra_unparse_xml(f, state->team_map[i]);
    fclose(f);
    state->team_map[i]->is_dirty = 0;
  }
}

int
team_extra_append_warning(
        team_extra_state_t state,
        int user_id,
        int issuer_id,
        const ej_ip_t *issuer_ip,
        time_t issue_date,
        const unsigned char *txt,
        const unsigned char *cmt)
{
  struct team_extra *te;
  struct team_warning *cur_warn;

  ASSERT(user_id > 0 && user_id <= EJ_MAX_USER_ID);

  if (user_id >= state->team_map_size) extend_team_map(state, user_id);
  te = get_entry(state, user_id, 0);
  if (te == (struct team_extra*) -1) return -1;
  ASSERT(te->user_id == user_id);

  if (te->warn_u == te->warn_a) {
    te->warn_a *= 2;
    if (!te->warn_a) te->warn_a = 8;
    XREALLOC(te->warns, te->warn_a);
  }
  XCALLOC(cur_warn, 1);
  te->warns[te->warn_u++] = cur_warn;

  cur_warn->date = issue_date;
  cur_warn->issuer_id = issuer_id;
  cur_warn->issuer_ip = *issuer_ip;
  cur_warn->text = xstrdup(txt);
  cur_warn->comment = xstrdup(cmt);

  te->is_dirty = 1;
  return 0;
}

int
team_extra_set_status(team_extra_state_t state, int user_id, int status)
{
  struct team_extra *te;

  ASSERT(user_id > 0 && user_id <= EJ_MAX_USER_ID);

  if (user_id >= state->team_map_size) extend_team_map(state, user_id);
  te = get_entry(state, user_id, 0);
  if (te == (struct team_extra*) -1) return -1;
  ASSERT(te->user_id == user_id);

  if (te->status == status) return 0;
  te->status = status;
  te->is_dirty = 1;
  return 1;
}

int
team_extra_set_disq_comment(team_extra_state_t state, int user_id,
                            const unsigned char *disq_comment)
{
  struct team_extra *te;

  ASSERT(user_id > 0 && user_id <= EJ_MAX_USER_ID);

  if (user_id >= state->team_map_size) extend_team_map(state, user_id);
  te = get_entry(state, user_id, 0);
  if (te == (struct team_extra*) -1) return -1;
  ASSERT(te->user_id == user_id);

  xfree(te->disq_comment);
  te->disq_comment = xstrdup(disq_comment);
  te->is_dirty = 1;
  return 1;
}

int
team_extra_get_run_fields(team_extra_state_t state, int user_id)
{
  struct team_extra *te;

  ASSERT(user_id > 0 && user_id <= EJ_MAX_USER_ID);

  if (user_id >= state->team_map_size) extend_team_map(state, user_id);
  te = get_entry(state, user_id, 1);
  if (!te || te == (struct team_extra*) -1) return 0;
  ASSERT(te->user_id == user_id);
  return te->run_fields;
}

int
team_extra_set_run_fields(team_extra_state_t state, int user_id, int run_fields)
{
  struct team_extra *te;

  ASSERT(user_id > 0 && user_id <= EJ_MAX_USER_ID);

  if (user_id >= state->team_map_size) extend_team_map(state, user_id);
  te = get_entry(state, user_id, 0);
  if (te == (struct team_extra*) -1) return -1;
  ASSERT(te->user_id == user_id);

  if (te->run_fields == run_fields) return 0;
  te->run_fields = run_fields;
  te->is_dirty = 1;
  return 1;
}

extern struct xuser_plugin_iface plugin_xuser_file;
struct xuser_cnts_state *
team_extra_open(
        const struct ejudge_cfg *config,
        const struct contest_desc *cnts,
        const struct section_global_data *global,
        const unsigned char *plugin_name,
        int flags)
{
  const struct common_loaded_plugin *loaded_plugin = NULL;
  const struct xuser_plugin_iface *iface = NULL;

  if (!plugin_register_builtin(&plugin_xuser_file.b, config)) {
    err("cannot register default plugin");
    return NULL;
  }

  if (!plugin_name) {
    if (global) plugin_name = global->xuser_plugin;
  }
  if (!plugin_name) plugin_name = "";

  if (!plugin_name[0] || !strcmp(plugin_name, "file")) {
    if (!(loaded_plugin = plugin_get("xuser", "file"))) {
      err("cannot load default plugin");
      return NULL;
    }
    iface = (struct xuser_plugin_iface*) loaded_plugin->iface;
    return iface->open(loaded_plugin->data, config, cnts, global, flags);
  }

  if ((loaded_plugin = plugin_get("xuser", plugin_name))) {
    iface = (struct xuser_plugin_iface*) loaded_plugin->iface;
    return iface->open(loaded_plugin->data, config, cnts, global, flags);
  }

  if (!config) {
    err("cannot load any plugin");
    return NULL;
  }

  const struct xml_tree *p = NULL;
  const struct ejudge_plugin *plg = NULL;
  for (p = config->plugin_list; p; p = p->right) {
    plg = (const struct ejudge_plugin*) p;
    if (plg->load_flag && !strcmp(plg->type, "xuser")
        && !strcmp(plg->name, plugin_name))
      break;
  }
  if (!p || !plg) {
    err("xuser plugin '%s' is not registered", plugin_name);
    return NULL;
  }

  loaded_plugin = plugin_load_external(plg->path, plg->type, plg->name, config);
  if (!loaded_plugin) {
    err("cannot load plugin %s, %s", plg->type, plg->name);
    return NULL;
  }
  iface = (struct xuser_plugin_iface*) loaded_plugin->iface;
  return iface->open(loaded_plugin->data, config, cnts, global, flags);
}
