/*
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <scif.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

void parse_cmd_args(int argc, char *argv[]);
void start_daemon(void);
void *hostmpssd_mon(void *arg);
void *micctrl_mon(void *arg);
void setsighandlers(void);
void mpsslog(char *format, ...);

pid_t start_pid = 0;
scif_epd_t send_ep;
scif_epd_t recv_ep;

#define MPSSVAR_DIR "/var/mpss"
#define MPSSCOOKIES_DIR MPSSVAR_DIR "/cookies"
#define LOGFILE_NAME "/var/log/mpssd"
FILE *logfp = NULL;

#define USER_NAME_BUFSIZE 65	// other commands such as passwd will accept up to 64 so use this
				// value internally for card side mpssd.

#define COOKIE_MAX_SIZE	1024	// Probably only needs to be 8 bytes

int
main(int argc, char *argv[])
{
	pid_t pid;

	parse_cmd_args(argc, argv);
	setsighandlers();

	if (logfp != stderr) {
		if ((logfp = fopen(LOGFILE_NAME, "a")) == NULL) {
			fprintf(stderr, "cannot open logfile '%s'\n", LOGFILE_NAME);
			exit(EBADF);
		}
	}

	if (logfp == stderr) {
		start_daemon();
	} else {
		start_pid = getpid();
		switch ((pid = fork())) {
		case -1:
			fprintf(stderr, "cannot fork: %s\n", strerror(errno));
			exit(ENOEXEC);
		case 0:
			start_daemon();
		}
	}

	exit(0);
}

void
parse_cmd_args(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "l")) != -1) {
		switch(c) {
		case 'l':		// Local mode - don't fork and log to screen
			logfp = stderr;
			break;
		default:
			fprintf(stderr, "Unknown argument %c\n", c);
			exit(EINVAL);
		}
	}

	if (optind != argc) {
		fprintf(stderr, "Uknown argement '%s'\n", optarg);
		exit(EINVAL);
	}
}

void
mpsslog(char *format, ...)
{
	va_list args;
	char buffer[4096];
	time_t t;
	char *ts;

	if (logfp == NULL)
		return;

	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	time(&t);
	ts = ctime(&t);
	ts[strlen(ts) - 1] = '\0';
	fprintf(logfp, "%s: %s", ts, buffer);
	fflush(logfp);
}

#define MONITOR_START		1
#define MONITOR_START_ACK	2
#define MONITOR_START_NACK	3
#define REQ_CREDENTIAL		4
#define REQ_CREDENTIAL_ACK	5
#define REQ_CREDENTIAL_NACK	6
#define MONITOR_STOPPING	7

uint16_t nodes[128];
uint16_t self;

void
start_daemon(void)
{
	struct scif_portID sendID = {0, MPSSD_MONRECV};
	struct scif_portID recvID;
	unsigned int proto = MONITOR_START;
	unsigned int ack;
	scif_epd_t lep;
	pthread_t hostmpssd_pth;
	pthread_t micctrl_pth;
	void *joinval;
	int err;
	uint16_t send_port;
	uint16_t remote_port = 0;

	mpsslog("[Start] Daemon starting\n");
	scif_get_nodeIDs(nodes, 128, &self);

	if ((lep = scif_open()) < 0) {
		mpsslog("[Start] Cannot open SCIF listen port: %s\n", strerror(errno));
		exit(errno);
	}

	if (scif_bind(lep, MPSSD_MONSEND) < 0) {
		mpsslog("[Start] Cannot bind to monsitor send SCIF port: %s\n", strerror(errno));
		exit(errno);
	}

	if (scif_listen(lep, 16) < 0) {
		mpsslog("[Start] Set Listen on SCIF port fail: %s\n", strerror(errno));
		exit(errno);
	}

	while (1) {
		if ((send_ep = scif_open()) < 0) {
			mpsslog("[Start] Failed to open SCIF: %s\n", strerror(errno));
			exit(errno);
		}

		while ((send_port = scif_connect(send_ep, &sendID)) < 0) {
			mpsslog("[Start] Failed to connect to mpssd monitor thread on host: %s\n", strerror(errno));
			mpsslog("[Start] Retrying connect to host in 5 seconds\n");
			sleep(5);
		}

		if (scif_send(send_ep, &proto, sizeof(proto), SCIF_SEND_BLOCK) < sizeof(proto)) {
			mpsslog("Failed to send start packet to host: %s\n", strerror(errno));
			goto close_send_and_retry_in_5;
		}

		while (scif_accept(lep, &recvID, &recv_ep, SCIF_ACCEPT_SYNC) < 0) {
			if (errno != EINTR) {
				mpsslog("[Start] Wait scif_accept() failed: %s\n", strerror(errno));
				goto close_all_and_retry_in_5;
			}
		}

		// First we tell whomever connected to us, what local port number
		// we use to talk to host mpssd. Connections are already established, so this
		// information is not secret, nor possible to act upon.
		if (scif_send(recv_ep, &send_port, sizeof(send_port), SCIF_SEND_BLOCK) < 0) {
			mpsslog("[Start] Monitor start handshake error: %s\n", strerror(errno));
			goto close_all_and_retry_in_5;
		}

		// Next, we expect mpssd (send_ep is reliable) to tell us which port
		// number it uses to talk to us. This should be remote port number of
		// recv_ep, if it is, we know that recv_ep belongs to mpssd and is safe to use.
		if (scif_recv(send_ep, &remote_port, sizeof(remote_port), SCIF_RECV_BLOCK) < 0) {
			mpsslog("[Start] Monitor start handshake error: %s\n", strerror(errno));
			goto close_all_and_retry_in_5;
		}

		// mpssd and client connecting to us are different entities.
		// Abort both connections.
		if (recvID.port != remote_port || sendID.node != recvID.node) {
			mpsslog("[Start] Failed to authenticate connection with mpssd\n");
			goto close_all_and_retry_in_5;
		}

		if (scif_recv(recv_ep, &ack, sizeof(ack), SCIF_RECV_BLOCK) < 0) {
			mpsslog("[Start] Monitor start ACK receive failed: %s\n", strerror(errno));
			goto close_all_and_retry_in_5;
		}


		if (ack != MONITOR_START_ACK) {
			mpsslog("[Start] Monitor start request not acked - value %s\n", ack);
			goto close_all_and_retry_in_5;
		}

		mpsslog("[Start] Connected to host mpssd success\n");

		if ((err = pthread_create(&hostmpssd_pth, NULL, hostmpssd_mon, NULL))) {
			mpsslog("[Start] Failed to create monitor thread: %d\n", strerror(err));
			goto close_all_and_retry_in_5;
		}

		if ((err = pthread_join(hostmpssd_pth, &joinval)))
			mpsslog("[Start] Failed join to monitor thread: %d\n", strerror(err));
		else
			mpsslog("[Start] Host connect exited - reconnect\n");

		if (scif_close(send_ep) < 0)
			mpsslog("[Start] Close main send end porint failed: %d\n", strerror(errno));

		if (scif_close(recv_ep) < 0)
			mpsslog("[Start] Close main receive end porint failed: %d\n", strerror(errno));

		continue;

close_all_and_retry_in_5:
		if (scif_close(send_ep) < 0)
			mpsslog("[Start] Close main send end porint failed: %d\n", strerror(errno));

close_send_and_retry_in_5:
		if (scif_close(recv_ep) < 0)
			mpsslog("[Start] Close main receive end porint failed: %d\n", strerror(errno));
		
		mpsslog("[Start] Retrying connect to host in 5 seconds\n");
		sleep(5);
	}
}

static int create_cookies_dir()
{
	const int owner_access = S_IRUSR | S_IWUSR | S_IXUSR;

	if (mkdir(MPSSVAR_DIR, owner_access)) {
		if (errno != EEXIST) {
			mpsslog("[CookieDir] Couldn't create " MPSSVAR_DIR " directory: %s\n", strerror(errno));
			return -1;
		} else {
			if (chmod(MPSSVAR_DIR, owner_access)) {
				mpsslog("[CookieDir] Couldn't modify permissions of " MPSSVAR_DIR " directory: %s\n", strerror(errno));
				return -1;
			}
		}
	}

	if (mkdir(MPSSCOOKIES_DIR, owner_access)) {
		if (errno != EEXIST) {
			mpsslog("[CookieDir] Couldn't create " MPSSCOOKIES_DIR " directory: %s\n", strerror(errno));
			return -1;
		} else {
			if (chmod(MPSSCOOKIES_DIR, owner_access)) {
				mpsslog("[CookieDir] Couldn't modify permissions of " MPSSVAR_DIR " directory: %s\n", strerror(errno));
				return -1;
			}
		}
	}

	return 0;
}


void *
hostmpssd_mon(void *arg)
{
	unsigned int proto;
	unsigned int retproto = REQ_CREDENTIAL_ACK;
	unsigned int jobid;
	unsigned int namelen;
	char username[USER_NAME_BUFSIZE];
	unsigned int cookielen;
	unsigned int len;
	char cookie[COOKIE_MAX_SIZE];
	char oldcookie[COOKIE_MAX_SIZE];
	char cookiename[PATH_MAX];
	struct passwd *pass;
	struct stat sbuf;
	int createcookie = TRUE;
	int fd;
	int err;

	while (1) {
		while ((err = scif_recv(recv_ep, &proto, sizeof(proto), SCIF_RECV_BLOCK)) < 0) {
			if (errno == ECONNRESET) {
				return (void *)-1;
			}
		}

		switch (proto) {
		case REQ_CREDENTIAL:
			memset(username, 0, sizeof(username));
			memset(cookie, 0, sizeof(cookie));

			if ((err = scif_recv(recv_ep, &jobid, sizeof(jobid), SCIF_RECV_BLOCK)) < 0) {
				mpsslog("[CredReq] Read jobid failed %s\n", strerror(errno));
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (err < sizeof(jobid)) {
				mpsslog("[CredReq] Read jobid failed size %d < %d\n", err, sizeof(jobid));
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if ((err = scif_recv(recv_ep, &namelen, sizeof(namelen), SCIF_RECV_BLOCK)) < 0) {
				mpsslog("[CredReq] Read namelen failed %s\n", strerror(errno));
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (err < sizeof(namelen)) {
				mpsslog("[CredReq] Read namelen failed size %d < %d\n", err, sizeof(namelen));
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (namelen > sizeof(username) -1) {
				mpsslog("[CredReq] Specified name length %d too long\n", namelen);
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if ((err = scif_recv(recv_ep, username, namelen, SCIF_RECV_BLOCK)) < 0) {
				mpsslog("[CredReq] Read username failed %s\n", strerror(errno));
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (err < namelen) {
				mpsslog("[CredReq] Read username failed size %d < %d\n", err, namelen);
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if ((err = scif_recv(recv_ep, &cookielen, sizeof(cookielen), SCIF_RECV_BLOCK)) < 0) {
				mpsslog("[CredReq] Read cookielen failed %s\n", strerror(errno));
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (err < sizeof(cookielen)) {
				mpsslog("[CredReq] Read namelen failed size %d < %d\n", err, sizeof(cookielen));
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (cookielen > sizeof(cookie) -1) {
				mpsslog("[CredReq] Specified cookie length %d too long\n", cookielen);
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if ((err = scif_recv(recv_ep, cookie, cookielen, SCIF_RECV_BLOCK)) < 0) {
				mpsslog("[CredReq] Read cookie failed %s\n", strerror(errno));
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (err < cookielen) {
				mpsslog("[CredReq] Read cookie failed size %d < %d\n", err, cookielen);
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			while ((pass = getpwent()) != NULL) {
				if (!strcmp(username, pass->pw_name)) {
					break;
				}
			}

			endpwent();

			if (pass == NULL) {
				mpsslog("[CredReq] Fail user '%s' does not exist\n", username);
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (snprintf(cookiename, PATH_MAX, MPSSCOOKIES_DIR "/%s", pass->pw_name) == PATH_MAX) {
				mpsslog("[CredReq] Cookie file name will exceed PATH_MAX(%d)\n", PATH_MAX);
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (create_cookies_dir() != 0) {
				mpsslog("[CredReq] Couldnt create " MPSSCOOKIES_DIR "\n");
				retproto = REQ_CREDENTIAL_NACK;
				goto sendresponse;
			}

			if (lstat(cookiename, &sbuf) == 0) {
				if (S_ISLNK(sbuf.st_mode)) {
					if (unlink(cookiename) < 0) {
						mpsslog("[CredReq] %s Cannot create: is a link and removal failed: %s\n",
							cookiename, strerror(errno));
						retproto = REQ_CREDENTIAL_NACK;
						goto sendresponse;

					}

					mpsslog("[CredReq] %s: Is a link - remove and recreate\n", cookiename);

				} else if (sbuf.st_nlink != 1) {
					if (unlink(cookiename) < 0) {
						mpsslog("[CredReq] %s Cannot create: has more than one hard link and "
								"removal failed: %s\n", cookiename, strerror(errno));
						retproto = REQ_CREDENTIAL_NACK;
						goto sendresponse;
					}

					mpsslog("[CredReq] %s: Too many hard links - remove and recreate\n", cookiename);

				} else {
					createcookie = FALSE;
				}
			}

			if (!createcookie) {
				if ((fd = open(cookiename, O_RDONLY)) < 0) {
					mpsslog("[CredReq] Failed to open %s for read: %s\n",
										cookiename, strerror(errno));
					retproto = REQ_CREDENTIAL_NACK;
					goto sendresponse;
				}

				len = read(fd, oldcookie, cookielen);

				if (close(fd) < 0)
					mpsslog("[CredReq] Failed to close cookie file %s: %s\n",
										cookiename, strerror(errno));

				if (len != cookielen) {
					if (unlink(cookiename) < 0) {
						mpsslog("[CredReq] Cannot create cookie file %s bad size and "
							"removal failed: %s\n", cookiename, strerror(errno));
						retproto = REQ_CREDENTIAL_NACK;
						goto sendresponse;
					}

					mpsslog("[CredReq] %s: Bad size remove and recreate\n", cookiename);
					createcookie = 1;
				} else if (memcmp(cookie, oldcookie, cookielen)) {
					if (unlink(cookiename) < 0) {
						mpsslog("[CredReq] Cannot create cookie file %s does not match and "
							"removal failed: %s\n", cookiename, strerror(errno));
						retproto = REQ_CREDENTIAL_NACK;
						goto sendresponse;
					}

					mpsslog("[CredReq] %s: Bad size remove and recreate\n", cookiename);
					createcookie = TRUE;
				}
			}

			if (createcookie) {
				if ((fd = open(cookiename, O_WRONLY|O_CREAT, 0400)) < 0) {
					mpsslog("[CredReq] Failed to open %s for write: %s\n",
										cookiename, strerror(errno));
					retproto = REQ_CREDENTIAL_NACK;
					goto sendresponse;
				}

				if (write(fd, cookie, cookielen) < cookielen) {
					mpsslog("[CredReq] Failed to write cookie to %s\n", cookiename);

					if (close(fd) < 0)
						mpsslog("[CredReq] Failed to close cookie file %s: %s\n",
										cookiename, strerror(errno));

					if (unlink(cookiename) < 0)
						mpsslog("[CredReq] Failed to remove old %s for write: %s\n",
										cookiename, strerror(errno));
					retproto = REQ_CREDENTIAL_NACK;
					goto sendresponse;
				}

				if (fchmod(fd, S_IRUSR) < 0)
					mpsslog("[CredReq] Failed to set file modes for cookie file %s: %s\n",
						cookiename, strerror(errno));

				if (close(fd) < 0)
					mpsslog("[CredReq] Failed to close cookie file %s: %s\n",
										cookiename, strerror(errno));
			}

sendresponse:
			createcookie = TRUE;
			if ((err = scif_send(send_ep, &retproto, sizeof(retproto), 0)) < sizeof(retproto)) {
				mpsslog("[CredReq] Send ack proto failed %s\n", strerror(errno));
			}

			if ((err = scif_send(send_ep, &jobid, sizeof(jobid), SCIF_SEND_BLOCK)) < sizeof(jobid)) {
				mpsslog("[CredReq] Send ack jobid failed %s\n", strerror(errno));
			}

			break;

		default:
			if (proto != -1)
				mpsslog("[HostMpssdMon] Illegal request %d\n", proto);
		}
	}
}

void
quit_handler(int sig, siginfo_t *siginfo, void *context)
{
	unsigned int proto = MONITOR_STOPPING;
	int err;

	if ((err = scif_send(send_ep, &proto, sizeof(proto), 0)) < 0) {
		mpsslog("[Quit] Send monitor stopping proto failed\n");
		exit(0);
	}

	if ((err = scif_send(send_ep, &self, sizeof(self), SCIF_SEND_BLOCK)) < sizeof(self)) {
		mpsslog("[Quit] Send monitor stopping node id failed\n");
		exit(0);
	}

	if ((err = scif_close(send_ep)) < 0) {
		mpsslog("[Quit] Close send end point failed %s\n", strerror(errno));
		exit(0);
	}

	exit(0);
}

void
setsighandlers(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = quit_handler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGHUP, &act, NULL);
}

