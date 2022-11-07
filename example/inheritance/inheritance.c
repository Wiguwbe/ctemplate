
#include <stdio.h>

#include "tpl-parent.h"
#include "tpl-sub1.h"
#include "tpl-sub2.h"

int main(int argc, char **argv)
{
	int(*tpl[])(FILE*,void*) = {
		sub1_gen,
		sub2_gen,
		sub1_gen
	};
	struct sub1_ctx c_0 = {.value = 66, .str = "index 0"};
	struct sub2_ctx c_1 = { .fl = 43.608};
	struct sub1_ctx c_2 = {.value = -43, .str = "a negative number"};
	void *ctx_s[3] = { &c_0, &c_1, &c_2};

	struct parent_ctx ctx = {
		.tpl = tpl,
		.ctx = ctx_s,
		.ctx_len = 3
	};

	return parent_gen(stdout, &ctx);
}
