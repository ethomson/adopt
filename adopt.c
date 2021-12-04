/*
 * Copyright (c), Edward Thomson <ethomson@edwardthomson.com>
 * All rights reserved.
 *
 * This file is part of adopt, distributed under the MIT license.
 * For full terms and conditions, see the included LICENSE file.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "adopt.h"

#ifdef _WIN32
# include <Windows.h>
#else
# include <fcntl.h>
# include <sys/ioctl.h>
#endif

#ifdef _MSC_VER
# define INLINE(type) static __inline type
#else
# define INLINE(type) static inline type
#endif

#define spec_is_named_type(x) \
	((x)->type == ADOPT_BOOL || \
	 (x)->type == ADOPT_SWITCH || \
	 (x)->type == ADOPT_VALUE || \
	 (x)->type == ADOPT_VALUE_OPTIONAL)

INLINE(const adopt_spec *) spec_byname(
	adopt_parser *parser, const char *name, size_t namelen)
{
	const adopt_spec *spec;

	for (spec = parser->specs; spec->type; ++spec) {
		if (spec->type == ADOPT_LITERAL && namelen == 0)
			return spec;

		if (spec_is_named_type(spec) &&
			spec->name &&
			strlen(spec->name) == namelen &&
			strncmp(name, spec->name, namelen) == 0)
			return spec;
	}

	return NULL;
}

INLINE(const adopt_spec *) spec_byalias(adopt_parser *parser, char alias)
{
	const adopt_spec *spec;

	for (spec = parser->specs; spec->type; ++spec) {
		if (spec_is_named_type(spec) && alias == spec->alias)
			return spec;
	}

	return NULL;
}

INLINE(const adopt_spec *) spec_nextarg(adopt_parser *parser)
{
	const adopt_spec *spec;
	size_t args = 0;

	for (spec = parser->specs; spec->type; ++spec) {
		if (spec->type == ADOPT_ARG) {
			if (args == parser->arg_idx) {
				parser->arg_idx++;
				return spec;
			}

			args++;
		}

		if (spec->type == ADOPT_ARGS && args == parser->arg_idx)
			return spec;
	}

	return NULL;
}

static adopt_status_t parse_long(adopt_opt *opt, adopt_parser *parser)
{
	const adopt_spec *spec;
	char *arg = parser->args[parser->idx++], *name = arg + 2, *eql;
	size_t namelen;

	namelen = (eql = strrchr(arg, '=')) ? (size_t)(eql - name) : strlen(name);

	opt->arg = arg;

	if ((spec = spec_byname(parser, name, namelen)) == NULL) {
		opt->spec = NULL;
		opt->status = ADOPT_STATUS_UNKNOWN_OPTION;
		goto done;
	}

	opt->spec = spec;

	/* Future options parsed as literal */
	if (spec->type == ADOPT_LITERAL)
		parser->in_literal = 1;

	if (spec->type == ADOPT_BOOL && spec->value)
		*((int *)spec->value) = 1;

	if (spec->type == ADOPT_SWITCH && spec->value)
		*((int *)spec->value) = spec->switch_value;

	/* Parse values as "--foo=bar" or "--foo bar" */
	if (spec->type == ADOPT_VALUE || spec->type == ADOPT_VALUE_OPTIONAL) {
		if (eql && *(eql+1))
			opt->value = eql + 1;
		else if ((parser->idx + 1) <= parser->args_len)
			opt->value = parser->args[parser->idx++];

		if (spec->value)
			*((char **)spec->value) = opt->value;
	}

	/* Required argument was not provided */
	if (spec->type == ADOPT_VALUE && !opt->value)
		opt->status = ADOPT_STATUS_MISSING_VALUE;
	else
		opt->status = ADOPT_STATUS_OK;

done:
	return opt->status;
}

static adopt_status_t parse_short(adopt_opt *opt, adopt_parser *parser)
{
	const adopt_spec *spec;
	char *arg = parser->args[parser->idx++], alias = *(arg + 1);

	opt->arg = arg;

	if ((spec = spec_byalias(parser, alias)) == NULL) {
		opt->spec = NULL;
		opt->status = ADOPT_STATUS_UNKNOWN_OPTION;
		goto done;
	}

	opt->spec = spec;

	if (spec->type == ADOPT_BOOL && spec->value)
		*((int *)spec->value) = 1;

	if (spec->type == ADOPT_SWITCH && spec->value)
		*((int *)spec->value) = spec->switch_value;

	/* Parse values as "-ifoo" or "-i foo" */
	if (spec->type == ADOPT_VALUE || spec->type == ADOPT_VALUE_OPTIONAL) {
		if (strlen(arg) > 2)
			opt->value = arg + 2;
		else if ((parser->idx + 1) <= parser->args_len)
			opt->value = parser->args[parser->idx++];

		if (spec->value)
			*((char **)spec->value) = opt->value;
	}

	/* Required argument was not provided */
	if (spec->type == ADOPT_VALUE && !opt->value)
		opt->status = ADOPT_STATUS_MISSING_VALUE;
	else
		opt->status = ADOPT_STATUS_OK;

done:
	return opt->status;
}

