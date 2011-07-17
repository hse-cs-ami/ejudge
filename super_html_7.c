/* -*- mode: c -*- */
/* $Id$ */

/* Copyright (C) 2011 Alexander Chernov <cher@ejudge.ru> */

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

#include "config.h"
#include "version.h"
#include "ej_limits.h"

#include "super-serve.h"
#include "super_proto.h"
#include "super_html.h"
#include "ejudge_cfg.h"
#include "contests.h"
#include "mischtml.h"
#include "xml_utils.h"
#include "serve_state.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ARMOR(s)  html_armor_buf(&ab, (s))
#define FAIL(c) do { retval = -(c); goto cleanup; } while (0)

void
super_html_7_force_link()
{
}

static int
get_full_caps(const struct super_http_request_info *phr, const struct contest_desc *cnts, opcap_t *pcap)
{
  opcap_t caps1 = 0, caps2 = 0;

  opcaps_find(&phr->config->capabilities, phr->login, &caps1);
  opcaps_find(&cnts->capabilities, phr->login, &caps2);
  *pcap = caps1 | caps2;
  return 0;
}



static int
check_other_editors(
        FILE *log_f,
        FILE *out_f,
        struct super_http_request_info *phr,
        int contest_id,
        const struct contest_desc *cnts)
{
  unsigned char buf[1024];
  unsigned char hbuf[1024];
  const unsigned char *cl = " class=\"b0\"";
  time_t current_time = time(0);
  serve_state_t cs = NULL;
  struct stat stb;

  // check if this contest is already edited by anybody else
  const struct sid_state *other_session = super_serve_sid_state_get_cnts_editor(contest_id);

  if (other_session == phr->ss) {
    snprintf(buf, sizeof(buf), "serve-control: %s, the contest is being edited by you",
             phr->html_name);
    ss_write_html_header(out_f, phr, buf, 0, NULL);
    fprintf(out_f, "<h1>%s</h1>\n", buf);

    fprintf(out_f, "<ul>");
    fprintf(out_f, "<li>%s%s</a></li>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                          NULL, NULL),
            "Main page");
    fprintf(out_f, "</ul>\n");

    fprintf(out_f, "<p>To edit the tests you should finish editing the contest settings.</p>\n");

    ss_write_html_footer(out_f);
    return 0;
  }

  if (other_session) {
    snprintf(buf, sizeof(buf), "serve-control: %s, the contest is being edited by someone else",
             phr->html_name);
    ss_write_html_header(out_f, phr, buf, 0, NULL);
    fprintf(out_f, "<h1>%s</h1>\n", buf);

    fprintf(out_f, "<ul>");
    fprintf(out_f, "<li>%s%s</a></li>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                          NULL, NULL),
            "Main page");
    fprintf(out_f, "</ul>\n");

    fprintf(out_f, "<p>This contest is being edited by another user or in another session.</p>");
    fprintf(out_f, "<table%s><tr><td%s>%s</td><td%s>%016llx</td></tr>",
            cl, cl, "Session", cl, other_session->sid);
    fprintf(out_f, "<tr><td%s>%s</td><td%s>%s</td></tr>",
            cl, "IP address", cl, xml_unparse_ip(other_session->remote_addr));
    fprintf(out_f, "<tr><td%s>%s</td><td%s>%s</td></tr></table>\n",
            cl, "User login", cl, other_session->user_login);
    ss_write_html_footer(out_f);
    return 0;
  }

  other_session = super_serve_sid_state_get_test_editor(contest_id);
  if (other_session && other_session != phr->ss) {
    snprintf(buf, sizeof(buf), "serve-control: %s, the tests are being edited by someone else",
             phr->html_name);
    ss_write_html_header(out_f, phr, buf, 0, NULL);
    fprintf(out_f, "<h1>%s</h1>\n", buf);

    fprintf(out_f, "<ul>");
    fprintf(out_f, "<li>%s%s</a></li>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                          NULL, NULL),
            "Main page");
    fprintf(out_f, "</ul>\n");

    fprintf(out_f, "<p>This tests are being edited by another user or in another session.</p>");
    fprintf(out_f, "<table%s><tr><td%s>%s</td><td%s>%016llx</td></tr>",
            cl, cl, "Session", cl, other_session->sid);
    fprintf(out_f, "<tr><td%s>%s</td><td%s>%s</td></tr>",
            cl, "IP address", cl, xml_unparse_ip(other_session->remote_addr));
    fprintf(out_f, "<tr><td%s>%s</td><td%s>%s</td></tr></table>\n",
            cl, "User login", cl, other_session->user_login);
    ss_write_html_footer(out_f);
    return 0;
  }

  if ((cs = phr->ss->te_state) && cs->last_timestamp > 0 && cs->last_check_time + 10 >= current_time) {
    return 1;
  }

  if (cs && cs->last_timestamp > 0) {
    if (!cs->config_path) goto invalid_serve_cfg;
    if (stat(cs->config_path, &stb) < 0) goto invalid_serve_cfg;
    if (!S_ISREG(stb.st_mode)) goto invalid_serve_cfg;
    if (stb.st_mtime == cs->last_timestamp) {
      cs->last_check_time = current_time;
      return 1;
    }
  }

  phr->ss->te_state = serve_state_destroy(cs, cnts, NULL);

  if (serve_state_load_contest_config(phr->config, contest_id, cnts, &phr->ss->te_state) < 0)
    goto invalid_serve_cfg;

  if (!cs->config_path) goto invalid_serve_cfg;
  if (stat(cs->config_path, &stb) < 0) goto invalid_serve_cfg;
  if (!S_ISREG(stb.st_mode)) goto invalid_serve_cfg;
  cs->last_timestamp = stb.st_mtime;
  cs->last_check_time = current_time;

  return 1;

invalid_serve_cfg:
  phr->ss->te_state = serve_state_destroy(cs, cnts, NULL);
  return -S_ERR_INV_SERVE_CONFIG_PATH;
}

int
super_serve_op_TESTS_MAIN_PAGE(
        FILE *log_f,
        FILE *out_f,
        struct super_http_request_info *phr)
{
  int retval = 0;
  int contest_id = 0;
  const struct contest_desc *cnts = NULL;
  opcap_t caps = 0;

  ss_cgi_param_int_opt(phr, "contest_id", &contest_id, 0);
  if (contest_id <= 0) FAIL(S_ERR_INV_CONTEST);
  if (contests_get(contest_id, &cnts) < 0 || !cnts) FAIL(S_ERR_INV_CONTEST);

  if (phr->priv_level < PRIV_LEVEL_JUDGE) FAIL(S_ERR_PERM_DENIED);
  get_full_caps(phr, cnts, &caps);
  if (opcaps_check(caps, OPCAP_CONTROL_CONTEST) < 0) FAIL(S_ERR_PERM_DENIED);

  retval = check_other_editors(log_f, out_f, phr, contest_id, cnts);
  if (retval <= 0) goto cleanup;
  retval = 0;

cleanup:
  return retval;
}
