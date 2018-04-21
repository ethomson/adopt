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
	ADOPT_NONE = 0,

	/**
	 * An argument that, when specified, sets a given value to true.
	 * This is useful for arguments like "--debug".  The `value` pointer
	 * in the returned option will be set to `1` when this is set.
	 */
	ADOPT_BOOL,

	/**
	 * An argument that, when specified, sets the given `value_ptr`
	 * to the given `value`.
	 */
	ADOPT_SWITCH,

	/** An argument that has a value ("-nvalue" or "--name value") */
	ADOPT_VALUE,

	/** An argument that has an optional value ("-n" or "-n foo") */
	ADOPT_VALUE_OPTIONAL,

	/** The literal arguments follow specifier, bare "--" */
	ADOPT_LITERAL,

	/** A single "free" argument ("path") */
	ADOPT_ARG,

	/** Unmatched arguments, a collection of "free" arguments ("paths...") */
	ADOPT_ARGS,
} adopt_type_t;

/**
 * Usage information for an argument, to be displayed to the end-user.
 * This is only for display, the parser ignores this usage information.
 */
typedef enum {
	/** This argument is required. */
	ADOPT_USAGE_REQUIRED = (1u << 0),

	/** This argument should not be displayed in usage. */
	ADOPT_USAGE_HIDDEN = (1u << 1),

	/** This is a multiple choice argument, combined with the previous arg. */
	ADOPT_USAGE_CHOICE = (1u << 2),

	/** In usage, show the long format instead of the abbreviated format. */
	ADOPT_USAGE_SHOW_LONG = (1u << 3),
} adopt_usage_t;

/** Specification for an available option. */
typedef struct adopt_spec {
	/** Type of option expected. */
	adopt_type_t type;

	/** Name of the long option. */
	const char *name;

	/** The alias is the short (one-character) option alias. */
	const char alias;

	/**
	 * If this spec is of type `ADOPT_BOOL`, this is a pointer to
	 * an `int` that will be set to `1` if the option is specified.
	 *
	 * If this spec is of type `ADOPT_SWITCH`, this is a pointer to
	 * an `int` that will be set to the opt's `value` (below) when
	 * this option is specified.
	 *
	 * If this spec is of type `ADOPT_VALUE` or `ADOPT_VALUE_OPTIONAL`,
	 * this is a pointer to a `char *`, that will be set to the value
	 * specified on the command line.
	 */
	void *value;

	/**
	 * If this spec is of type `ADOPT_SWITCH`, this is the value to
	 * set in the option's `value_ptr` pointer when it is specified.
	 * This is ignored for other opt types.
	 */
	int switch_value;

	/**
	 * The name of the value, provided when creating usage information.
	 * This is required only for the functions that display usage
	 * information and only when a spec is of type `ADOPT_VALUE`.
	 */
	const char *value_name;

	/**
	 * Short description of the option, used when creating usage
	 * information.  This is only used when creating usage information.
	 */
	const char *help;

	/**
	 * Optional `adopt_usage_t`, used when creating usage information.
	 */
	adopt_usage_t usage;
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
} adopt_opt;

/* The internal parser state.  Callers should not modify this structure. */
typedef struct adopt_parser {
	const adopt_spec *specs;
	char **args;
	size_t args_len;

	size_t idx;
	size_t arg_idx;
	int in_literal : 1,
	in_short : 1;
} adopt_parser;

/**
 * Initializes a parser that parses the given arguments according to the
 * given specifications.
 *
 * @param parser The `adopt_parser` that will be initialized
 * @param specs A NULL-terminated array of `adopt_spec`s that can be parsed
 * @param argv The arguments that will be parsed
 * @param args_len The length of arguments to be parsed
 */
void adopt_parser_init(
	adopt_parser *parser,
	const adopt_spec specs[],
	char **argv,
	size_t args_len);

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
 * @param opt The option that failed to parse
 * @return 0 on success, -1 on failure
 */
int adopt_status_fprint(
	FILE *file,
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
