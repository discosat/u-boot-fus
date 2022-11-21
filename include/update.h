/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * F&S update command
 */

#ifndef __UPDATE_H_
#define __UPDATE_H_	1

enum update_action {
	UPDATE_ACTION_NONE,
	UPDATE_ACTION_UPDATE,
	UPDATE_ACTION_INSTALL,
	UPDATE_ACTION_RECOVER
};
int update_script(enum update_action action_id, const char *autocheck,
		  const char *fname, unsigned long addr);

#endif /* __UPDATE_H_ */
