#include <string.h>
#include "module.h"
#include "misc.h"
#include "shared.h"

/*
 * the code in misc.c expects this to be a global variable. Yes, that's
 * weird, and no I'm not gonna do anything about it right now. it is a
 * global variable in Nagios and the module is the designated user of
 * that functionality, so it's not, strictly speaking, necessary.
 */
char *config_file = NULL;

#define CMD_HASH 1
#define CMD_LAST_CHANGE 2
#define CMD_FILES 3
int main(int argc, char **argv)
{
	unsigned char hash[20];
	int cmd = 0;
	unsigned int i;
	char *base, *cmd_string = NULL;

	base = strrchr(argv[0], '/');
	if (!base)
		base = argv[0];
	else
		base++;

	if (!prefixcmp(base, "oconf.")) {
		cmd_string = base + strlen("oconf.");
		if (!strcmp(cmd_string, "changed")) {
			cmd = CMD_LAST_CHANGE;
		} else if (!strcmp(cmd_string, "hash")) {
			cmd = CMD_HASH;
		}
	}

	for (i = 1; i < (unsigned int)argc; i++) {
		char *arg = argv[i];
		if (!prefixcmp(arg, "--nagios-cfg=")) {
			config_file = &arg[strlen("--nagios-cfg=")];
			continue;
		}
		while (*arg == '-')
			arg++;

		if (!prefixcmp(arg, "last") || !prefixcmp(arg, "change")) {
			cmd = CMD_LAST_CHANGE;
			continue;
		}
		if (!strcmp(arg, "sha1") || !strcmp(arg, "hash")) {
			cmd = CMD_HASH;
			continue;
		}
		if (!prefixcmp(arg, "list") || !strcmp(arg, "files")) {
			cmd = CMD_FILES;
			continue;
		}
	}
	if (!config_file)
		config_file = "/opt/monitor/etc/nagios.cfg";

	switch (cmd) {
	case CMD_HASH:
		get_config_hash(hash);
		printf("%s\n", tohex(hash, 20));
		break;
	case CMD_LAST_CHANGE:
		printf("%lu\n", get_last_cfg_change());
		break;
	case CMD_FILES:
		{
			struct file_list **sorted_flist;
			unsigned int num_files;

			sorted_flist = get_sorted_oconf_files(&num_files);
			if(!sorted_flist)
				break;

			for (i = 0; i < num_files; i++) {
				printf("%s\n", sorted_flist[i]->name);
				sorted_flist[i]->next = NULL;
				file_list_free(sorted_flist[i]);
			}
			free(sorted_flist);
		}
		break;
	}
	return 0;
}
