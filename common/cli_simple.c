/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Add to readline cmdline-editing by
 * (C) Copyright 2005
 * JinHua Luo, GuangDong Linux Center, <luo.jinhua@gd-linux.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <bootretry.h>
#include <cli.h>
#include <linux/ctype.h>

#define DEBUG_PARSER	0	/* set to 1 to debug */

#define debug_parser(fmt, args...)		\
	debug_cond(DEBUG_PARSER, fmt, ##args)


int cli_simple_parse_line(char *line, char *argv[])
{
	int nargs = 0;

	debug_parser("%s: \"%s\"\n", __func__, line);
	while (nargs < CONFIG_SYS_MAXARGS) {
		/* skip any white space */
		while (isblank(*line))
			++line;

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
			debug_parser("%s: nargs=%d\n", __func__, nargs);
			return nargs;
		}

		argv[nargs++] = line;	/* begin of argument string	*/

		/* find end of string */
		while (*line && !isblank(*line))
			++line;

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
			debug_parser("parse_line: nargs=%d\n", nargs);
			return nargs;
		}

		*line++ = '\0';		/* terminate current arg	 */
	}

	printf("** Too many args (max. %d) **\n", CONFIG_SYS_MAXARGS);

	debug_parser("%s: nargs=%d\n", __func__, nargs);
	return nargs;
}

 /*
 * WARNING:
 *
 * We must create a temporary copy of the command since the command we get
 * may be the result from getenv(), which returns a pointer directly to
 * the environment data, which may change magicly when the command we run
 * creates or modifies environment variables (like "bootp" does).
 */
int cli_simple_run_command(const char *cmd, int flag)
{
	char cmdbuf[CONFIG_SYS_CBSIZE];	/* working copy of cmd		*/
	char output[CONFIG_SYS_CBSIZE];
	char *argv[CONFIG_SYS_MAXARGS + 1];	/* NULL terminated	*/
	int argc;
	int repeatable = 1;
	int rc = 0;
	char c;
	char delim;
	int varindex;
	char *input = cmdbuf;
	char *s;
	int out;
	int error;

	/* The parser uses a state machine to decide how a character is to be
	   interpreted. */
	enum parse_state {
		PS_WS,	      /* Ignoring whitespace at beginning of argument */
		PS_NORMAL,    /* Normal unquoted argument */
		PS_SQ,	      /* Within single quotes */
		PS_DQ,	      /* Within double quotes */
	} ps;

	debug_parser("[RUN_COMMAND] cmd[%p]=\"", cmd);
	if (DEBUG_PARSER) {
		/* use puts - string may be loooong */
		puts(cmd ? cmd : "NULL");
		puts("\"\n");
	}

	clear_ctrlc();		/* forget any previous Control C */

	if (!cmd || !*cmd) {
		return -1;	/* empty command */
	}

	if (strlen(cmd) >= CONFIG_SYS_CBSIZE) {
		puts ("## Command too long!\n");
		return -1;
	}

	strcpy (cmdbuf, cmd);

	/* Parse commands, check for invalid and repeatable commands and
	   execute command */

	c = *input++;
	do {
		/* New command */
		out = 0;
		error = 0;
		argc = 0;
		argv[argc] = output;
		ps = PS_WS;
		do {
			/* Command separator */
			if (c == ';') {
				if ((ps == PS_WS) || (ps == PS_NORMAL)) {
					c = *input++;
					break;
				}
				output[out++] = c;
			} else if (c == '\\') {
				c = *input++;
				if (!c) {
					output[out++] = '\\';
					break;
				}
				output[out++] = c;
			} else {
				switch (c) {
				case ' ':
				case '\t':
					if ((ps == PS_DQ) || (ps == PS_SQ))
						output[out++] = c;
					else if (ps == PS_NORMAL) {
						/* Start new argument */
						output[out++] = 0;
						argv[argc] = output + out;
						ps = PS_WS;
					}
					break;

				case '\'':
					if (ps == PS_DQ)
						output[out++] = c;
					else if (ps == PS_SQ)
						ps = PS_NORMAL;
					else {
						if (ps == PS_WS)
							argc++;
						ps = PS_SQ;
					}
					break;

				case '\"':
					if (ps == PS_SQ)
						output[out++] = c;
					else if (ps == PS_DQ)
						ps = PS_NORMAL;
					else {
						if (ps == PS_WS)
							argc++;
						ps = PS_DQ;
					}
					break;

				case '$':
					if (ps == PS_SQ) {
						output[out++] = c;
						break;
					}
					if (ps == PS_WS) {
						argc++;
						ps = PS_NORMAL;
					}
					/* Save start of variable */
					varindex = out;
					output[out++] = c;
					if (out >= CONFIG_SYS_CBSIZE)
						break;

					/* Check for opening brace */
					c = *input++;
					if (!c)
						break;

					if (c == '(')
						delim = ')';
					else if (c == '{')
						delim = '}';
					else {
						output[out++] = c;
						break;
					}

					/* Copy var name */
					do {
						output[out++] = c;
						c = *input++;
					} while (c && (c != delim)
						 && (out < CONFIG_SYS_CBSIZE));

					/* Unexpected end of command or no
					   more room in buffer */
					if (!c || (out >= CONFIG_SYS_CBSIZE))
						break;

					/* The variable name starts 2 chars
					   after the $( or ${ respectively;
					   read the environment variable */
					output[out] = 0;
					s = getenv(output + varindex + 2);
#if DEBUG_PARSER
					printf("[$(%s)='",
					       output + varindex + 2);
					puts(s);
					puts("']\n");
#endif

					/* Resume to start of variable */
					out = varindex;

					/* Copy var content to output */
					if (s) {
						do {
							char cc;
							cc = *s++;
							if (!cc)
								break;
							output[out++] = cc;
						} while (out < CONFIG_SYS_CBSIZE);
					}
					break;

				default:  /* Any other character */
					output[out++] = c;
					if (ps == PS_WS) {
						argc++;
						ps = PS_NORMAL;
					}
					break;
				}
			}
			if (!c)
				break;
			/* Make sure that we don't exceed the allowed number
			   of arguments */
			if (argc >= CONFIG_SYS_MAXARGS) {
				argc--;
				error |= 1; /* Remember as error */
			}

			/* If string is too long, store all remaining
			   characters in the last character; this will give a
			   strange result but as we don't execute this command
			   anyway, it does not matter. By doing this here, we
			   don't need to check for enough space in the buffer
			   in the loop above and we also automatically find
			   the end of the current command. */
			if (out >= CONFIG_SYS_CBSIZE) {
				out--;
				error |= 2; /* Remember as error */
			}

			/* Get next character */
			c = *input++;
		} while (c);

		/* Write final 0 */
		output[out] = 0;

#if DEBUG_PARSER
		{
			int i;

			for (i=0; i<argc; i++)
				printf("[argv[%d]='%s']\n", i, argv[i]);
		}
#endif

		/* argv is NULL terminated */
		argv[argc] = NULL;

		/* Check for some errors that may have appeared while parsing
		   the command */
		if (error) {
			char *pReason;
			if (error & 1)
				pReason = "Too many command arguments!\n";
			else
				pReason = "Expanded command too long!\n";
			puts(pReason);
			rc = -1;
			continue;
		}

		/* Do we have an empty command? */
		if (!argc) {
			rc = -1;
			continue;
		}

		/* Find and execute the command */
		if (cmd_process(flag, argc, argv, &repeatable, NULL))
			rc = -1;

		/* Did the user stop this? */
		if (had_ctrlc ())
			return -1;	/* if stopped then not repeatable */
	} while (c);

	return rc ? rc : repeatable;
}

