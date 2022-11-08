// the runner for test1

#include <stdio.h>

#include "tpl-test1.h"

extern char _data_test1[];

int main(int argc, char**argv)
{
	struct test1_ctx ctx = {
		.integer = 13,
		.string = "fourteen"
	};

	printf("%p\n", _data_test1);

	return test1_gen(stdout, &ctx);
}
