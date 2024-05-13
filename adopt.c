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
#include <limits.h>
#include <assert.h>

#if defined(__sun) || defined(__illumos__)
# include <alloca.h>
#endif

#include "adopt.h"

#ifdef _WIN32
# include <windows.h>
#else
# include <fcntl.h>
# include <sys/ioctl.h>
#endif

#ifdef _MSC_VER
# define INLINE(type) static __inline type
#else
# define INLINE(type) static inline type
#endif

#ifdef _MSC_VER
# define alloca _alloca
#endif

#define spec_is_option_type(x) \
	((x)->type == ADOPT_TYPE_BOOL || \
	 (x)->type == ADOPT_TYPE_SWITCH || \
	 (x)->type == ADOPT_TYPE_VALUE)

INLINE(const adopt_spec *) spec_for_long(
	int *is_negated,
	int *has_value,
	const char **value,
	const adopt_parser *parser,
	const char *arg)
{
	const adopt_spec *spec;
	char *eql;
	size_t eql_pos;

	eql = strchr(arg, '=');
	eql_pos = (eql = strchr(arg, '=')) ? (size_t)(eql - arg) : strlen(arg);

	for (spec = parser->specs; spec->type; ++spec) {
		/* Handle -- (everything after this is literal) */
		if (spec->type == ADOPT_TYPE_LITERAL && arg[0] == '\0')
			return spec;

		/* Handle --no-option arguments for bool types */
		if (spec->type == ADOPT_TYPE_BOOL &&
		    strncmp(arg, "no-", 3) == 0 &&
		    strcmp(arg + 3, spec->name) == 0) {
			*is_negated = 1;
			return spec;
		}

		/* Handle the typical --option arguments */
		if (spec_is_option_type(spec) &&
		    spec->name &&
		    strcmp(arg, spec->name) == 0)
			return spec;

		/* Handle --option=value arguments */
		if (spec->type == ADOPT_TYPE_VALUE &&
		    spec->name && eql &&
		    strncmp(arg, spec->name, eql_pos) == 0 &&
		    spec->name[eql_pos] == '\0') {
			*has_value = 1;
			*value = arg[eql_pos + 1] ? &arg[eql_pos + 1] : NULL;
			return spec;
		}
	}

	return NULL;
}

INLINE(const adopt_spec *) spec_for_short(
	const char **value,
	const adopt_parser *parser,
	const char *arg)
{
	const adopt_spec *spec;

	for (spec = parser->specs; spec->type; ++spec) {
		/* Handle -svalue short options with a value */
		if (spec->type == ADOPT_TYPE_VALUE &&
		    arg[0] == spec->alias &&
		    arg[1] != '\0') {
			*value = &arg[1];
			return spec;
		}

		/* Handle typical -s short options */
		if (arg[0] == spec->alias) {
			*value = NULL;
			return spec;
		}
	}

	return NULL;
}

INLINE(const adopt_spec *) spec_for_arg(adopt_parser *parser)
{
	const adopt_spec *spec;
	size_t args = 0;

	for (spec = parser->specs; spec->type; ++spec) {
		if (spec->type == ADOPT_TYPE_ARG) {
			if (args == parser->arg_idx) {
				parser->arg_idx++;
				return spec;
			}

			args++;
		}

		if (spec->type == ADOPT_TYPE_ARGS && args == parser->arg_idx)
			return spec;
	}

	return NULL;
}

INLINE(int) spec_is_choice(const adopt_spec *spec)
{
	return ((spec + 1)->type &&
	       ((spec + 1)->usage & ADOPT_USAGE_CHOICE));
}

/*
 * If we have a choice with switches and bare arguments, and we see
 * the switch, then we no longer expect the bare argument.
 */
INLINE(void) consume_choices(const adopt_spec *spec, adopt_parser *parser)
{
	/* back up to the beginning of the choices */
	while (spec->type && (spec->usage & ADOPT_USAGE_CHOICE))
		--spec;

	if (!spec_is_choice(spec))
		return;

	do {
		if (spec->type == ADOPT_TYPE_ARG)
			parser->arg_idx++;
		++spec;
	} while(spec->type && (spec->usage & ADOPT_USAGE_CHOICE));
}

