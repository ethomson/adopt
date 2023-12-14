/*
 * Copyright (c), Edward Thomson <ethomson@edwardthomson.com>
 * All rights reserved.
 *
 * This file is part of adopt, distributed under the MIT license.
 * For full terms and conditions, see the included LICENSE file.
 */

#ifndef ADOPT_H
#define ADOPT_H

#include <stdio.h>
#include <stdint.h>

/**
 * The type of argument to be parsed.
 */
typedef enum {
	ADOPT_TYPE_NONE = 0,

	/**
	 * An option that, when specified, sets a given value to true.
	 * This is useful for options like "--debug".  A negation
	 * option (beginning with "no-") is implicitly specified; for
	 * example "--no-debug".  The `value` pointer in the returned
	 * option will be set to `1` when this is specified, and set to
	 * `0` when the negation "no-" option is specified.
	 */
	ADOPT_TYPE_BOOL,

	/**
	 * An option that, when specified, sets the given `value` pointer
	 * to the specified `switch_value`.  This is useful for booleans
	 * where you do not want the implicit negation that comes with an
	 * `ADOPT_TYPE_BOOL`, or for switches that multiplex a value, like
	 * setting a mode.  For example, `--read` may set the `value` to
	 * `MODE_READ` and `--write` may set the `value` to `MODE_WRITE`.
	 */
	ADOPT_TYPE_SWITCH,

	/**
	 * An option that, when specified, increments the given
	 * `value` by the given `switch_value`.  This can be specified
	 * multiple times to continue to increment the `value`.
	 * (For example, "-vvv" to set verbosity to 3.)
	 */
	ADOPT_TYPE_ACCUMULATOR,

	/**
	 * An option that takes a value, for example `-n value`,
	 * `-nvalue`, `--name value` or `--name=value`.
	 */
	ADOPT_TYPE_VALUE,

	/**
	 * A bare "--" that indicates that arguments following this are
	 * literal.  This allows callers to specify things that might
	 * otherwise look like options, for example to operate on a file
	 * named "-rf" then you can invoke "program -- -rf" to treat
	 * "-rf" as an argument not an option.
	 */
	ADOPT_TYPE_LITERAL,

	/**
	 * A single argument, not an option.  When options are exhausted,
	 * arguments will be matches in the order that they're specified
	 * in the spec list.  For example, if two `ADOPT_TYPE_ARGS` are
	 * specified, `input_file` and `output_file`, then the first bare
	 * argument on the command line will be `input_file` and the
	 * second will be `output_file`.
	 */
	ADOPT_TYPE_ARG,

	/**
	 * A collection of arguments.  This is useful when you want to take
	 * a list of arguments, for example, multiple paths.  When specified,
	 * the value will be set to the first argument in the list.
	 */
	ADOPT_TYPE_ARGS,
} adopt_type_t;

/**
 * Additional information about an option, including parsing
 * restrictions and usage information to be displayed to the end-user.
 */
typedef enum {
	/** Defaults for the argument. */
	ADOPT_USAGE_DEFAULT  = 0,

	/** This argument is required. */
	ADOPT_USAGE_REQUIRED = (1u << 0),

	/**
	 * This is a multiple choice argument, combined with the previous
	 * argument.  For example, when the previous argument is `-f` and
	 * this optional is applied to an argument of type `-b` then one
	 * of `-f` or `-b` may be specified.
	 */
	ADOPT_USAGE_CHOICE = (1u << 1),

	/**
	 * This argument short-circuits the remainder of parsing.
	 * Useful for arguments like `--help`.
	 */
	ADOPT_USAGE_STOP_PARSING = (1u << 2),

	/** The argument's value is optional ("-n" or "-n foo") */
	ADOPT_USAGE_VALUE_OPTIONAL = (1u << 3),

	/** This argument should not be displayed in usage. */
	ADOPT_USAGE_HIDDEN = (1u << 4),

	/** In usage, show the long format instead of the abbreviated format. */
	ADOPT_USAGE_SHOW_LONG = (1u << 5),
} adopt_usage_t;

