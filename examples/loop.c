/*
 * Copyright (c), Edward Thomson <ethomson@edwardthomson.com>
 * All rights reserved.
 *
 * This file is part of adopt, distributed under the MIT license.
 * For full terms and conditions, see the included LICENSE file.
 */

#include <stdio.h>

#include "adopt.h"

static int verbose = 0;
static int volume = 1;
static char *channel = "default";
static char *filename1 = NULL;
static char *filename2 = NULL;
static char **other = NULL;

adopt_spec opt_specs[] = {
	{ ADOPT_TYPE_BOOL, "verbose", 'v', &verbose, 0,
	  0, NULL, "Turn on verbose information" },
	{ ADOPT_TYPE_SWITCH, "quiet", 'q', &volume, 0,
	  ADOPT_USAGE_REQUIRED, NULL, "Emit no output" },
	{ ADOPT_TYPE_SWITCH, "loud", 'l', &volume, 2,
	  ADOPT_USAGE_CHOICE, NULL, "Emit louder than usual output" },
	{ ADOPT_TYPE_VALUE, "channel", 'c', &channel, 0,
	  0, "channel", "Set the channel" },
	{ ADOPT_TYPE_LITERAL },
	{ ADOPT_TYPE_ARG, NULL, 0, &filename1, 0,
	  ADOPT_USAGE_REQUIRED, "file1", "The first filename" },
	{ ADOPT_TYPE_ARG, NULL, 0, &filename2, 0,
	  0, "file2", "The second (optional) filename" },
	{ ADOPT_TYPE_ARGS, NULL, 0, &other, 0,
	  0, "other", "The other (optional) arguments" },
	{ 0 }
};

static const char *volume_tostr(int volume)
{
	switch(volume) {
	case 0:
		return "quiet";
	case 1:
		return "normal";
	case 2:
		return "loud";
	}

	return "unknown";
}

int main(int argc, char **argv)
{
	adopt_parser parser;
	adopt_opt opt;
	size_t i;

	adopt_parser_init(&parser, opt_specs, argv + 1, argc - 1);

	while (adopt_parser_next(&opt, &parser)) {
		if (opt.status != ADOPT_STATUS_OK) {
			adopt_status_fprint(stderr, argv[0], &opt);
			adopt_usage_fprint(stderr, argv[0], opt_specs);
			return 129;
		}
	}

	if (!filename1) {
		fprintf(stderr, "filename is required\n");
		adopt_usage_fprint(stderr, argv[0], opt_specs);
		return 129;
	}

	printf("verbose: %d\n", verbose);
	printf("volume: %s\n", volume_tostr(volume));
	printf("channel: %s\n", channel ? channel : "(null)");
	printf("filename one: %s\n", filename1 ? filename1 : "(null)");
	printf("filename two: %s\n", filename2 ? filename2 : "(null)");

	/* display other args */
	if (other) {
		printf("other args [%d]: ", (int)opt.args_len);

		for (i = 0; i < opt.args_len; i++) {
			if (i)
				printf(", ");

			printf("%s", other[i]);
		}

		printf("\n");
	}
}

