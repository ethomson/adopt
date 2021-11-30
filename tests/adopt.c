#include "clar.h"
#include "adopt.h"

typedef struct {
	adopt_spec *spec;
	const char *arg;
} adopt_expected;

static void test_parse(
	adopt_spec *specs,
	char *args[],
	size_t argslen,
	adopt_expected expected[],
	size_t expectlen)
{
	adopt_parser parser;
	adopt_opt opt;
	size_t i;

	adopt_parser_init(&parser, specs, args, argslen);

	for (i = 0; i < expectlen; ++i) {
		cl_assert(adopt_parser_next(&opt, &parser) > 0);

		cl_assert_equal_p(expected[i].spec, opt.spec);

		if (expected[i].arg && expected[i].spec == NULL)
			cl_assert_equal_s(expected[i].arg, opt.arg);
		else if (expected[i].arg)
			cl_assert_equal_s(expected[i].arg, opt.value);
		else
			cl_assert(opt.value == NULL);
	}

	cl_assert(adopt_parser_next(&opt, &parser) == 0);
}

static void test_returns_missing_value(
	adopt_spec *specs,
	char *args[],
	size_t argslen)
{
	adopt_parser parser;
	adopt_opt opt;
	adopt_status_t status;

	adopt_parser_init(&parser, specs, args, argslen);

	status = adopt_parser_next(&opt, &parser);
	cl_assert_equal_i(ADOPT_STATUS_MISSING_VALUE, status);
}

void test_adopt__empty(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 0, &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 }
	};

	/* Parse an empty arg list */
	test_parse(specs, NULL, 0, NULL, 0);
	cl_assert_equal_i(0, foo);
	cl_assert_equal_i(0, bar);
}

void test_adopt__args(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 0, &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 }
	};

	char *args[] = { "bare1", "bare2" };
	adopt_expected expected[] = {
		{ NULL, "bare1" },
		{ NULL, "bare2" },
	};

	/* Parse an arg list with only bare arguments */
	test_parse(specs, args, 2, expected, 2);
	cl_assert_equal_i(0, foo);
	cl_assert_equal_i(0, bar);
}

void test_adopt__unknown(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 0, &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 }
	};

	char *args[] = { "--unknown-long", "-u" };
	adopt_expected expected[] = {
		{ NULL, "--unknown-long" },
		{ NULL, "-u" },
	};

	/* Parse an arg list with only bare arguments */
	test_parse(specs, args, 2, expected, 2);
	cl_assert_equal_i(0, foo);
	cl_assert_equal_i(0, bar);
}

void test_adopt__returns_unknown_option(void)
{
	adopt_parser parser;
	adopt_opt opt;
	adopt_status_t status;
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 0, &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 }
	};

	char *args[] = { "--unknown-long", "-u" };

	adopt_parser_init(&parser, specs, args, 2);

	status = adopt_parser_next(&opt, &parser);
	cl_assert_equal_i(ADOPT_STATUS_UNKNOWN_OPTION, status);
}

void test_adopt__long_switches1(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 0, &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 }
	};

	char *args1[] = { "--foo", "bare1" };
	adopt_expected expected1[] = {
		{ &specs[0], NULL },
		{ NULL, "bare1" },
	};

	/* Parse --foo bare1 */
	test_parse(specs, args1, 2, expected1, 2);
	cl_assert_equal_i('f', foo);
	cl_assert_equal_i(0, bar);
}

void test_adopt__long_switches2(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 0, &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 }
	};

	char *args2[] = { "--foo", "--bar" };
	adopt_expected expected2[] = {
		{ &specs[0], NULL },
		{ &specs[1], NULL }
	};

	/* Parse --foo --bar */
	test_parse(specs, args2, 2, expected2, 2);
	cl_assert_equal_i('f', foo);
	cl_assert_equal_i('b', bar);
}


void test_adopt__long_switches3(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 0, &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 }
	};

	char *args3[] = { "--foo", "bare2", "--bar", "-u" };
	adopt_expected expected3[] = {
		{ &specs[0], NULL },
		{ NULL, "bare2" },
		{ &specs[1], NULL },
		{ NULL, "-u" },
	};

	/* Parse --foo bare2 --bar -u */
	test_parse(specs, args3, 4, expected3, 4);
	cl_assert_equal_i('f', foo);
	cl_assert_equal_i('b', bar);
}

