# dataplane-vnc
DataPlane VNC server daemon

dataplane-vnc is a minimal VNC server daemon.  it listens on TCP port
5900 by default (IPv4 as well as IPv6 if available).  It logs all
incoming VNC connection attempts and initial RFB protocol message
exchanges to syslog.
