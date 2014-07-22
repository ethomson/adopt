#!/usr/bin/perl -w

use strict;

die "usage: $0 <prefix>\n" if ($#ARGV < 0);

my $prefix = $ARGV[0];
my $prefix_upper = $prefix;
$prefix_upper =~ tr/a-z/A-Z/;

translate("adopt.c", "${prefix}.c");
translate("adopt.h", "${prefix}.h");

sub translate {
	my($in, $out) = @_;

	open(IN, $in) || die "$0: could not open ${in}: $!\n";
	open(OUT, '>' . $out) || die "$0: could not open ${out}: $!\n";

	while(<IN>) {
		# if a prefix becomes foo_opt, we want to rewrite adopt_opt specially
		# to avoid it becoming foo_opt_opt
		s/adopt_opt/${prefix}/g if ($prefix =~ /_opt$/);

		s/adopt_/${prefix}_/g;
		s/ADOPT_/${prefix_upper}_/g;

		print OUT;
	}

	close(IN);
	close(OUT);
}

