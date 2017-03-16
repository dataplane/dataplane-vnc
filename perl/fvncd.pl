#!/usr/bin/perl -T
use strict;
use warnings;

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

$|++;  # autoflush
$SIG{CHLD} = 'IGNORE';    # to avoid having defunct children around

my $server = IO::Socket::IP->new(
    LocalHost    => '::',   # use one v6 socket for simplicity
    Reuse        => 1,
    V6Only       => 0,
    LocalService => LISTENER_PORT,
    Type         => SOCK_STREAM,
    Listen       => MAX_CONNECTIONS,
) or die "Could not open port " . LISTENER_PORT . " : $@\n";

# initialize syslog
$Sys::Syslog::host = SYSLOG_SERVER;
openlog( SYSLOG_IDENT, "nodelay,pid", SYSLOG_FACILITY );

daemonize();

my ( $client, $connection );
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

    my $daddr   = v4mapped( $client->sockhost );
    my $saddr   = v4mapped( $client->peerhost );
    my $sport   = $client->peerport;
    $connection = "saddr: $saddr; sport: $sport; daddr: $daddr";
    logit( { message => "connection $connection" } );

    my $vnc_version = vnc_handshake();
    if ( $vnc_version !~ m{ \A RFB [ ] \d{3} [.] \d{3} [\n] \z }xms ) {
        logit(
            {
                priority => LOG_WARNING,
                message => "invalid ProtocolVersion handshake; $connection"
            }
        );
        exit 0;
    }

    $vnc_version =~ s{ [RFB\s]+ }{}gmxs;    # xxx.yyy is all we need now
    logit( { message => "ProtocolVersion $vnc_version; $connection" } );

    $vnc_version = normalize_vnc_version($vnc_version);

    my %select_version = (
        '3.3' => \&vnc_version_3_3,
        '3.7' => \&vnc_version_3_7,
        '3.8' => \&vnc_version_3_8,
        'UNK' => \&vnc_version_unk,   # unknown / unsupported version
    );
    $select_version{$vnc_version}->();

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
    my $sent = $client->send(RFB_VERSION);
    if ( not defined $sent ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "send failure vnc_handshake; $connection"
            }
        );
        exit 0;
    }

    my $version;
    my $received = $client->recv( $version, 12 );

    if ( not defined $received ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "recv timeout vnc_handshake; $connection"
            }
        );
        exit 0;
    }

    if ( length $version != 12 ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "invalid version length vnc_handshake; $connection"
            }
        );
        exit 0;
    }

    return $version;
}

sub normalize_vnc_version {
    my $version = shift or return;

    return '3.3' if $version eq '003.003';
    return '3.7' if $version eq '003.007';
    return '3.8' if $version eq '003.008';

    # other priority versions exist, but we do not currently support them
    return 'UNK';
}

sub vnc_version_3_3 {
# TODO: randomize vnc and none auth?
# TODO call a sec function?
    # default Authentication type None
    my $security_type = pack( 'N', 0x00000001 );

    my $sent = $client->send($security_type);
    if ( not defined $sent ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "send failure vnc_version_3_3; $connection"
            }
        );
        exit 0;
    }

    client_init();

    return;
}

sub vnc_version_3_7 {
#TODO: return security_type negotiated and then do something
    security_negotiation('3_7');
    return;
}

sub vnc_version_3_8 {
#TODO: return security_type negotiated and then do something
    security_negotiation('3_8');
    return;
}


sub vnc_version_unk {
    # version is unknown and unreliable
    logit(
        {
            priority => LOG_WARNING,
            message  => "unsupported client vnc_version_unk; $connection"
        }
    );
    exit 0;

    return;
}

sub client_init {
    my $shared_flag; 
    my $received = $client->recv( $shared_flag, 1 );

    if ( not defined $received ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "recv timeout client_init; $connection"
            }
        );
        exit 0;
    }

    if ( length $shared_flag == 0 ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "null shared_flag client_init; $connection"
            }
        );
        exit 0;
    }

    ($shared_flag) = unpack( 'C', $shared_flag );

    if ( $shared_flag == 0 ) {
        logit( { message => "client wants exclusive access; $connection" } );
    }
    else {
        logit( { message => "client wants shared access; $connection" } );
    }

    return;
}

sub security_negotation {
    my $version = shift or return;

    # bytes are: 3 types, none, vnc, apple remote desktop
    my $sectypes = pack( "N", 0x0301021E );

    my $sent = $client->send($sectypes);
    if ( not defined $sent ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "send failure vnc_version_$version; $connection"
            }
        );
        exit 0;
    }

    my $sectype; 
    my $received = $client->recv( $sectype, 1 );

    if ( not defined $received ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "recv timeout vnc_version_$version; $connection"
            }
        );
        exit 0;
    }

    if ( length $sectype == 0 ) {
        logit(
            {
                priority => LOG_WARNING,
                message  => "null sectype vnc_version_$version; $connection"
            }
        );
        exit 0;
    }
#TODO
    logit( { message => "client sectype $sectype; $connection" } );
    ($sectype) = unpack( 'C', $sectype );
    logit( { message => "client sectype $sectype (unpacked); $connection" } );
 
    return;
}