static adopt_status_t parse_long(adopt_opt *opt, adopt_parser *parser)
{
	const adopt_spec *spec;
	char *arg = parser->args[parser->idx++];
	const char *value = NULL;
	int is_negated = 0, has_value = 0;

	opt->arg = arg;

	if ((spec = spec_for_long(&is_negated, &has_value, &value, parser, &arg[2])) == NULL) {
		opt->spec = NULL;
		opt->status = ADOPT_STATUS_UNKNOWN_OPTION;
		goto done;
	}

	opt->spec = spec;

	/* Future options parsed as literal */
	if (spec->type == ADOPT_TYPE_LITERAL)
		parser->in_literal = 1;

	/* --bool or --no-bool */
	else if (spec->type == ADOPT_TYPE_BOOL && spec->value)
		*((int *)spec->value) = !is_negated;

	/* --accumulate */
	else if (spec->type == ADOPT_TYPE_ACCUMULATOR && spec->value)
		*((int *)spec->value) += spec->switch_value ? spec->switch_value : 1;

	/* --switch */
	else if (spec->type == ADOPT_TYPE_SWITCH && spec->value)
		*((int *)spec->value) = spec->switch_value;

	/* Parse values as "--foo=bar" or "--foo bar" */
	else if (spec->type == ADOPT_TYPE_VALUE) {
		if (has_value)
			opt->value = (char *)value;
		else if ((parser->idx + 1) <= parser->args_len)
			opt->value = parser->args[parser->idx++];

		if (spec->value)
			*((char **)spec->value) = opt->value;
	}

	/* Required argument was not provided */
	if (spec->type == ADOPT_TYPE_VALUE &&
	    !opt->value &&
	    !(spec->usage & ADOPT_USAGE_VALUE_OPTIONAL))
		opt->status = ADOPT_STATUS_MISSING_VALUE;
	else
		opt->status = ADOPT_STATUS_OK;

	consume_choices(opt->spec, parser);

done:
	return opt->status;
}

static adopt_status_t parse_short(adopt_opt *opt, adopt_parser *parser)
{
	const adopt_spec *spec;
	char *arg = parser->args[parser->idx++];
	const char *value;

	opt->arg = arg;

	if ((spec = spec_for_short(&value, parser, &arg[1 + parser->in_short])) == NULL) {
		opt->spec = NULL;
		opt->status = ADOPT_STATUS_UNKNOWN_OPTION;
		goto done;
	}

	opt->spec = spec;

	if (spec->type == ADOPT_TYPE_BOOL && spec->value)
		*((int *)spec->value) = 1;

	else if (spec->type == ADOPT_TYPE_ACCUMULATOR && spec->value)
		*((int *)spec->value) += spec->switch_value ? spec->switch_value : 1;

	else if (spec->type == ADOPT_TYPE_SWITCH && spec->value)
		*((int *)spec->value) = spec->switch_value;

	/* Parse values as "-ifoo" or "-i foo" */
	else if (spec->type == ADOPT_TYPE_VALUE) {
		if (value)
			opt->value = (char *)value;
		else if ((parser->idx + 1) <= parser->args_len)
			opt->value = parser->args[parser->idx++];

		if (spec->value)
			*((char **)spec->value) = opt->value;
	}

	/*
	 * Handle compressed short arguments, like "-fbcd"; see if there's
	 * another character after the one we processed.  If not, advance
	 * the parser index.
	 */
	if (spec->type != ADOPT_TYPE_VALUE && arg[2 + parser->in_short] != '\0') {
		parser->in_short++;
		parser->idx--;
	} else {
		parser->in_short = 0;
	}

	/* Required argument was not provided */
	if (spec->type == ADOPT_TYPE_VALUE && !opt->value)
		opt->status = ADOPT_STATUS_MISSING_VALUE;
	else
		opt->status = ADOPT_STATUS_OK;

	consume_choices(opt->spec, parser);

done:
	return opt->status;
}

