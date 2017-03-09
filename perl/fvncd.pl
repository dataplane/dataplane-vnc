#!/usr/bin/perl -T
use strict;
use warnings;

use feature qw(switch);
no warnings 'experimental::smartmatch';

use English;
use IO::Socket::IP;
use POSIX qw( setsid);
use Sys::Syslog qw( :standard :macros );

use constant LISTENER_PORT   => 5900;
use constant MAX_CONNECTIONS => 20;

use constant SYSLOG_FACILITY => 'LOG_LOCAL0';
use constant SYSLOG_IDENT    => 'fvncd';
use constant SYSLOG_PRIORITY => 'LOG_INFO';
use constant SYSLOG_SERVER   => '127.0.0.1';

use constant RFB_VERSION => "RFB 003.008\n";

$OUTPUT_AUTOFLUSH = 1;
$SIG{CHLD} = 'IGNORE';    # to avoid having defunct children around

#NOTE: uses one v6 socket for compatibility and simplicity
my $server = IO::Socket::IP->new(
    Listen       => MAX_CONNECTIONS,
    LocalHost    => '::',
    LocalService => LISTENER_PORT,
    Reuse        => 1,
    Type         => SOCK_STREAM,
    V6Only       => 0,
) or die "Could not open port " . LISTENER_PORT . " : $@\n";

# initialize syslog
$Sys::Syslog::host = SYSLOG_SERVER;
openlog( SYSLOG_IDENT, "nodelay,pid", SYSLOG_FACILITY );

daemonize();

my $client;
while ( $client = $server->accept() ) {
    my $pid;
    while ( not defined( $pid = fork() ) ) {
        sleep 5;
    }

    if ($pid) {
        close $client;
        next;
    }

# see http://stackoverflow.com/questions/28719064/how-to-set-in-perl-recv-timeout-in-my-code
    setsockopt( $client, SOL_SOCKET, SO_RCVTIMEO, pack( 'l!l!', 10, 0 ) );
    $client->autoflush(1);

    my $daddr      = v4mapped( $client->sockhost );
    my $saddr      = v4mapped( $client->peerhost );
    my $sport      = $client->peerport;
    my $connection = "saddr: $saddr; sport: $sport; daddr: $daddr";
    logit( { message => "connection $connection" } );

    my $vnc_version = vnc_handshake();
    if ( !$vnc_version ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "recv timeout vnc_handshake $connection"
            }
        );
        exit 0;
    }
    if ( $vnc_version !~ m{ RFB[ ]003[.]00[3578][\n] }xms ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "unknown ProtocolVersion $connection"
            }
        );
        exit 0;
    }

    $vnc_version =~ s{ [RFB0\s]+ }{}gmxs;    # x.y is all we need now
    logit( { message => "ProtocolVersion $vnc_version $connection" } );

    # TODO: work in progress
    given ($vnc_version) {
        when ('3.3') { vnc_version_3_3(); }
        when ('3.5') { vnc_version_3_3(); }    # buggy client, treat as 3.3
        when ('3.7') { vnc_version_3_7(); }
        when ('3.8') { vnc_version_3_8(); }
    }

    exit 0;
}

close $server;
closelog();
exit 0;

# http://stackoverflow.com/questions/1518923/how-can-i-create-a-tcp-server-daemon-process-in-perl
sub daemonize {
    chdir '/' or die "Can't chdir to /: $!";
    open STDIN,  '/dev/null'  or die "Can't read /dev/null: $!";
    open STDOUT, '>/dev/null' or die "Can't write to /dev/null: $!";
    defined( my $pid = fork ) or die "Can't fork: $!";
    exit if $pid;
    setsid or die "Can't start a new session: $!";
    open STDERR, '>&STDOUT' or die "Can't dup stdout: $!";
    return;
}

sub logit {
    my ($arg_ref) = @_;
    my $priority = $arg_ref->{priority} || SYSLOG_PRIORITY;
    my $message = $arg_ref->{message} or return;

    syslog( $priority, "%s", $message );

    return;
}

# check for an IPv6 V4MAPPED address and convert to dotted quad if found
sub v4mapped {
    my $addr = shift or return;
    $addr =~ s{ \A ::ffff: }{}ixms;
    return $addr;
}

# https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst
sub vnc_handshake {

    # XXX: check to make sure send succeeds?
    $client->send(RFB_VERSION);

    my $version;
    my $ret = $client->recv( $version, 12 );

    return if not defined($ret);
    return $version;
}

# TODO: try to go through security types
#       issue challenge, maybe evaluate later to known common passwords?
sub vnc_version_3_3 {
    my $security_type = pack( 'L', 2 );    # default to VNC Authentication

    # XXX: check to make sure send succeeds?
    $client->send($security_type);

    # TODO: receive Security type response
    return;
}

sub vnc_version_3_7 {
    return;
}

sub vnc_version_3_8 {
    return;
}