static adopt_status_t parse_arg(adopt_opt *opt, adopt_parser *parser)
{
	const adopt_spec *spec = spec_nextarg(parser);

	opt->spec = spec;
	opt->arg = parser->args[parser->idx];

	if (!spec) {
		parser->idx++;
		opt->status = ADOPT_STATUS_UNKNOWN_OPTION;
	} else if (spec->type == ADOPT_ARGS) {
		if (spec->value)
			*((char ***)spec->value) = &parser->args[parser->idx];

		/* Args consume all the remaining arguments. */
		parser->in_args = (parser->args_len - parser->idx);
		parser->idx = parser->args_len;
		opt->args_len = parser->in_args;
		opt->status = ADOPT_STATUS_OK;
	} else {
		if (spec->value)
			*((char **)spec->value) = parser->args[parser->idx];

		parser->idx++;
		opt->status = ADOPT_STATUS_OK;
	}

	return opt->status;
}

void adopt_parser_init(
	adopt_parser *parser,
	const adopt_spec specs[],
	char **args,
	size_t args_len)
{
	assert(parser);

	memset(parser, 0x0, sizeof(adopt_parser));

	parser->specs = specs;
	parser->args = args;
	parser->args_len = args_len;
}

adopt_status_t adopt_parser_next(adopt_opt *opt, adopt_parser *parser)
{
	assert(opt && parser);

	memset(opt, 0x0, sizeof(adopt_opt));

	if (parser->idx >= parser->args_len) {
		opt->args_len = parser->in_args;
		return ADOPT_STATUS_DONE;
	}

	/* Handle arguments in long form, those beginning with "--" */
	if (strncmp(parser->args[parser->idx], "--", 2) == 0 &&
		!parser->in_literal)
		return parse_long(opt, parser);

	/* Handle arguments in short form, those beginning with "-" */
	else if (strncmp(parser->args[parser->idx], "-", 1) == 0 &&
		!parser->in_literal)
		return parse_short(opt, parser);

	/* Handle "free" arguments, those without a dash */
	else
		return parse_arg(opt, parser);
}

INLINE(int) spec_included(const adopt_spec **specs, const adopt_spec *spec)
{
	const adopt_spec **i;

	for (i = specs; *i; ++i) {
		if (spec == *i)
			return 1;
	}

	return 0;
}

INLINE(int) spec_is_choice(const adopt_spec *spec)
{
	return ((spec + 1)->type &&
	       ((spec + 1)->usage & ADOPT_USAGE_CHOICE));
}

static adopt_status_t validate_required(
	adopt_opt *opt,
	const adopt_spec specs[],
	const adopt_spec **given_specs)
{
	const adopt_spec *spec, *required;
	int given;

	/*
	 * Iterate over the possible specs to identify requirements and
	 * ensure that those have been given on the command-line.
	 * Note that we can have required *choices*, where one in a
	 * list of choices must be specified.
	 */
	for (spec = specs, required = NULL, given = 0; spec->type; ++spec) {
		if (!required && (spec->usage & ADOPT_USAGE_REQUIRED)) {
			required = spec;
			given = 0;
		} else if (!required) {
			continue;
		}

		if (!given)
			given = spec_included(given_specs, spec);

		/*
		 * Validate the requirement unless we're in a required
		 * choice.  In that case, keep the required state and
		 * validate at the end of the choice list.
		 */
		if (!spec_is_choice(spec)) {
			if (!given) {
				opt->spec = required;
				opt->status = ADOPT_STATUS_MISSING_ARGUMENT;
				break;
			}

			required = NULL;
			given = 0;
		}
	}

	return opt->status;
}

adopt_status_t adopt_parse(
	adopt_opt *opt,
	const adopt_spec specs[],
	char **args,
	size_t args_len)
{
	adopt_parser parser;
	const adopt_spec **given_specs;
	size_t given_idx = 0;

	adopt_parser_init(&parser, specs, args, args_len);

	given_specs = alloca(sizeof(const adopt_spec *) * (args_len + 1));

	while (adopt_parser_next(opt, &parser)) {
		if (opt->status != ADOPT_STATUS_OK &&
		    opt->status != ADOPT_STATUS_DONE)
			return opt->status;

		given_specs[given_idx++] = opt->spec;
	}

	given_specs[given_idx] = NULL;

	return validate_required(opt, specs, given_specs);
}

static int spec_name_fprint(FILE *file, const adopt_spec *spec)
{
	int error;

	if (spec->type == ADOPT_ARG)
		error = fprintf(file, "%s", spec->value_name);
	else if (spec->type == ADOPT_ARGS)
		error = fprintf(file, "%s", spec->value_name);
	else if (spec->alias && !(spec->usage & ADOPT_USAGE_SHOW_LONG))
		error = fprintf(file, "-%c", spec->alias);
	else
		error = fprintf(file, "--%s", spec->name);

	return error;
}

