#!/usr/bin/perl

$basedir = $0;  $basedir =~ s|(.*)/[^/]*|$1|;
$output = `id`;
$output =~ /uid=\d+\((\w+)\).*context=(\w+):(\w+):(\w+)/ || die ("Can't determine user's id\n");
$user = $2;
$role = $3;

$socket_sid = `context_to_sid $user:$role:test_socket_base_t`;
$second_sid = `context_to_sid $user:$role:test_socket_secondary_t`;
chop($socket_sid);
chop($second_sid);

$count = shift @ARGV;


if (($pid = fork()) == 0) {
    $bad = 0;
    for ($i = 0; $i < $count; $i++) {
	#print "A";
	$result = system "runas -t test_socket_base_t -- $basedir/client $second_sid $socket_sid";
	if ($result != 0) { $bad++; }
    }
    print "ok($bad,0);\n";
    exit;
}

if (($pid = fork()) == 0) {
    $bad = 0;
    for ($i = 0; $i < $count; $i++) {
	#print "B";
	$result = system "runas -t test_socket_base_t -- $basedir/client $socket_sid $socket_sid";
	if ($result != 0) { $bad++; }
    }
    print "ok($bad,0);\n";
    exit;
}

if (($pid = fork()) == 0) {
    $bad = 0;
    for ($i = 0; $i < $count; $i++) {
	#print "C";
	$result = system "runas -t test_socket_base_t -- $basedir/client $second_sid $socket_sid";
	if ($result != 0) { $bad++; }
    }
    print "ok($bad,0);\n";
    exit;
}

if (($pid = fork()) == 0) {
    $bad = 0;
    for ($i = 0; $i < $count; $i++) {
	#print "D";
	$result = system "runas -t test_socket_base_t -- $basedir/client $socket_sid $socket_sid";
	if ($result != 0) { $bad++; }
    }
    print "ok($bad,0);\n";
    exit;
}

if (($pid = fork()) == 0) {
    $bad = 0;
    for ($i = 0; $i < $count; $i++) {
	#print "E";
	$result = system "runas -t test_socket_base_t -- $basedir/client $second_sid $socket_sid";
	if ($result != 0) { $bad++; }
    }
    print "ok($bad,0);\n";
    exit;
}

for ($i = 0; $i < $count; $i++) {
    $bad = 0;
    #print "F";
	$result = system "runas -t test_socket_base_t -- $basedir/client $socket_sid $socket_sid";
    if ($result != 0) { $bad++; }
}
print "ok($bad,0);\n";
exit;
