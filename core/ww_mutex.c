#include "core/ww_mutex.h"

/*
 * A lightweight ww_mutex implemenentation for backwards compat, based
 * on the old TTM reservation implementation. There are less tests and
 * There may, in theory, be starvation in context-free locks, and there
 * may be a slight performance loss in contended cases, since none of the
 * mutex subsystem contended case optimizations are present.
 *
 * The API and lockdep annotation is Copyright (C) 2013, Canonical Ltd. (GPL)
 * The implementation itself, except the lockdep annotation,
 * should be considered available under the same license as TTM.
 */

static bool __wwl_mutex_lock(struct ww_mutex *lock,
			     struct ww_acquire_ctx *ctx,
			     int *err)
{

	WARN_ON(ctx && ctx->contending_lock &&
		ctx->contending_lock != lock);

#ifdef CONFIG_DEBUG_WW_MUTEX_SLOWPATH
	if (ctx && ctx->count == ctx->deadlock_injection) {
		ctx->deadlock_injection *= 2;
		ctx->count = 0;
		if (ctx->contending_lock != lock) {
			ctx->contending_lock = lock;
			*err = -EDEADLK;
			return true;
		}
	}
#endif
	spin_lock(&lock->my_class->lock);
	if (!lock->held) {
		lock->held = true;
		lock->holder = ctx;
		spin_unlock(&lock->my_class->lock);
#ifdef CONFIG_DEBUG_WW_MUTEX_SLOWPATH
		if (ctx)
			ctx->count++;
#endif
		if (ctx && ctx->contending_lock == lock)
			ctx->contending_lock = NULL;
		*err = 0;
		return true;
	} else if (ctx && lock->holder == ctx) {
		spin_unlock(&lock->my_class->lock);
		*err = -EALREADY;
		return true;
	} else if (ctx && ctx->contending_lock != lock &&
		   lock->holder &&
		   lock->holder->ticket < ctx->ticket) {
		spin_unlock(&lock->my_class->lock);
		ctx->contending_lock = lock;
		*err = -EDEADLK;
		return true;
	}
	spin_unlock(&lock->my_class->lock);
	return false;
}

int ww_mutex_trylock(struct ww_mutex *lock)
{
	bool acquired;
	int err = -EBUSY;

	lock_acquire(&lock->base.dep_map, 0, 1, 0, 1, NULL, _RET_IP_);
	acquired = __wwl_mutex_lock(lock, NULL, &err);
	if (acquired)
		lock_acquired(&lock->base.dep_map, _RET_IP_);
	else
		lock_release(&lock->base.dep_map, 0, _RET_IP_);
	return acquired ? 1 : 0;
}

int ww_mutex_lock_interruptible(struct ww_mutex *lock,
				struct ww_acquire_ctx *ctx)
{
	int err = -EINTR;

	lock_acquire(&lock->base.dep_map, 0, 0, 0, 1,
		     ctx ? &ctx->dep_map : NULL, _RET_IP_);

	if (!__wwl_mutex_lock(lock, ctx, &err)) {
		lock_contended(&lock->base.dep_map, _RET_IP_);
		wait_event_interruptible(lock->queue,
					 __wwl_mutex_lock(lock, ctx, &err));
	}

	if (!err) {
		lock_acquired(&lock->base.dep_map, _RET_IP_);
	} else {
		lock_contended(&lock->base.dep_map, _RET_IP_);
		lock_release(&lock->base.dep_map, 0, _RET_IP_);
	}

	return err;
}

int ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
	int err = -EBUSY;

	lock_acquire(&lock->base.dep_map, 0, 0, 0, 1,
		     ctx ? &ctx->dep_map : NULL, _RET_IP_);
	if (!__wwl_mutex_lock(lock, ctx, &err)) {
		lock_contended(&lock->base.dep_map, _RET_IP_);
		wait_event(lock->queue, __wwl_mutex_lock(lock, ctx, &err));
	}

	if (!err) {
		lock_acquired(&lock->base.dep_map, _RET_IP_);
	} else {
		lock_contended(&lock->base.dep_map, _RET_IP_);
		lock_release(&lock->base.dep_map, 0, _RET_IP_);
	}

	WARN_ON_ONCE(err == -EBUSY);
	return err;
}

void ww_mutex_unlock(struct ww_mutex *lock)
{
	lock_release(&lock->base.dep_map, 0, _RET_IP_);
	spin_lock(&lock->my_class->lock);
	lock->held = false;
	lock->holder = NULL;
	wake_up_all(&lock->queue);
	spin_unlock(&lock->my_class->lock);
}
