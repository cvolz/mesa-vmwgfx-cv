/*
 * Wound/Wait Mutexes: blocking mutual exclusion locks with deadlock avoidance
 *
 * Original mutex implementation started by Ingo Molnar:
 *
 *  Copyright (C) 2004, 2005, 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *
 * Wound/wait implementation:
 *  Copyright (C) 2013 Canonical Ltd.
 *
 * Wound/ wait lightweight:
 *  Copyright (C) 2016 VMWare Inc.
 *
 * This file contains the main data structure and API definitions.
 */

#ifndef _WW_MUTEX_H_
#define _WW_MUTEX_H_

#include <linux/kernel.h>
#include <asm/atomic.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#define LREAD_ONCE(_x) (*(volatile typeof(_x) *)&(_x))

struct ww_class {
	spinlock_t lock;
	atomic64_t seqno;
	struct lock_class_key acquire_key;
	struct lock_class_key mutex_key;
	const char *acquire_name;
	const char *mutex_name;
};

struct ww_acquire_ctx {
	struct ww_class *my_class;
	u64 ticket;
	struct ww_mutex *contending_lock;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map dep_map;
#endif
#ifdef CONFIG_DEBUG_WW_MUTEX_SLOWPATH
	u64 deadlock_injection;
	u64 count;
#endif
};

struct ww_mutex {
	struct ww_class *my_class;
	bool held;
	struct ww_acquire_ctx *holder;
	wait_queue_head_t queue;
	struct {
#ifdef CONFIG_DEBUG_LOCK_ALLOC
		struct lockdep_map dep_map;
#endif
	} base;
};

#define __WW_CLASS_INITIALIZER(ww_class) {				\
		.seqno = ATOMIC64_INIT(0),				\
		.lock = __SPIN_LOCK_UNLOCKED(ww_class.lock),	\
		.acquire_name = #ww_class "_acquire",		\
		.mutex_name = #ww_class "_mutex"	\
			}				\

#define DEFINE_WW_CLASS(classname) \
	struct ww_class classname = __WW_CLASS_INITIALIZER(classname)

static inline void ww_acquire_init(struct ww_acquire_ctx *ctx,
				   struct ww_class *ww_class)
{
	ctx->my_class = ww_class;
	ctx->ticket = atomic64_inc_return(&ww_class->seqno);
	ctx->contending_lock = NULL;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	lockdep_init_map(&ctx->dep_map, ww_class->acquire_name,
			 &ww_class->acquire_key, 0);
	lock_acquire(&ctx->dep_map, 0, 0, 0, 1, NULL, _RET_IP_);
	lock_acquired(&ctx->dep_map, _RET_IP_);
#endif
#ifdef CONFIG_DEBUG_WW_MUTEX_SLOWPATH
	ctx->deadlock_injection = 1L;
	ctx->count = 0L;
#endif
}

static inline void ww_acquire_done(struct ww_acquire_ctx *ctx)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	lockdep_assert_held(ctx);
#endif
}

static inline void ww_acquire_fini(struct ww_acquire_ctx *ctx)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	lock_release(&ctx->dep_map, 0, _RET_IP_);
#endif
}

static inline void ww_mutex_init(struct ww_mutex *lock,
				 struct ww_class *ww_class)
{
  lock->my_class = ww_class;
  lock->held = 0;
  lock->holder = NULL;
  init_waitqueue_head(&lock->queue);
#ifdef CONFIG_DEBUG_LOCK_ALLOC
  lockdep_init_map(&lock->base.dep_map, ww_class->mutex_name,
		   &ww_class->mutex_key, 0);
#endif
}

static inline bool ww_mutex_is_locked(struct ww_mutex *lock)
{
	return LREAD_ONCE(lock->held);
}

int ww_mutex_trylock(struct ww_mutex *lock);

int ww_mutex_lock_interruptible(struct ww_mutex *lock,
				struct ww_acquire_ctx *ctx);

#define ww_mutex_lock_slow_interruptible ww_mutex_lock_interruptible

int ww_mutex_lock(struct ww_mutex *lock,
		  struct ww_acquire_ctx *ctx);

#define ww_mutex_lock_slow ww_mutex_lock

void ww_mutex_unlock(struct ww_mutex *lock);

static inline void ww_mutex_destroy(struct ww_mutex *lock)
{
}
#endif
