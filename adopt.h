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

	/** An argument that has a value ("--name value" or "-n value") */
	ADOPT_VALUE,

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

	/** A value is required for this argument. */
	ADOPT_USAGE_VALUE_REQUIRED = (1u << 1),

	/** This argument should not be displayed in usage. */
	ADOPT_USAGE_HIDDEN = (1u << 2),

	/** This is a multiple choice argument, combined with the previous arg. */
	ADOPT_USAGE_CHOICE = (1u << 3),
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
	 * If this spec is of type `ADOPT_VALUE`, this is a pointer to
	 * a `char *`, that will be set to the value specified on the
	 * command line.
	 *
	 * If this spec is of type `ADOPT_BOOL`, this is a pointer to
	 * an `int` that will be set to `1` if the option is specified.
	 *
	 * If this spec is of type `ADOPT_SWITCH`, this is a pointer to
	 * an `int` that will be set to the opt's `value` (below) when
	 * this option is specified.
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

/** An option provided on the command-line. */
typedef struct adopt_opt {
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
	 * The value provided to the argument, or `NULL` if the given
	 * argument is a switch argument that does not take a value.
	 * If the argument did not match and `adopt_spec`, this will
	 * point to the unknown argument.
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
int adopt_parser_next(
	adopt_opt *opt,
	adopt_parser *parser);

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