typedef enum {
	/** Default parsing behavior. */
	ADOPT_PARSE_DEFAULT  = 0,

	/**
	 * Parse with GNU `getopt_long` style behavior, where options can
	 * be intermixed with arguments at any position (for example,
	 * "file1 --help file2".)  Like `getopt_long`, this can mutate the
	 * arguments given.
	 */
	ADOPT_PARSE_GNU = (1u << 0),

	/**
	 * Force GNU `getopt_long` style behavior; the `POSIXLY_CORRECT`
	 * environment variable is ignored.
	 */
	ADOPT_PARSE_FORCE_GNU = (1u << 1),
} adopt_flag_t;

/** Specification for an available option. */
typedef struct adopt_spec {
	/** Type of option expected. */
	adopt_type_t type;

	/** Name of the long option. */
	const char *name;

	/** The alias is the short (one-character) option alias. */
	const char alias;

	/**
	 * If this spec is of type `ADOPT_TYPE_BOOL`, this is a pointer
	 * to an `int` that will be set to `1` if the option is specified.
	 *
	 * If this spec is of type `ADOPT_TYPE_SWITCH`, this is a pointer
	 * to an `int` that will be set to the opt's `switch_value` (below)
	 * when this option is specified.
	 *
	 * If this spec is of type `ADOPT_TYPE_ACCUMULATOR`, this is a
	 * pointer to an `int` that will be incremented by the opt's
	 * `switch_value` (below).  If no `switch_value` is provided then
	 * the value will be incremented by 1.
	 *
	 * If this spec is of type `ADOPT_TYPE_VALUE`,
	 * `ADOPT_TYPE_VALUE_OPTIONAL`, or `ADOPT_TYPE_ARG`, this is
	 * a pointer to a `char *` that will be set to the value
	 * specified on the command line.
	 *
	 * If this spec is of type `ADOPT_TYPE_ARGS`, this is a pointer
	 * to a `char **` that will be set to the remaining values
	 * specified on the command line.
	 */
	void *value;

	/**
	 * If this spec is of type `ADOPT_TYPE_SWITCH`, this is the value
	 * to set in the option's `value` pointer when it is specified.  If
	 * this spec is of type `ADOPT_TYPE_ACCUMULATOR`, this is the value
	 * to increment in the option's `value` pointer when it is
	 * specified.  This is ignored for other opt types.
	 */
	int switch_value;

	/**
	 * Optional usage flags that change parsing behavior and how
	 * usage information is shown to the end-user.
	 */
	uint32_t usage;

	/**
	 * The name of the value, provided when creating usage information.
	 * This is required only for the functions that display usage
	 * information and only when a spec is of type `ADOPT_TYPE_VALUE,
	 * `ADOPT_TYPE_ARG` or `ADOPT_TYPE_ARGS``.
	 */
	const char *value_name;

	/**
	 * Optional short description of the option to display to the
	 * end-user.  This is only used when creating usage information.
	 */
	const char *help;
} adopt_spec;

/** Return value for `adopt_parser_next`. */
typedef enum {
	/** Parsing is complete; there are no more arguments. */
	ADOPT_STATUS_DONE = 0,

	/**
	 * This argument was parsed correctly; the `opt` structure is
	 * populated and the value pointer has been set.
	 */
	ADOPT_STATUS_OK = 1,

	/**
	 * The argument could not be parsed correctly, it does not match
	 * any of the specifications provided.
	 */
	ADOPT_STATUS_UNKNOWN_OPTION = 2,

	/**
	 * The argument matched a spec of type `ADOPT_VALUE`, but no value
	 * was provided.
	 */
	ADOPT_STATUS_MISSING_VALUE = 3,

	/** A required argument was not provided. */
	ADOPT_STATUS_MISSING_ARGUMENT = 4,
} adopt_status_t;

