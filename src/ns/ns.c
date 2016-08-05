/* 
 * Copyright (c) 2015-2016, Gregory M. Kurtzer. All rights reserved.
 * 
 * “Singularity” Copyright (c) 2016, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * This software is licensed under a customized 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such,
 * the U.S. Government has been granted for itself and others acting on its
 * behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software
 * to reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so. 
 * 
*/

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>

#include "file.h"
#include "util.h"
#include "message.h"
#include "config_parser.h"



int singularity_ns_init(void) {

    printf("Hello from ns_init()\n");

    return(0);
}


int singularity_ns_pid_unshare(void) {
    config_rewind();
#ifdef NS_CLONE_NEWPID
    message(DEBUG, "Using PID namespace: CLONE_NEWPID\n");
    if ( ( getenv("SINGULARITY_NO_NAMESPACE_PID") == NULL ) && // Flawfinder: ignore (only checking for existance of envar)
            ( config_get_key_bool("allow pid ns", 1) > 0 ) ) {
        unsetenv("SINGULARITY_NO_NAMESPACE_PID");
        message(DEBUG, "Virtualizing PID namespace\n");
        if ( unshare(CLONE_NEWPID) < 0 ) {
            message(ERROR, "Could not virtualize PID namespace: %s\n", strerror(errno));
            ABORT(255);
        }
    } else {
        message(VERBOSE, "Not virtualizing PID namespace\n");
    }
#else
#ifdef NS_CLONE_PID
    message(DEBUG, "Using PID namespace: CLONE_PID\n");
    if ( ( getenv("SINGULARITY_NO_NAMESPACE_PID") == NULL ) && // Flawfinder: ignore (only checking for existance of envar)
            ( config_get_key_bool("allow pid ns", 1) > 0 ) ) {
        unsetenv("SINGULARITY_NO_NAMESPACE_PID");
        message(DEBUG, "Virtualizing PID namespace\n");
        if ( unshare(CLONE_NEWPID) < 0 ) {
            message(ERROR, "Could not virtualize PID namespace: %s\n", strerror(errno));
            ABORT(255);
        }
    } else {
        message(VERBOSE, "Not virtualizing PID namespace\n");
    }
#endif
    message(VERBOSE, "Skipping PID namespace creation, support not available\n");
#endif
    return(0);
}


int singularity_ns_mnt_unshare(void) {
#ifdef NS_CLONE_FS
    message(DEBUG, "Virtualizing FS namespace\n");
    if ( unshare(CLONE_FS) < 0 ) {
        message(ERROR, "Could not virtualize file system namespace: %s\n", strerror(errno));
        ABORT(255);
    }
#endif

    message(DEBUG, "Virtualizing mount namespace\n");
    if ( unshare(CLONE_NEWNS) < 0 ) {
        message(ERROR, "Could not virtualize mount namespace: %s\n", strerror(errno));
        ABORT(255);
    }


    config_rewind();
    int slave = config_get_key_bool("mount slave", 0);
    // Privatize the mount namespaces
    //
#ifdef SINGULARITY_MS_SLAVE
    message(DEBUG, "Making mounts %s\n", (slave ? "slave" : "private"));
    if ( mount(NULL, "/", NULL, (slave ? MS_SLAVE : MS_PRIVATE)|MS_REC, NULL) < 0 ) {
        message(ERROR, "Could not make mountspaces %s: %s\n", (slave ? "slave" : "private"), strerror(errno));
        ABORT(255);
    }
#else
    if ( slave > 0 ) {
        message(WARNING, "Requested option 'mount slave' is not available on this host, using private\n");
    }
    message(DEBUG, "Making mounts private\n");
    if ( mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) < 0 ) {
        message(ERROR, "Could not make mountspaces %s: %s\n", (slave ? "slave" : "private"), strerror(errno));
        ABORT(255);
    }
#endif

    return(0);
}



int singularity_ns_join(pid_t attach_pid) {
#ifdef NO_SETNS
    message(ERROR, "This host does not support joining existing name spaces\n");
    ABORT(1);
#else
    char *nsjoin_pid = (char *)malloc(64);
    char *nsjoin_mnt = (char *)malloc(64);

    snprintf(nsjoin_pid, 64, "/proc/%d/ns/pid", attach_pid); // Flawfinder: ignore
    snprintf(nsjoin_mnt, 64, "/proc/%d/ns/mnt", attach_pid); // Flawfinder: ignore

    if ( is_file(nsjoin_pid) == 0 ) {
        message(DEBUG, "Connecting to existing PID namespace\n");
        int fd = open(nsjoin_pid, O_RDONLY); // Flawfinder: ignore
        if ( setns(fd, CLONE_NEWPID) < 0 ) {
            message(ERROR, "Could not join existing PID namespace: %s\n", strerror(errno));
            ABORT(255);
        }
        close(fd);

    } else {
        message(ERROR, "Could not identify PID namespace: %s\n", nsjoin_pid);
        ABORT(255);
    }

    if ( is_file(nsjoin_mnt) == 0 ) {
        message(DEBUG, "Connecting to existing mount namespace\n");
        int fd = open(nsjoin_mnt, O_RDONLY); // Flawfinder: ignore
        if ( setns(fd, CLONE_NEWNS) < 0 ) {
            message(ERROR, "Could not join existing mount namespace: %s\n", strerror(errno));
            ABORT(255);
        }
        close(fd);

    } else {
        message(ERROR, "Could not identify mount namespace: %s\n", nsjoin_mnt);
        ABORT(255);
    }
#endif
    return(0);
}
