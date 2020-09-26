#include "pintos_thread.h"

// Forward declaration. This function is implemented in reaction-runner.c,
// but you needn't care what it does. Just be sure it's called when
// appropriate within reaction_o()/reaction_h().
void make_water();

struct reaction {
	// h原子个数
	int h_num;
	// 更改h原子个数所用的互斥锁
	struct lock *lock;
	// h原子为2时的条件变量
	struct condition *h_cond_ready;
	// o原子为1时的条件变量
	struct condition *o_cond_ready;
};

void
reaction_init(struct reaction *reaction)
{
	reaction->h_num = 0;
	reaction->h_cond_ready = malloc(sizeof(struct condition));
	reaction->o_cond_ready = malloc(sizeof(struct condition));
	reaction->lock = malloc(sizeof(struct lock));
	lock_init(reaction->lock);
	cond_init(reaction->h_cond_ready);
	cond_init(reaction->o_cond_ready);
}

void
reaction_h(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	++(reaction->h_num);
	// 现有的h原子数量大于等于2,通知正在等待h原子的o原子
	if (reaction->h_num >= 2)
		cond_signal(reaction->h_cond_ready, reaction->lock);
	// 等待o原子那边反应完成这里才返回
	// 因为题目要求所有的reaction_h都要返回,所以一个比较好的方法是让o原子等待两个h
	// 原子都就位,在reaction_o中进行化学反应后分别通知这两个h线程
	// 如果让h线程等待o,可能反应一次只能让一半的h线程返回(虽然也能产生water)
	cond_wait(reaction->o_cond_ready, reaction->lock);
	lock_release(reaction->lock);
}

void
reaction_o(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	// 等到来了两个h,才能进行化学反应
	while (reaction->h_num < 2)
		cond_wait(reaction->h_cond_ready, reaction->lock);
	reaction->h_num -= 2;
	make_water();
	// 分别通知两个h线程化学反应已经结束,让这两个h线程都能返回
	cond_signal(reaction->o_cond_ready, reaction->lock);
	cond_signal(reaction->o_cond_ready, reaction->lock);
	lock_release(reaction->lock);
}
