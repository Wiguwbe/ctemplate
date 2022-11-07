hello world
{$
	int integer;
	char *string;
$}

string is composed of:
{% for char*c=$string;*c;c++ %}
  - '{>c *c >}'
{% endfor %}

the integer is:
  decimal: {>d $.integer >}
  hex:     {>x $->integer >}

the integer is{% if $integer&1 %} not{% fi %} multiple of 2
