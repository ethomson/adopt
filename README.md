Adopt
=====

A portable command-line argument parser for C that handles short
(`-a`, `-b foo`, etc) and long (`--arg-a`, `--arg-b=foo`) style 
options.  It is meant to be compatible with GNU getopt in
command-line usage (though not as an API) and available under an
MIT license.

Adopt can also produces help syntax and usage messages based on
the arguments you accept, so that you don't need to remember to
update your usage messages when you add a new option.

Types of options
----------------

* Boolean values are options that do not take a value, and are
  either set or unset, for example `-v` or `--verbose`.  Booleans
  are shorthand for switches that are assigned a value of `1` when
  present.
* Switches are options that do not take a value on the command
  line, for example `--long` or `--short`.  When a switch is present
  on the command line, a variable will be set to a predetermined value.
  This is useful for (say) having a `length` variable that both
  `--long` and `--short` will update.
* Values are options that take a value, for example `-m 2` or
  `--level=2`.
* Literal separators are bare double-dashes, `--` as a lone
  word, indicating that further words will be treated as
  arguments (even if they begin with a dash).
* Arguments are options that are bare words, not prefixed with
  a single or double dash, for example `filename.txt`.
* Argument lists are the remainder of arguments, not prefixed
  with dashes.  For example, an array: `file1.txt`, `file2.txt`,
  ...

Specifying command-line options
-------------------------------

Options should be specified as an array of `adopt_spec` elements,
elements, terminated with an `adopt_spec` initialized to zeroes.

```c
int verbose = 0;
int volume = 1;
char *channel = "default";
char *filename1 = NULL;
char *filename2 = NULL;

adopt_spec opt_specs[] = {
    /* `verbose`, above, will be set to `1` when specified. */
    { ADOPT_BOOL, "verbose", 'v', &verbose },

    /* `volume` will be set to `0` when `--quiet` is specified, and
     * set to `2` when `--loud` is specified.  if neither is specified,
     * it will retain its default value of `1`, defined above.
     */
    { ADOPT_SWITCH, "quiet", 'q', &volume, 0 },
    { ADOPT_SWITCH, "loud", 'l', &volume, 2 },

    /* `channel` will be set to the given argument if an argument is
     * provided to `--channel`, or will be set to `NULL` if no argument
     * was specified.
     */
    { ADOPT_VALUE, "channel", 'c', &channel, NULL },

    /* A double dash (`--`) may be specified to indicate that the parser
     * should stop treated arguments as possible options and treat
     * remaining arguments as literal arguments; which allows a user
     * to specify `--loud` as an actual literal filename (below).
     */
    { ADOPT_LITERAL },

    /* `filename` will be set to the first bare argument */
    { ADOPT_ARG, NULL, 0, &filename1 },

    /* `filename2` will be set to the first bare argument */
    { ADOPT_ARG, NULL, 0, &filename2 },

    /* End of the specification list. */
    { 0 },
};
```

Parsing arguments
-----------------

The simplest way to parse arguments is by calling `adopt_parse`.
This will parse the arguments given to it, will set the given
`spec->value`s to the appropriate arguments, and will stop and
return an error on any invalid input.  If there are errors, you
can display that information with `adopt_status_fprint`.

```c
int main(int argc, const char **argv)
{
    adopt_parser parser;
    adopt_opt result;
    const char *value;
    const char *file;
    
    if (adopt_parse(&result, opt_specs, argv + 1, argc - 1) < 0) {
            adopt_status_fprint(stderr, &opt);
            adopt_usage_fprint(stderr, argv[0], opt_specs);
            return 129;
    }
}
```

Parsing arguments individually
-------------------------------

For more control over your parsing, you may iterate over the
parser in a loop.  After initializing the parser by calling
`adopt_parser_init` with the `adopt_spec`s and the command-line
arguments given to your program, you can call `adopt_parser_next`
in a loop to handle each option.
   
```c
int main(int argc, const char **argv)
{
    adopt_parser parser;
    adopt_opt opt;
    const char *value;
    const char *file;
    
    adopt_parser_init(&parser, opt_specs, argv + 1, argc - 1);
    
    while (adopt_parser_next(&opt, &parser)) {
        /*
         * Although regular practice would be to simply let the parser
         * set the variables for you, there is information about the
         * parsed argument available in the `opt` struct.  For example,
         * `opt.spec` will point to the `adopt_spec` that was parsed,
         * or `NULL` if it did not match a specification.
         */
        if (!opt.spec) {
            fprintf(stderr, "Unknown option: %s\n", opt.arg);
            return 129;
        }
    }
}
```

