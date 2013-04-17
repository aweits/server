/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// force a bad LSN during the forward scan.  recovery should fail.

#include "test.h"
#include "includes.h"
#if defined(HAVE_LIMITS_H)
# include <limits.h>
#endif
#if defined(HAVE_SYS_SYSLIMITS_H)
# include <sys/syslimits.h>
#endif

#define TESTDIR __SRCFILE__ ".dir"

static void recover_callback_at_turnaround(void *UU(arg)) {
    // change the LSN in the first log entry of log 2.  this will cause an LSN error during the forward scan.
    int r;
    char logname[PATH_MAX];
    sprintf(logname, "%s/log000000000002.tokulog%d", TESTDIR, TOKU_LOG_VERSION);
    FILE *f = fopen(logname, "r+b"); assert(f);
    r = fseek(f, 025, SEEK_SET); assert(r == 0);
    char c = 100;
    size_t n = fwrite(&c, sizeof c, 1, f); assert(n == sizeof c);
    r = fclose(f); assert(r == 0);
}

static int 
run_test(void) {
    int r;

    // setup the test dir
    r = system("rm -rf " TESTDIR);
    CKERR(r);
    r = toku_os_mkdir(TESTDIR, S_IRWXU); assert(r == 0);

    // log 1 has the checkpoint
    TOKULOGGER logger;
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);

    LSN beginlsn;
    toku_log_begin_checkpoint(logger, &beginlsn, true, 0, 0);
    toku_log_end_checkpoint(logger, NULL, true, beginlsn, 0, 0, 0);

    r = toku_logger_close(&logger); assert(r == 0);

    // log 2 has hello
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);

    BYTESTRING hello  = { (uint32_t) strlen("hello"), (char *) "hello" };
    toku_log_comment(logger, NULL, true, 0, hello);

    r = toku_logger_close(&logger); assert(r == 0);

    // log 3 has there
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);

    BYTESTRING there  = { (uint32_t) strlen("there"), (char *) "there" };
    toku_log_comment(logger, NULL, true, 0, there);

    r = toku_logger_close(&logger); assert(r == 0);

    // redirect stderr
    int devnul = open(DEV_NULL_FILE, O_WRONLY);
    assert(devnul>=0);
    r = toku_dup2(devnul, fileno(stderr)); 	    assert(r==fileno(stderr));
    r = close(devnul);                      assert(r==0);

    // delete log 2 at the turnaround to force
    toku_recover_set_callback(recover_callback_at_turnaround, NULL);

    // run recovery
    r = tokudb_recover(NULL,
		       NULL_prepared_txn_callback,
		       NULL_keep_cachetable_callback,
		       NULL_logger, TESTDIR, TESTDIR, 0, 0, 0, NULL, 0); 
    assert(r != 0);

    r = system("rm -rf " TESTDIR);
    CKERR(r);

    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}