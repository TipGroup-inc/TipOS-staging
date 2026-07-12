#ifndef SHELL_CMDS_H
#define SHELL_CMDS_H

void cmd_help(void);
void cmd_clear(void);
void cmd_echo(const char *args);
void cmd_about(void);
void cmd_shutdown(void);
void cmd_reboot(void);
void cmd_ls(void);
void cmd_touch(const char *name);
void cmd_rm(const char *name);
void cmd_cat(const char *name);
void cmd_edit(const char *name);
void cmd_mkdir(const char *name);
void cmd_cd(const char *name);
void cmd_pwd(void);
void cmd_mv(const char *args);
void cmd_cp(const char *args);
void cmd_rmdir(const char *name);
void cmd_stat(const char *name);
void cmd_disp(void);
void cmd_exec(const char *name);
void cmd_date(void);
void cmd_uptime(void);
void cmd_cc(const char *args);
int  cmd_exec_in_dir(const char *name, const char *dir);

#endif
