#!/usr/bin/perl

$count = shift || 1;

print "Running all tests $count times\n";

for ($i = 0; $i < $count; $i++) {
    print "$i: ";
    $foo = `./runtests.pl`;
    if ($foo =~ m|All tests successful.\n|) {
	print $';
    } else {
	print "Tests Failed: $'\n";
    }
}
