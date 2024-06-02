// This is an auto-generated file, do not edit

#ifndef __SUPER_SERVE_META_H__
#define __SUPER_SERVE_META_H__

#include <stdlib.h>

enum
{
  SSSS_next = 1,
  SSSS_prev,
  SSSS_sid,
  SSSS_remote_addr,
  SSSS_init_time,
  SSSS_flags,
  SSSS_edited_cnts,
  SSSS_user_id,
  SSSS_user_login,
  SSSS_user_name,
  SSSS_edit_page,
  SSSS_users_header_text,
  SSSS_users_footer_text,
  SSSS_register_header_text,
  SSSS_register_footer_text,
  SSSS_team_header_text,
  SSSS_team_menu_1_text,
  SSSS_team_menu_2_text,
  SSSS_team_menu_3_text,
  SSSS_team_separator_text,
  SSSS_team_footer_text,
  SSSS_priv_header_text,
  SSSS_priv_footer_text,
  SSSS_register_email_text,
  SSSS_copyright_text,
  SSSS_welcome_text,
  SSSS_reg_welcome_text,
  SSSS_users_header_loaded,
  SSSS_users_footer_loaded,
  SSSS_register_header_loaded,
  SSSS_register_footer_loaded,
  SSSS_team_header_loaded,
  SSSS_team_menu_1_loaded,
  SSSS_team_menu_2_loaded,
  SSSS_team_menu_3_loaded,
  SSSS_team_separator_loaded,
  SSSS_team_footer_loaded,
  SSSS_priv_header_loaded,
  SSSS_priv_footer_loaded,
  SSSS_register_email_loaded,
  SSSS_copyright_loaded,
  SSSS_welcome_loaded,
  SSSS_reg_welcome_loaded,
  SSSS_serve_parse_errors,
  SSSS_cfg,
  SSSS_global,
  SSSS_aprob_u,
  SSSS_aprob_a,
  SSSS_aprobs,
  SSSS_aprob_flags,
  SSSS_prob_a,
  SSSS_probs,
  SSSS_prob_flags,
  SSSS_atester_total,
  SSSS_atesters,
  SSSS_tester_total,
  SSSS_testers,
  SSSS_enable_stand2,
  SSSS_enable_plog,
  SSSS_enable_extra_col,
  SSSS_disable_compilation_server,
  SSSS_enable_win32_languages,
  SSSS_lang_a,
  SSSS_langs,
  SSSS_loc_cs_map,
  SSSS_cs_loc_map,
  SSSS_lang_opts,
  SSSS_lang_libs,
  SSSS_lang_flags,
  SSSS_cscs,
  SSSS_cs_langs_loaded,
  SSSS_cs_lang_total,
  SSSS_cs_cfg,
  SSSS_cs_langs,
  SSSS_cs_lang_names,
  SSSS_extra_cs_cfgs_total,
  SSSS_extra_cs_cfgs,
  SSSS_serv_langs,
  SSSS_cur_lang,
  SSSS_cur_prob,
  SSSS_prob_show_adv,
  SSSS_contest_start_cmd_text,
  SSSS_contest_stop_cmd_text,
  SSSS_stand_header_text,
  SSSS_stand_footer_text,
  SSSS_stand2_header_text,
  SSSS_stand2_footer_text,
  SSSS_plog_header_text,
  SSSS_plog_footer_text,
  SSSS_compile_home_dir,
  SSSS_user_filter_set,
  SSSS_user_filter,
  SSSS_user_offset,
  SSSS_user_count,
  SSSS_group_filter_set,
  SSSS_group_filter,
  SSSS_group_offset,
  SSSS_group_count,
  SSSS_contest_user_filter_set,
  SSSS_contest_user_filter,
  SSSS_contest_user_offset,
  SSSS_contest_user_count,
  SSSS_group_user_filter_set,
  SSSS_group_user_filter,
  SSSS_group_user_offset,
  SSSS_group_user_count,
  SSSS_marked,
  SSSS_update_state,
  SSSS_te_state,

  SSSS_LAST_FIELD,
};

struct sid_state;

int ss_sid_state_get_type(int tag);
size_t ss_sid_state_get_size(int tag);
const char *ss_sid_state_get_name(int tag);
const void *ss_sid_state_get_ptr(const struct sid_state *ptr, int tag);
void *ss_sid_state_get_ptr_nc(struct sid_state *ptr, int tag);
int ss_sid_state_lookup_field(const char *name);
void ss_sid_state_copy(struct sid_state *dst, const struct sid_state *src);
void ss_sid_state_free(struct sid_state *ptr);

struct meta_methods;
extern const struct meta_methods ss_sid_state_methods;

#endif
