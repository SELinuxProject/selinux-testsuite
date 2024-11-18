#!/usr/bin/perl

use strict;
use warnings;

use Test::Harness;

my @dirs    = split( / /, $ENV{SUBDIRS} );
my @scripts = ();

for (@dirs) {
    push @scripts, "$_/test";
}

my $output = `id`;
$output =~ /uid=\d+\((\w+)\).*context=(\w+):(\w+):(\w+)/
  || die("Can't determine user's id\n");
my $unixuser = $1;
my $user     = $2;
my $role     = $3;
my $type     = $4;

print "Running as user $unixuser with context $2:$3:$4\n\n";

if ( ( $role ne "sysadm_r" && $role ne "unconfined_r" && $role ne "system_r" )
    || $unixuser ne "root" )
{
    print
"These tests are intended to be run as root, in the sysadm_r or unconfined_r role\n";
    exit;
}

$output = `getenforce`;
chop $output;
if ( $output ne "Enforcing" && $output ne "enforcing" ) {
    print "These tests are intended to be run in enforcing mode only.\n";
    print "Run 'setenforce 1' to switch to enforcing mode.\n";
    exit;
}

runtests(@scripts);

