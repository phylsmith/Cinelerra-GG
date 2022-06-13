/* Cancel a thread.
   Copyright (C) 2002 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */
#include <stdio.h>
#if defined(__TERMUX__)

#include <pthread.h>

#include <pt-internal.h>

#include <errno.h>

int
pthread_cancel (pthread_t t)
{
  int err = 0;
  struct pthread_internal_t *p = (struct pthread_internal_t*) t;
	
	pthread_init();
	
  pthread_mutex_lock (&p->cancel_lock);
  if (p->attr.flags & PTHREAD_ATTR_FLAG_CANCEL_PENDING)
    {
      pthread_mutex_unlock (&p->cancel_lock);
      return 0;
    }
    
  p->attr.flags |= PTHREAD_ATTR_FLAG_CANCEL_PENDING;

  if (!(p->attr.flags & PTHREAD_ATTR_FLAG_CANCEL_ENABLE))
    {
      pthread_mutex_unlock (&p->cancel_lock);
      return 0;
    }

  if (p->attr.flags & PTHREAD_ATTR_FLAG_CANCEL_ASYNCRONOUS) {
		pthread_mutex_unlock (&p->cancel_lock);
    err = __pthread_do_cancel (p);
	} else {
		// DEFERRED CANCEL NOT IMPLEMENTED YET
		pthread_mutex_unlock (&p->cancel_lock);
	}

  return err;
}
/* Cancel a thread. */

#include <pthread.h>

#include <pt-internal.h>

static void
call_exit (void)
{
  pthread_exit (0);
}

int
__pthread_do_cancel (struct pthread_internal_t *p)
{
	
	if(p == (struct pthread_internal_t *)pthread_self())
    call_exit ();
  else if(p->attr.flags & PTHREAD_ATTR_FLAG_CANCEL_HANDLER)
    pthread_kill((pthread_t)p, SIGRTMIN);
	else
		pthread_kill((pthread_t)p, SIGTERM);

  return 0;
}
/* Init a thread. */

#include <pthread.h>
#include <bthread.h>
#include <pt-internal.h>
#include <signal.h>

void pthread_cancel_handler(int signum) {
	pthread_exit(0);
}

void pthread_init(void) {
	struct sigaction sa;
	struct pthread_internal_t * p = (struct pthread_internal_t *)pthread_self();
	
	if(p->attr.flags & PTHREAD_ATTR_FLAG_CANCEL_HANDLER)
		return;
	
	// set thread status as pthread_create should do.
	// ASYNCROUNOUS is not set, see pthread_setcancelstate(3)
	p->attr.flags |= PTHREAD_ATTR_FLAG_CANCEL_HANDLER|PTHREAD_ATTR_FLAG_CANCEL_ENABLE;
	
	sa.sa_handler = pthread_cancel_handler;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	
	sigaction(SIGRTMIN, &sa, NULL);
}
/* Set the cancel state for the calling thread.  */

#include <pthread.h>
#include <bthread.h>
#include <pt-internal.h>
#include <errno.h>

int
pthread_setcancelstate (int state, int *oldstate)
{
  struct pthread_internal_t *p = (struct pthread_internal_t*)pthread_self();
	int newflags;

	pthread_init();
	
  switch (state)
    {
    default:
      return EINVAL;
    case PTHREAD_CANCEL_ENABLE:
    case PTHREAD_CANCEL_DISABLE:
      break;
    }

  pthread_mutex_lock (&p->cancel_lock);
  if (oldstate)
    *oldstate = p->attr.flags & PTHREAD_ATTR_FLAG_CANCEL_ENABLE;
  
	if(state == PTHREAD_ATTR_FLAG_CANCEL_ENABLE)
		p->attr.flags |= PTHREAD_ATTR_FLAG_CANCEL_ENABLE;
	else
		p->attr.flags &= ~PTHREAD_ATTR_FLAG_CANCEL_ENABLE;
	newflags=p->attr.flags;
  pthread_mutex_unlock (&p->cancel_lock);

	if((newflags & PTHREAD_ATTR_FLAG_CANCEL_PENDING) && (newflags & PTHREAD_ATTR_FLAG_CANCEL_ENABLE) && (newflags & PTHREAD_ATTR_FLAG_CANCEL_ASYNCRONOUS))
		__pthread_do_cancel(p);
	
  return 0;
}
/*   */

#include <pthread.h>
#include <bthread.h>
#include <pt-internal.h>
#include <errno.h>

int
pthread_setcanceltype (int type, int *oldtype)
{
  struct pthread_internal_t *p = (struct pthread_internal_t*)pthread_self();
	int newflags;
	
	pthread_init();
	
  switch (type)
    {
    default:
      return EINVAL;
    case PTHREAD_CANCEL_DEFERRED:
    case PTHREAD_CANCEL_ASYNCHRONOUS:
      break;
    }

  pthread_mutex_lock (&p->cancel_lock);
  if (oldtype)
    *oldtype = p->attr.flags & PTHREAD_ATTR_FLAG_CANCEL_ASYNCRONOUS;
	
	if(type == PTHREAD_CANCEL_ASYNCHRONOUS)
		p->attr.flags |= PTHREAD_ATTR_FLAG_CANCEL_ASYNCRONOUS;
	else
		p->attr.flags &= ~PTHREAD_ATTR_FLAG_CANCEL_ASYNCRONOUS;
	newflags=p->attr.flags;
  pthread_mutex_unlock (&p->cancel_lock);

	if((newflags & PTHREAD_ATTR_FLAG_CANCEL_PENDING) && (newflags & PTHREAD_ATTR_FLAG_CANCEL_ENABLE) && (newflags & PTHREAD_ATTR_FLAG_CANCEL_ASYNCRONOUS))
		__pthread_do_cancel(p);
	
  return 0;
}
/* Add an explicit cancelation point.  */

#include <pthread.h>
#include <bthread.h>
#include <pt-internal.h>

void
pthread_testcancel (void)
{
  struct pthread_internal_t *p = (struct pthread_internal_t*)pthread_self();
  int cancelled;
	
	pthread_init();

  pthread_mutex_lock (&p->cancel_lock);
  cancelled = (p->attr.flags & PTHREAD_ATTR_FLAG_CANCEL_ENABLE) && (p->attr.flags & PTHREAD_ATTR_FLAG_CANCEL_PENDING);
  pthread_mutex_unlock (&p->cancel_lock);

  if (cancelled)
    pthread_exit (PTHREAD_CANCELED);
		
}
#endif
