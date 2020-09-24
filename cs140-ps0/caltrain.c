#include "pintos_thread.h"

struct station {
	// 火车上的空闲座位
	int seat_num;
	// 火车站正在等待的乘客
	int wait_passengers;
	// 上火车了的乘客
	int board_passengers;
	// 互斥锁
	struct lock *lock;
	// 火车座位坐满
	struct condition *cond_full;
	// 有新火车来到
	struct condition *cond_train;
};

void
station_init(struct station *station)
{
	station->seat_num = 0;
	station->wait_passengers = 0;
	station->board_passengers = 0;
	station->lock = malloc(sizeof(struct lock));
	station->cond_full = malloc(sizeof(struct condition));
	station->cond_train = malloc(sizeof(struct condition));
	lock_init(station->lock);
	cond_init(station->cond_full);
	cond_init(station->cond_train);
}

void
station_load_train(struct station *station, int count)
{
	lock_acquire(station->lock);
	station->seat_num += count;
	// 通知此station已经有座位了
	// 有座位了才通知释放锁
	while (station->seat_num > 0 && station->wait_passengers > 0) {
		cond_broadcast(station->cond_train, station->lock);
		// 全坐满了,可以出发了
		cond_wait(station->cond_full, station->lock);
	}
	// 出发后把座位重设为0,表示车已经开走了
	// 注意,没有乘客在等的时候会直接开走
	station->seat_num = 0;
	station->board_passengers = 0;
	lock_release(station->lock);
}

void
station_wait_for_train(struct station *station)
{
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
	++(station->wait_passengers);
	// 上车了要调用station_on_board才会坐下
	// 车上如果上满人了,则阻塞,等待新车来
	while (station->board_passengers == station->seat_num)
		cond_wait(station->cond_train, station->lock);
	--(station->wait_passengers);
	++(station->board_passengers);
	lock_release(station->lock);
}

void
station_on_board(struct station *station)
{
	// 乘客上车,将减少座位数量
	lock_acquire(station->lock);
	--(station->board_passengers);
	--(station->seat_num);
	// 没有座位或者所有等待的乘客都坐好了
	if (station->seat_num == 0 || station->board_passengers == 0)
		cond_signal(station->cond_full, station->lock);
	lock_release(station->lock);
}
