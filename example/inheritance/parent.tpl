{$
	// list of template function pointers
	int (**tpl)(FILE *file, void*ctx);
	// list of context pointers
	void **ctx;
	int ctx_len;
$}
This is the parent, it calls sub templates.

The dynamic subs follow
--------
{% for int i=0; i<$ctx_len; i++ %}
	{% include $tpl[i] $ctx[i] %}
--------
{%endfor%}

And sub1 and sub2 included statically

# Sub1:
{% include "sub1" {.value=53, .str="hello"}%}

# Sub2
{% include "sub2" {.fl=4.566} %}

