#include "pintos_thread.h"

struct station {
	int seat_num;
	struct lock *lock;
	struct condition *cond_empty;
};

void
station_init(struct station *station)
{
	station->seat_num = 0;
	lock_init(station->lock);
	cond_init(station->cond_empty);
}

void
station_load_train(struct station *station, int count)
{
	lock_acquire(station->lock);
	station->seat_num += count;
	// 通知此station已经有座位了
	// 有座位了才通知释放锁
	if (station->seat_num > 0)
		cond_broadcast(station->cond_empty, station->lock);
	lock_release(station->lock);
}

void
station_wait_for_train(struct station *station)
{
	// 等待cond_empty
	lock_acquire(station->lock);
	/**
	 * pthread_cond_wait的工作机制如下:
	 * 1. 阻塞等待条件变量cond满足
	 * 2. 解除已绑定的互斥锁
	 * 3. 当线程被唤醒时,pthread_cond_wait函数返回,该函数同时会解除线程阻塞并使线程重新申请绑定互斥锁
	 * 其中1, 2味原子操作,但是3表明线程唤醒后还需要重新绑定互斥锁
	 * 极有可能在线程B绑定互斥锁之前,线程A已执行了
	 * 获取互斥锁->修改条件变量->解除互斥锁，此时B获取到互斥锁，但是条件变量不能满足，还要继续阻塞等待
	 **/
	while (station->seat_num <= 0)
		cond_wait(station->cond_empty, station->lock);
	station_on_board(station);
	lock_release(station->lock);
}

void
station_on_board(struct station *station)
{
	--(station->seat_num); 
}