static adopt_status_t parse_arg(adopt_opt *opt, adopt_parser *parser)
{
	const adopt_spec *spec = spec_for_arg(parser);

	opt->spec = spec;
	opt->arg = parser->args[parser->idx];

	if (!spec) {
		parser->idx++;
		opt->status = ADOPT_STATUS_UNKNOWN_OPTION;
	} else if (spec->type == ADOPT_TYPE_ARGS) {
		if (spec->value)
			*((char ***)spec->value) = &parser->args[parser->idx];

		/*
		 * We have started a list of arguments; the remainder of
		 * given arguments need not be examined.
		 */
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

static int support_gnu_style(unsigned int flags)
{
	if ((flags & ADOPT_PARSE_FORCE_GNU) != 0)
		return 1;

	if ((flags & ADOPT_PARSE_GNU) == 0)
		return 0;

	/* TODO: Windows */
#if defined(_WIN32) && defined(UNICODE)
	if (_wgetenv(L"POSIXLY_CORRECT") != NULL)
		return 0;
#else
	if (getenv("POSIXLY_CORRECT") != NULL)
		return 0;
#endif

	return 1;
}

void adopt_parser_init(
	adopt_parser *parser,
	const adopt_spec specs[],
	char **args,
	size_t args_len,
	unsigned int flags)
{
	assert(parser);

	memset(parser, 0x0, sizeof(adopt_parser));

	parser->specs = specs;
	parser->args = args;
	parser->args_len = args_len;
	parser->flags = flags;

	parser->needs_sort = support_gnu_style(flags);
}

INLINE(const adopt_spec *) spec_for_sort(
	int *needs_value,
	const adopt_parser *parser,
	const char *arg)
{
	int is_negated, has_value = 0;
	const char *value;
	const adopt_spec *spec = NULL;
	size_t idx = 0;

	*needs_value = 0;

	if (strncmp(arg, "--", 2) == 0) {
		spec = spec_for_long(&is_negated, &has_value, &value, parser, &arg[2]);
		*needs_value = !has_value;
	}

	else if (strncmp(arg, "-", 1) == 0) {
		spec = spec_for_short(&value, parser, &arg[1]);

		/*
		 * Advance through compressed short arguments to see if
		 * the last one has a value, eg "-xvffilename".
		 */
		while (spec && !value && arg[1 + ++idx] != '\0')
			spec = spec_for_short(&value, parser, &arg[1 + idx]);

		*needs_value = (value == NULL);
	}

	return spec;
}

/*
 * Some parsers allow for handling arguments like "file1 --help file2";
 * this is done by re-sorting the arguments in-place; emulate that.
 */
static int sort_gnu_style(adopt_parser *parser)
{
	size_t i, j, insert_idx = parser->idx, offset;
	const adopt_spec *spec;
	char *option, *value;
	int needs_value, changed = 0;

	parser->needs_sort = 0;

	for (i = parser->idx; i < parser->args_len; i++) {
		spec = spec_for_sort(&needs_value, parser, parser->args[i]);

		/* Not a "-" or "--" prefixed option.  No change. */
		if (!spec)
			continue;

		/* A "--" alone means remaining args are literal. */
		if (spec->type == ADOPT_TYPE_LITERAL)
			break;

		option = parser->args[i];

		/*
		 * If the argument is a value type and doesn't already
		 * have a value (eg "--foo=bar" or "-fbar") then we need
		 * to copy the next argument as its value.
		 */
		if (spec->type == ADOPT_TYPE_VALUE && needs_value) {
			/*
			 * A required value is not provided; set parser
			 * index to this value so that we fail on it.
			 */
			if (i + 1 >= parser->args_len) {
				parser->idx = i;
				return 1;
			}

			value = parser->args[i + 1];
			offset = 1;
		} else {
			value = NULL;
			offset = 0;
		}

		/* Caller error if args[0] is an option. */
		if (i == 0)
			return 0;

		/* Shift args up one (or two) and insert the option */
		for (j = i; j > insert_idx; j--)
			parser->args[j + offset] = parser->args[j - 1];

		parser->args[insert_idx] = option;

		if (value)
			parser->args[insert_idx + 1] = value;

		insert_idx += (1 + offset);
		i += offset;

		changed = 1;
	}

	return changed;
}

adopt_status_t adopt_parser_next(adopt_opt *opt, adopt_parser *parser)
{
	assert(opt && parser);

	memset(opt, 0x0, sizeof(adopt_opt));

	if (parser->idx >= parser->args_len) {
		opt->args_len = parser->in_args;
		return ADOPT_STATUS_DONE;
	}

	/* Handle options in long form, those beginning with "--" */
	if (strncmp(parser->args[parser->idx], "--", 2) == 0 &&
	    !parser->in_short &&
	    !parser->in_literal)
		return parse_long(opt, parser);

	/* Handle options in short form, those beginning with "-" */
	else if (parser->in_short ||
	         (strncmp(parser->args[parser->idx], "-", 1) == 0 &&
		  !parser->in_literal))
		return parse_short(opt, parser);

	/*
	 * We've reached the first "bare" argument.  In POSIX mode, all
	 * remaining items on the command line are arguments.  In GNU
	 * mode, there may be long or short options after this.  Sort any
	 * options up to this position then re-parse the current position.
	 */
	if (parser->needs_sort && sort_gnu_style(parser))
		return adopt_parser_next(opt, parser);

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
	size_t args_len,
	unsigned int flags)
{
	adopt_parser parser;
	const adopt_spec **given_specs;
	size_t given_idx = 0;

	adopt_parser_init(&parser, specs, args, args_len, flags);

	given_specs = alloca(sizeof(const adopt_spec *) * (args_len + 1));

	while (adopt_parser_next(opt, &parser)) {
		if (opt->status != ADOPT_STATUS_OK &&
		    opt->status != ADOPT_STATUS_DONE)
			return opt->status;

		if ((opt->spec->usage & ADOPT_USAGE_STOP_PARSING))
			return (opt->status = ADOPT_STATUS_DONE);

		given_specs[given_idx++] = opt->spec;
	}

	given_specs[given_idx] = NULL;

	return validate_required(opt, specs, given_specs);
}

int adopt_foreach(
	const adopt_spec specs[],
	char **args,
	size_t args_len,
	unsigned int flags,
	int (*callback)(adopt_opt *, void *),
	void *callback_data)
{
	adopt_parser parser;
	adopt_opt opt;
	int ret;

	adopt_parser_init(&parser, specs, args, args_len, flags);

	while (adopt_parser_next(&opt, &parser)) {
		if ((ret = callback(&opt, callback_data)) != 0)
			return ret;
	}

	return 0;
}

static int spec_name_fprint(FILE *file, const adopt_spec *spec)
{
	int error;

	if (spec->type == ADOPT_TYPE_ARG)
		error = fprintf(file, "%s", spec->value_name);
	else if (spec->type == ADOPT_TYPE_ARGS)
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
			break;
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

		if (spec->type == ADOPT_TYPE_VALUE && spec->alias &&
		    !(spec->usage & ADOPT_USAGE_VALUE_OPTIONAL) &&
		    !(spec->usage & ADOPT_USAGE_SHOW_LONG))
			error = fprintf(file, "-%c <%s>", spec->alias, spec->value_name);
		else if (spec->type == ADOPT_TYPE_VALUE && spec->alias &&
		         !(spec->usage & ADOPT_USAGE_SHOW_LONG))
			error = fprintf(file, "-%c [<%s>]", spec->alias, spec->value_name);
		else if (spec->type == ADOPT_TYPE_VALUE &&
		         !(spec->usage & ADOPT_USAGE_VALUE_OPTIONAL))
			error = fprintf(file, "--%s[=<%s>]", spec->name, spec->value_name);
		else if (spec->type == ADOPT_TYPE_VALUE)
			error = fprintf(file, "--%s=<%s>", spec->name, spec->value_name);
		else if (spec->type == ADOPT_TYPE_ARG)
			error = fprintf(file, "<%s>", spec->value_name);
		else if (spec->type == ADOPT_TYPE_ARGS)
			error = fprintf(file, "<%s>...", spec->value_name);
		else if (spec->type == ADOPT_TYPE_LITERAL)
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

