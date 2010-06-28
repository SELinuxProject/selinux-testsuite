#!/usr/bin/perl

use Test::Harness;

@dirs = split(/ /, $ENV{SUBDIRS});

for (@dirs) {
    push @scripts, "$_/test";
}

$output = `id`;
$output =~ /uid=\d+\((\w+)\).*context=(\w+):(\w+):(\w+)/ || die ("Can't determine user's id\n");
$unixuser = $1;
$user = $2;
$role = $3;
$type = $4;

print "Running as user $unixuser with context $2:$3:$4\n\n";

if (($role ne "sysadm_r" && $role ne "unconfined_r" && $role ne "system_r") || $unixuser ne "root") {
    print "These tests are intended to be run as root, in the sysadm_r or unconfined_r role\n";
    exit;
}

$output = `getenforce`;
chop $output;
if ($output ne "Enforcing" && $output ne "enforcing") {
    print "These tests are intended to be run in enforcing mode only.\n";
    print "Run 'setenforce 1' to switch to enforcing mode.\n";
    exit;
}

runtests(@scripts);

