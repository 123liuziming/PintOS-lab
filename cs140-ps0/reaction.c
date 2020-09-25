#include "pintos_thread.h"

// Forward declaration. This function is implemented in reaction-runner.c,
// but you needn't care what it does. Just be sure it's called when
// appropriate within reaction_o()/reaction_h().
void make_water();

struct reaction {
	int h_num;
	struct lock *lock;
	struct condition *h_cond_not_ready;
	struct condition *o_cond_not_ready;
};

void
reaction_init(struct reaction *reaction)
{
	reaction->h_num = 0;
	reaction->h_cond_not_ready = malloc(sizeof(struct condition));
	reaction->o_cond_not_ready = malloc(sizeof(struct condition));
	reaction->lock = malloc(sizeof(struct lock));
	lock_init(reaction->lock);
	cond_init(reaction->h_cond_not_ready);
	cond_init(reaction->o_cond_not_ready);
}

void
reaction_h(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	++(reaction->h_num);
	if (reaction->h_num >= 2)
		cond_signal(reaction->h_cond_not_ready, reaction->lock);
	cond_wait(reaction->o_cond_not_ready, reaction->lock);
	lock_release(reaction->lock);
}

void
reaction_o(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	while (reaction->h_num < 2)
		cond_wait(reaction->h_cond_not_ready, reaction->lock);
	reaction->h_num -= 2;
	make_water();
	cond_signal(reaction->o_cond_not_ready, reaction->lock);
	cond_signal(reaction->o_cond_not_ready, reaction->lock);
	lock_release(reaction->lock);
}