Required arguments
------------------

Some value options may require arguments, and it would be an error for
callers to omit those.  

The simplest way to detect unspecified values is to inspect for `NULL`.
First, set a variable to a default value, like with `channel` above.  If
the `--channel` option is specified without a value, then `channel` will
be set to `NULL` when it is read.  This can be detected either during the
processing loop, or after.  For example:

```c
if (!channel) {
    fprintf(stderr, "Option: %s requires an argument\n", opt.arg);
    return 129;
}
```

### Inspecting the `opt` struct

If you cannot use a sentinal value, perhaps because `NULL` is a useful
value, you can also inspect the `opt` struct.  For example:

```c
if (opt.spec->value == &channel && !channel) {
    fprintf(stderr, "Option: %s requires an argument\n", opt.arg);
    return 129;
}
```

Displaying usage
----------------

If you provide additional usage information in your `adopt_spec`s, you
can have adopt print this usage information for you based on the options
you accept.  This allows you to define a single structure for both
argument parsing _and_ help information, which keeps your help in sync
with the code automatically.

Simply set the three additional structure members:  `value_name` is the
one-word description of the argument (for options of type `ADOPT_VALUE`),
`help` is a short description of the option, and `usage` is a set of
flags that determine whether all or part of the option is required.

(Specifying an option or a value does not affect parsing, it only affects
how help is displayed.  If an option or value is not required, its help
will display it within square brackets.)

Adding these to the above example:

```c
adopt_spec opt_specs[] = {
    /* For bools and switches, you probably only want to set the help.
     * There is no value to describe, and these are unlikely to be
     * required.
     */
    { ADOPT_BOOL, "verbose", 'v', &verbose,
      NULL, "Turn on verbose mode", 0 },

    /* Specifying that an option is an `ADOPT_USAGE_CHOICE` indicates
     * that it is orthogonal to the previous entry.  These two options
     * will be rendered together in help output as `[-q|-l]`.
     */
    { ADOPT_SWITCH, "quiet", 'q', &volume, 0,
      NULL, "Emit no output", 0 },
    { ADOPT_SWITCH, "loud", 'l', &volume, 2 },
      NULL, "Emit louder than usual output", ADOPT_USAGE_CHOICE },

    /* Set the `value_name` and specify that the value is required.
     * This will be rendered in help output as `-c <channel>`;
     * if it was not required, it would be rendered as `-c [<channel>]`.
     */
    { ADOPT_VALUE, "channel", 'c', &channel, NULL,
      "channel", "Set the channel number", ADOPT_USAGE_VALUE_REQUIRED },

    { ADOPT_LITERAL },

    /* `filename` is required.  It will be rendered in help output as
     * `<file1>`.
     */
    { ADOPT_ARG, NULL, 0, &filename1, NULL,
      "file1", "The first filename", ADOPT_USAGE_REQUIRED },

    /* `filename` is not required.  It will be rendered in help output
     * as `[<file2>]`.
     */
    { ADOPT_ARG, NULL, 0, &filename2, NULL,
      "file2", "The second (optional) filename", 0 },

    /* End of the specification list. */
    { 0 },
};
```

If you call `adopt_usage_fprint` with the above specifications, it will emit:

```
usage: ./example [-v] [-q] [-l] [-c <channel>] -- <file1> [<file2>]
```

Inclusion in your product
-------------------------

If you loathe additional namespaces, and want to use adopt inside your
product without using the `adopt_` prefix, you can use the included
`rename.pl` script to give the functions and structs your own prefix.

For example:

```bash
% ./rename.pl ed_opt
```

Will product `ed_opt.c` and `ed_opt.h` that can be included in an
application, with the variables renamed to `ed_opt` (instead of `adopt`)
and enum names renamed to `ED_OPT` (instead of `ADOPT`).

Or simply:

```bash
% ./rename.pl opt
```

To produce `opt.c` and `opt.h`, with variables prefixed with `opt`.

About Adopt
-----------
Adopt was written by Edward Thomson <ethomson@edwardthomson.com>
and is available under the MIT License.  Please see the
included file `LICENSE` for more information.

Adopt takes its name from the only entry in `/usr/dict/words`
that ends in the letters **opt**.  But if you happen to like adopt,
consider a donation to your local pet shelter or human society.