void cli_simple_loop(void)
{
	static char lastcommand[CONFIG_SYS_CBSIZE] = { 0, };

	int len;
	int flag;
	int rc = 1;

	for (;;) {
		if (rc >= 0) {
			/* Saw enough of a valid command to
			 * restart the timeout.
			 */
			bootretry_reset_cmd_timeout();
		}
		len = cli_readline(get_sys_prompt());

		flag = 0;	/* assume no special flags for now */
		if (len > 0)
			strcpy(lastcommand, console_buffer);
		else if (len == 0)
			flag |= CMD_FLAG_REPEAT;
#ifdef CONFIG_BOOT_RETRY_TIME
		else if (len == -2) {
			/* -2 means timed out, retry autoboot
			 */
			puts("\nTimed out waiting for command\n");
# ifdef CONFIG_RESET_TO_RETRY
			/* Reinit board to run initialization code again */
			do_reset(NULL, 0, 0, NULL);
# else
			return;		/* retry autoboot */
# endif
		}
#endif

		if (len == -1)
			puts("<INTERRUPT>\n");
		else
			rc = run_command_repeatable(lastcommand, flag);

		if (rc <= 0) {
			/* invalid command or not repeatable, forget it */
			lastcommand[0] = 0;
		}
	}
}

int cli_simple_run_command_list(char *cmd, int flag)
{
	char *line, *next;
	int rcode = 0;

	/*
	 * Break into individual lines, and execute each line; terminate on
	 * error.
	 */
	next = cmd;
	line = cmd;
	while (*next) {
		if (*next == '\n') {
			*next = '\0';
			/* run only non-empty commands */
			if (*line) {
				debug("** exec: \"%s\"\n", line);
				if (cli_simple_run_command(line, 0) < 0) {
					rcode = 1;
					break;
				}
			}
			line = next + 1;
		}
		++next;
	}
	if (rcode == 0 && *line)
		rcode = (cli_simple_run_command(line, 0) < 0);

	return rcode;
}
