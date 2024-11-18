#!/usr/bin/perl

use strict;
use warnings;

use Test::Harness;

my @test = "$ARGV[0]";
runtests(@test);
