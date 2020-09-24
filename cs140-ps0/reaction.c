#include "pintos_thread.h"

// Forward declaration. This function is implemented in reaction-runner.c,
// but you needn't care what it does. Just be sure it's called when
// appropriate within reaction_o()/reaction_h().
void make_water();

struct reaction {
	int h_num;
	int o_num;
	struct lock *lock;
	struct condition *cond_not_ready;
};

void
reaction_init(struct reaction *reaction)
{
	reaction->h_num = 0;
	reaction->o_num = 0;
	lock_acquire(reaction->lock);
	cond_init(reaction->cond_not_ready);
}

void
reaction_h(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	++(reaction->h_num);
	while (reaction->o_num < 2)
		cond_wait(reaction->cond_not_ready, reaction->lock);
	reaction->o_num -= 2;
	--reaction->h_num;
	if (reaction->h_num >= 0) {
		cond_signal(reaction->cond_not_ready, reaction->lock);
		make_water();
	}
	lock_release(reaction->lock);
}

void
reaction_o(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	++(reaction->o_num);
	while (reaction->h_num < 1)
		cond_wait(reaction->cond_not_ready, reaction->lock);
	reaction->o_num -= 2;
	--reaction->h_num;
	if (reaction->o_num >= 0) {
		cond_signal(reaction->cond_not_ready, reaction->lock);
		make_water();
	}
	lock_release(reaction->lock);
}
