/*
 * Copyright (C) 2006 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _XENO_POSIX_SIGNAL_H
#define _XENO_POSIX_SIGNAL_H

#ifdef __KERNEL__

#include <linux/signal.h>

/* These are not defined in kernel-space headers. */
#define sa_sigaction sa_handler
typedef void (*sighandler_t) (int sig);
typedef unsigned long sig_atomic_t;

#undef sigemptyset
#undef sigfillset
#undef sigaddset
#undef sigdelset
#undef sigismember
#undef sigaction
#undef sigqueue
#undef SIGRTMIN
#undef SIGRTMAX

#define sigaction(sig, action, old) cobalt_sigaction(sig, action, old)
#define sigemptyset cobalt_sigemptyset
#define sigfillset cobalt_sigfillset
#define sigaddset cobalt_sigaddset
#define sigdelset cobalt_sigdelset
#define sigismember cobalt_sigismember

#define SIGRTMIN 33
#define SIGRTMAX 64

struct cobalt_thread;

#ifdef __cplusplus
extern "C" {
#endif

int sigemptyset(sigset_t *set);

int sigfillset(sigset_t *set);

int sigaddset(sigset_t *set,
	      int signum);

int sigdelset(sigset_t *set,
	      int signum);

int sigismember(const sigset_t *set,
		int signum);

int pthread_kill(struct cobalt_thread *thread,
		 int sig);

int pthread_sigmask(int how,
		    const sigset_t *set,
		    sigset_t *oset);

int sigaction(int sig,
	      const struct sigaction *action,
	      struct sigaction *old);

int sigpending(sigset_t *set);

int sigwait(const sigset_t *set,
	    int *sig);

/* Real-time signals. */
int sigwaitinfo(const sigset_t *__restrict__ set,
		siginfo_t *__restrict__ info);

int sigtimedwait(const sigset_t *__restrict__ user_set,
		 siginfo_t *__restrict__ info,
		 const struct timespec *__restrict__ timeout);

int pthread_sigqueue_np (struct cobalt_thread *thread, int sig, union sigval value);

#ifdef __cplusplus
}
#endif

#else /* !__KERNEL__ */

#include_next <signal.h>
/*
 * In case <signal.h> is included as a side effect of an __need*
 * macro, include it a second time to get all definitions.
 */
#include_next <signal.h>

#endif /* !__KERNEL__ */

/*
 * Those are pseudo-signals only available with pthread_kill() to
 * suspend/resume/unblock threads synchronously, force them out of
 * primary mode or even demote them to the SCHED_OTHER class via the
 * low-level nucleus interface. Can't block those signals, queue them,
 * or even set them in a sigset. Those are nasty, strictly anti-POSIX
 * things; we do provide them nevertheless only because we are mean
 * people doing harmful code for no valid reason. Can't go against
 * your nature, right?  Nah... (this said, don't blame us for POSIX,
 * we are not _that_ mean).
 */
#define SIGSUSP (SIGRTMAX + 1)
#define SIGRESM (SIGRTMAX + 2)
#define SIGRELS (SIGRTMAX + 3)
#define SIGKICK (SIGRTMAX + 4)
#define SIGDEMT (SIGRTMAX + 5)

#endif /* _XENO_POSIX_SIGNAL_H */