int adopt_status_fprint(
	FILE *file,
	const char *command,
	const adopt_opt *opt)
{
	const adopt_spec *choice;
	int error;

	if (command && (error = fprintf(file, "%s: ", command)) < 0)
		return error;

	switch (opt->status) {
	case ADOPT_STATUS_DONE:
		error = fprintf(file, "finished processing arguments (no error)\n");
		break;
	case ADOPT_STATUS_OK:
		error = fprintf(file, "no error\n");
		break;
	case ADOPT_STATUS_UNKNOWN_OPTION:
		error = fprintf(file, "unknown option: %s\n", opt->arg);
		break;
	case ADOPT_STATUS_MISSING_VALUE:
		if ((error = fprintf(file, "argument '")) < 0 ||
		    (error = spec_name_fprint(file, opt->spec)) < 0 ||
		    (error = fprintf(file, "' requires a value.\n")) < 0)
			;
		break;
	case ADOPT_STATUS_MISSING_ARGUMENT:
		if (spec_is_choice(opt->spec)) {
			int is_choice = 1;

			if (spec_is_choice((opt->spec)+1))
				error = fprintf(file, "one of");
			else
				error = fprintf(file, "either");

			if (error < 0)
				break;

			for (choice = opt->spec; is_choice; ++choice) {
				is_choice = spec_is_choice(choice);

				if (!is_choice)
					error = fprintf(file, " or");
				else if (choice != opt->spec)
					error = fprintf(file, ",");

				if ((error < 0) ||
				    (error = fprintf(file, " '")) < 0 ||
				    (error = spec_name_fprint(file, choice)) < 0 ||
				    (error = fprintf(file, "'")) < 0)
					break;

				if (!spec_is_choice(choice))
					break;
			}

			if ((error < 0) ||
			    (error = fprintf(file, " is required.\n")) < 0)
				break;
		} else {
			if ((error = fprintf(file, "argument '")) < 0 ||
			    (error = spec_name_fprint(file, opt->spec)) < 0 ||
			    (error = fprintf(file, "' is required.\n")) < 0)
				break;
		}

		break;
	default:
		error = fprintf(file, "Unknown status: %d\n", opt->status);
		break;
	}

	return error;
}

int adopt_usage_fprint(
	FILE *file,
	const char *command,
	const adopt_spec specs[])
{
	const adopt_spec *spec;
	int choice = 0, next_choice = 0, optional = 0;
	int error;

	if ((error = fprintf(file, "usage: %s", command)) < 0)
		goto done;

	for (spec = specs; spec->type; ++spec) {
		if (!choice)
			optional = !(spec->usage & ADOPT_USAGE_REQUIRED);

		next_choice = !!((spec + 1)->usage & ADOPT_USAGE_CHOICE);

		if (spec->usage & ADOPT_USAGE_HIDDEN)
			continue;

		if (choice)
			error = fprintf(file, "|");
		else
			error = fprintf(file, " ");

		if (error < 0)
			goto done;

		if (optional && !choice && (error = fprintf(file, "[")) < 0)
			error = fprintf(file, "[");
		if (!optional && !choice && next_choice)
			error = fprintf(file, "(");

		if (error < 0)
			goto done;

		if (spec->type == ADOPT_VALUE && spec->alias && !(spec->usage & ADOPT_USAGE_SHOW_LONG))
			error = fprintf(file, "-%c <%s>", spec->alias, spec->value_name);
		else if (spec->type == ADOPT_VALUE)
			error = fprintf(file, "--%s=<%s>", spec->name, spec->value_name);
		else if (spec->type == ADOPT_VALUE_OPTIONAL && spec->alias)
			error = fprintf(file, "-%c [<%s>]", spec->alias, spec->value_name);
		else if (spec->type == ADOPT_VALUE_OPTIONAL)
			error = fprintf(file, "--%s[=<%s>]", spec->name, spec->value_name);
		else if (spec->type == ADOPT_ARG)
			error = fprintf(file, "<%s>", spec->value_name);
		else if (spec->type == ADOPT_ARGS)
			error = fprintf(file, "<%s>...", spec->value_name);
		else if (spec->type == ADOPT_LITERAL)
			error = fprintf(file, "--");
		else if (spec->alias && !(spec->usage & ADOPT_USAGE_SHOW_LONG))
			error = fprintf(file, "-%c", spec->alias);
		else
			error = fprintf(file, "--%s", spec->name);

		if (error < 0)
			goto done;

		if (!optional && choice && !next_choice)
			error = fprintf(file, ")");
		else if (optional && !next_choice)
			error = fprintf(file, "]");

		if (error < 0)
			goto done;

		choice = next_choice;
	}

	error = fprintf(file, "\n");

done:
	error = (error < 0) ? -1 : 0;
	return error;
}