void test_adopt__long_values1(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 0, &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 0, &bar, 0 },
		{ 0 }
	};

	char *args1[] = { "--foo", "arg_1" };
	adopt_expected expected1[] = {
		{ &specs[0], "arg_1" },
	};

	/* Parse --foo arg_1 */
	test_parse(specs, args1, 2, expected1, 1);
	cl_assert_equal_s("arg_1", foo);
	cl_assert_equal_s(NULL, bar);
}

void test_adopt__long_values2(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 0, &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 0, &bar, 0 },
		{ 0 }
	};

	char *args2[] = { "--foo", "--bar" };
	adopt_expected expected2[] = {
		{ &specs[0], "--bar" },
	};

	/* Parse --foo --bar */
	test_parse(specs, args2, 2, expected2, 1);
	cl_assert_equal_s("--bar", foo);
	cl_assert_equal_s(NULL, bar);
}

void test_adopt__long_values3(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 0, &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 0, &bar, 0 },
		{ 0 }
	};

	char *args3[] = { "--foo", "--arg_1", "--bar", "arg_2" };
	adopt_expected expected3[] = {
		{ &specs[0], "--arg_1" },
		{ &specs[1], "arg_2" },
	};

	/* Parse --foo --arg_1 --bar arg_2 */
	test_parse(specs, args3, 4, expected3, 2);
	cl_assert_equal_s("--arg_1", foo);
	cl_assert_equal_s("arg_2", bar);
}

void test_adopt__long_values4(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 0, &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 0, &bar, 0 },
		{ 0 }
	};

	char *args4[] = { "--foo=bar" };
	adopt_expected expected4[] = {
		{ &specs[0], "bar" },
	};

	/* Parse --foo=bar */
	test_parse(specs, args4, 1, expected4, 1);
	cl_assert_equal_s("bar", foo);
	cl_assert_equal_s(NULL, bar);
}

void test_adopt__long_values5(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 0, &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 0, &bar, 0 },
		{ 0 }
	};

	char *args5[] = { "--bar=" };
	adopt_expected expected5[] = {
		{ &specs[1], NULL },
	};

	/* Parse --bar= */
	test_parse(specs, args5, 1, expected5, 1);
	cl_assert_equal_s(NULL, foo);
	cl_assert_equal_s(NULL, bar);
}

void test_adopt__returns_missing_value(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE, "foo", 'f', &foo, 0 },
		{ ADOPT_VALUE, "bar", 'b', &bar, 0 },
		{ 0 }
	};
	char *missing1[] = { "--foo" };
	char *missing2[] = { "--foo=" };
	char *missing3[] = { "-f" };

	test_returns_missing_value(specs, missing1, 1);
	test_returns_missing_value(specs, missing2, 1);
	test_returns_missing_value(specs, missing3, 1);
}

void test_adopt__short_switches2(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 'f', &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 'b', &bar, 'b' },
		{ 0 }
	};

	char *args2[] = { "-f", "-b" };
	adopt_expected expected2[] = {
		{ &specs[0], NULL },
		{ &specs[1], NULL }
	};

	/* Parse -f -b */
	test_parse(specs, args2, 2, expected2, 2);
	cl_assert_equal_i('f', foo);
	cl_assert_equal_i('b', bar);
}

void test_adopt__short_switches3(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 'f', &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 'b', &bar, 'b' },
		{ 0 }
	};

	char *args3[] = { "-f", "bare2", "-b", "-u" };
	adopt_expected expected3[] = {
		{ &specs[0], NULL },
		{ NULL, "bare2" },
		{ &specs[1], NULL },
		{ NULL, "-u" },
	};

	/* Parse -f bare2 -b -u */
	test_parse(specs, args3, 4, expected3, 4);
	cl_assert_equal_i('f', foo);
	cl_assert_equal_i('b', bar);
}

void test_adopt__short_values1(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 'f', &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 'b', &bar, 0 },
		{ 0 }
	};

	char *args1[] = { "-f", "arg_1" };
	adopt_expected expected1[] = {
		{ &specs[0], "arg_1" },
	};

	/* Parse -f arg_1 */
	test_parse(specs, args1, 2, expected1, 1);
	cl_assert_equal_s("arg_1", foo);
	cl_assert_equal_s(NULL, bar);
}

void test_adopt__short_values2(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 'f', &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 'b', &bar, 0 },
		{ 0 }
	};

	char *args2[] = { "-f", "--bar" };
	adopt_expected expected2[] = {
		{ &specs[0], "--bar" },
	};

	/* Parse -f --bar */
	test_parse(specs, args2, 2, expected2, 1);
	cl_assert_equal_s("--bar", foo);
	cl_assert_equal_s(NULL, bar);
}

