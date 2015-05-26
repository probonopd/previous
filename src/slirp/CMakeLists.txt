check_include_files(sys/filio.h HAVE_SYS_FILIO_H)
if(HAVE_SYS_FILIO_H)
	add_definitions(-DHAVE_SYS_FILIO_H)
endif(HAVE_SYS_FILIO_H)

check_include_files(sys/ioctl.h HAVE_SYS_IOCTL_H)
if(HAVE_SYS_IOCTL_H)
	add_definitions(-DHAVE_SYS_IOCTL_H)
endif(HAVE_SYS_IOCTL_H)

check_include_files(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
	add_definitions(-DHAVE_UNISTD_H)
endif(HAVE_UNISTD_H)

add_definitions(-DHAVE_STRERROR)

add_library(Slirp
            bootp.c cksum.c if.c ip_icmp.c ip_input.c ip_output.c 
            mbuf.c misc.c sbuf.c slirp.c slirpdebug.c socket.c 
            tcp_input.c tcp_output.c tcp_subr.c tcp_timer.c tftp.c udp.c)
