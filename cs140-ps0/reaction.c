#include "pintos_thread.h"

// Forward declaration. This function is implemented in reaction-runner.c,
// but you needn't care what it does. Just be sure it's called when
// appropriate within reaction_o()/reaction_h().
void make_water();

struct reaction {
	int h_num;
	int o_num;
	struct lock *lock;
	struct condition *h_cond_not_ready;
	struct condition *o_cond_not_ready;
};

void
reaction_init(struct reaction *reaction)
{
	reaction->h_num = 0;
	reaction->o_num = 0;
	lock_acquire(reaction->lock);
	cond_init(reaction->h_cond_not_ready);
	cond_init(reaction->o_cond_not_ready);
}

void
reaction_h(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	++(reaction->h_num);
	while (reaction->h_num < 2)
		cond_wait(reaction->h_cond_not_ready, reaction->lock);
	while (reaction->o_num < 1)
		cond_wait(reaction->o_cond_not_ready, reaction->lock);
	if (reaction->h_num >= 2 && reaction->o_num >= 1) {
		reaction->h_num -= 2;
		--reaction->o_num;
		make_water();
	}
	if (reaction->h_num >= 2)
		cond_signal(reaction->h_cond_not_ready, reaction->lock);
	lock_release(reaction->lock);
}

void
reaction_o(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	++(reaction->o_num);
	while (reaction->o_num < 1)
		cond_wait(reaction->o_cond_not_ready, reaction->lock);
	while (reaction->h_num < 2)
		cond_wait(reaction->h_cond_not_ready, reaction->lock);
	if (reaction->h_num >= 2 && reaction->o_num >= 1) {
		reaction->h_num -= 2;
		--reaction->o_num;
		make_water();
	}
	if (reaction->o_num >= 1)
		cond_signal(reaction->o_cond_not_ready, reaction->lock);
	lock_release(reaction->lock);
}