/** An option provided on the command-line. */
typedef struct adopt_opt {
	/** The status of parsing the most recent argument. */
	adopt_status_t status;

	/**
	 * The specification that was provided on the command-line, or
	 * `NULL` if the argument did not match an `adopt_spec`.
	 */
	const adopt_spec *spec;

	/**
	 * The argument as it was specified on the command-line, including
	 * dashes, eg, `-f` or `--foo`.
	 */
	char *arg;

	/**
	 * If the spec is of type `ADOPT_VALUE` or `ADOPT_VALUE_OPTIONAL`,
	 * this is the value provided to the argument.
	 */
	char *value;

	/**
	 * If the argument is of type `ADOPT_ARGS`, this is the number of
	 * arguments remaining.  This value is persisted even when parsing
	 * is complete and `status` == `ADOPT_STATUS_DONE`.
	 */
	size_t args_len;
} adopt_opt;

/* The internal parser state.  Callers should not modify this structure. */
typedef struct adopt_parser {
	const adopt_spec *specs;
	char **args;
	size_t args_len;
	unsigned int flags;

	/* Parser state */
	size_t idx;
	size_t arg_idx;
	size_t in_args;
	size_t in_short;
	unsigned int needs_sort : 1,
	             in_literal : 1;
} adopt_parser;

/**
 * Parses all the command-line arguments and updates all the options using
 * the pointers provided.  Parsing stops on any invalid argument and
 * information about the failure will be provided in the opt argument.
 *
 * This is the simplest way to parse options; it handles the initialization
 * (`parser_init`) and looping (`parser_next`).
 *
 * @param opt The The `adopt_opt` information that failed parsing
 * @param specs A NULL-terminated array of `adopt_spec`s that can be parsed
 * @param args The arguments that will be parsed
 * @param args_len The length of arguments to be parsed
 * @param flags The `adopt_flag_t flags for parsing
 */
adopt_status_t adopt_parse(
    adopt_opt *opt,
    const adopt_spec specs[],
    char **args,
    size_t args_len,
    unsigned int flags);

/**
 * Initializes a parser that parses the given arguments according to the
 * given specifications.
 *
 * @param parser The `adopt_parser` that will be initialized
 * @param specs A NULL-terminated array of `adopt_spec`s that can be parsed
 * @param args The arguments that will be parsed
 * @param args_len The length of arguments to be parsed
 * @param flags The `adopt_flag_t flags for parsing
 */
void adopt_parser_init(
	adopt_parser *parser,
	const adopt_spec specs[],
	char **args,
	size_t args_len,
	unsigned int flags);

/**
 * Parses the next command-line argument and places the information about
 * the argument into the given `opt` data.
 *
 * @param opt The `adopt_opt` information parsed from the argument
 * @param parser An `adopt_parser` that has been initialized with
 *        `adopt_parser_init`
 * @return true if the caller should continue iterating, or 0 if there are
 *         no arguments left to process.
 */
adopt_status_t adopt_parser_next(
	adopt_opt *opt,
	adopt_parser *parser);

/**
 * Prints the status after parsing the most recent argument.  This is
 * useful for printing an error message when an unknown argument was
 * specified, or when an argument was specified without a value.
 *
 * @param file The file to print information to
 * @param command The name of the command to use when printing (optional)
 * @param opt The option that failed to parse
 * @return 0 on success, -1 on failure
 */
int adopt_status_fprint(
	FILE *file,
	const char *command,
	const adopt_opt *opt);

/**
 * Prints usage information to the given file handle.
 *
 * @param file The file to print information to
 * @param command The name of the command to use when printing
 * @param specs The specifications allowed by the command
 * @return 0 on success, -1 on failure
 */
int adopt_usage_fprint(
	FILE *file,
	const char *command,
	const adopt_spec specs[]);

#endif /* ADOPT_H */
