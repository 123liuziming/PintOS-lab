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
	cond_wait(station->cond_empty, station->lock);
	station_on_board(station);
	lock_release(station->lock);
}

void
station_on_board(struct station *station)
{
	--(station->seat_num); 
}