void test_adopt__short_values3(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 'f', &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 'b', &bar, 0 },
		{ 0 }
	};

	char *args3[] = { "-f", "--arg_1", "-b", "arg_2" };
	adopt_expected expected3[] = {
		{ &specs[0], "--arg_1" },
		{ &specs[1], "arg_2" },
	};

	/* Parse -f --arg_1 -b arg_2 */
	test_parse(specs, args3, 4, expected3, 2);
	cl_assert_equal_s("--arg_1", foo);
	cl_assert_equal_s("arg_2", bar);
}

void test_adopt__short_values4(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 'f', &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 'b', &bar, 0 },
		{ 0 }
	};

	char *args4[] = { "-fbar" };
	adopt_expected expected4[] = {
		{ &specs[0], "bar" },
	};

	/* Parse -fbar */
	test_parse(specs, args4, 1, expected4, 1);
	cl_assert_equal_s("bar", foo);
	cl_assert_equal_s(NULL, bar);
}

void test_adopt__short_values5(void)
{
	char *foo = NULL, *bar = NULL;

	adopt_spec specs[] = {
		{ ADOPT_VALUE_OPTIONAL, "foo", 'f', &foo, 0 },
		{ ADOPT_VALUE_OPTIONAL, "bar", 'b', &bar, 0 },
		{ 0 }
	};

	char *args5[] = { "-b" };
	adopt_expected expected5[] = {
		{ &specs[1], NULL },
	};

	/* Parse -b */
	test_parse(specs, args5, 1, expected5, 1);
	cl_assert_equal_s(NULL, foo);
	cl_assert_equal_s(NULL, bar);
}

void test_adopt__literal(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, "foo", 0, &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ ADOPT_LITERAL },
		{ 0 }
	};

	char *args1[] = { "--foo", "--", "--bar" };
	adopt_expected expected1[] = {
		{ &specs[0], NULL },
		{ &specs[2], NULL },
		{ NULL, "--bar" },
	};

	/* Parse --foo -- --bar */
	test_parse(specs, args1, 3, expected1, 3);
	cl_assert_equal_i('f', foo);
	cl_assert_equal_i(0, bar);
}

void test_adopt__no_long_argument(void)
{
	int foo = 0, bar = 0;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, NULL, 'f', &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 },
	};

	char *args1[] = { "-f", "--bar" };
	adopt_expected expected1[] = {
		{ &specs[0], NULL },
		{ &specs[1], NULL },
	};

	/* Parse -f --bar */
	test_parse(specs, args1, 2, expected1, 2);
	cl_assert_equal_i('f', foo);
	cl_assert_equal_i('b', bar);
}

void test_adopt__parse_oneshot(void)
{
	int foo = 0, bar = 0;
	adopt_opt result;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, NULL, 'f', &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 },
	};

	char *args[] = { "-f", "--bar" };

	cl_must_pass(adopt_parse(&result, specs, args, 2));

	cl_assert_equal_i('f', foo);
	cl_assert_equal_i('b', bar);

	cl_assert_equal_i(ADOPT_STATUS_DONE, result.status);
	cl_assert_equal_p(NULL, result.arg);
	cl_assert_equal_p(NULL, result.value);
}

void test_adopt__parse_oneshot_unknown_option(void)
{
	int foo = 0, bar = 0;
	adopt_opt result;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, NULL, 'f', &foo, 'f' },
		{ ADOPT_SWITCH, "bar", 0, &bar, 'b' },
		{ 0 },
	};

	char *args[] = { "-f", "--bar", "--asdf" };

	cl_assert_equal_i(ADOPT_STATUS_UNKNOWN_OPTION, adopt_parse(&result, specs, args, 3));
}

void test_adopt__parse_oneshot_missing_value(void)
{
	int foo = 0;
	char *bar = NULL;
	adopt_opt result;

	adopt_spec specs[] = {
		{ ADOPT_SWITCH, NULL, 'f', &foo, 'f' },
		{ ADOPT_VALUE,  "bar", 0,  &bar, 'b' },
		{ 0 },
	};

	char *args[] = { "-f", "--bar" };

	cl_assert_equal_i(ADOPT_STATUS_MISSING_VALUE, adopt_parse(&result, specs, args, 2));
}
