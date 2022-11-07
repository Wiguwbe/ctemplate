# C Template Engine

A Jinja inspired _small_ templating engine for C.

Generates C code that in turn renders the templated text.

## Why?

It _may_ be useful.

## Top Level

Given a template text (`.tpl`), the tool will generate:

- a C source file (`.c`) that holds the function that renders the engine;
- a C header file (`.h`) that exposes the interface to the renderer;
- an ELF relocatable object file (`.o`) that holds the text used by the renderer. This is generated similarly to `objcopy`.

The template can then be called from C code.

## Files generated

Given a template `template.tpl`, it generates the files:

- `tpl-template.c`: the C source file;
- `tpl-template.h`: the C header file;
- `tpl-template-data.o`: the data file;

The interface generated is:

```
struct template_ctx {
	// from template file
	...
};

int template_gen(FILE *file, struct template_ctx *ctx);
```

Where the `FILE *file` is the output file (you have fmemopen if you want to write to memory), and the `struct template_ctx *ctx` is a pointer to the context to use (more on this later).

## Templating

Quick example:

```
{#
#include "my-files.h"
#include <stdlib.h>
#}
{$
	int value;
	char *str;
$}
This is a template file, the "value" is {>d $value >} (0x{>x $.value >})

The string passed was: "{>s $->str >}"
```

### The context

The context can be accessed through the special variable `$`.
To access its members, there are simplified ways:
- `$member`;
- `$.member`;
- `$->member`.

Where all these options do the same.

To define the context (`struct <tpl>_ctx`), use the `{$ ... $}` tags (example above).

### Printing stuff

To print stuff, the tag `{> .. >}` is a kind of wrapper for `printf`. The format is specified as
an extension to the tag (no spaces allowed):

```
{><fmt> <arg> >}
// becomes
fprintf(file, "%<fmt>", <arg>);

{>d 42 >} -> fpritnf(file, "%d", 42);
// etc
```

### Extra headers

To include extra headers in the C source file, use the `{# ... #}` tags. The text inside will be copied directly into the top of the file.

This is useful to include extra functions to be used throughout the template.

### Control structures

The tag `{% ... %}` is used to invoke "special" methods, namely used
to simplify the writing of loops/ifs. This can also be used to include other templates (more on this later).

The format is `{% <keyword> <args...> %}`, where the currently available keywords are:

- `for`: generates a for loop, the rest of the tag will be placed inside the parenthesis. The braces will automatically be generated.
- `while`: similar to `for`;
- `if`: similar to `for`/`while`;
- `elif`: similar to `if`. The previous brace group will be closed.
- `else`: similar to `elif` but doesn't take arguments.
- `end`/`fi`/`endfor`: these simply close the brace group, multiple names to make template more verbose, they are not checked.
- `include`: more on this later.

### Blocks of code

Direct-copy blocks of code can be used with the `{{ ... }}` tag.
This code will be copied into the C source file. Context `$...` can still be used here.

This is another option to write loops or do more complex `printf`s.

## File naming

Because C names don't really like slashes (`/`), dots (`.`) or
hyphens (`-`) (or pretty much anything not "alphanumeric_"), these
characters will be replaced with underscores (`_`).

Also, when taking templates to generate, the file extension will be dropped. This **does not happen** when `include`-ing with constant strings.

So, generating the template "partial/header.tpl" will take the
_basename_ `partial_header`, so will generate the files:
`tpl-partial_header.c`, `tpl-partial_header.h` and
`tpl-partial_header-data.o`, the C function `int partial_header_gen(...)` and the structure `struct partial_header_ctx {...};`.

## Inheritance

The current implementation doesn't have the concept of "parent", only of "included" (sub-)templates.

This is done with the "special" function `include`.

The format for the include is:

```
{% include <template> <context> %}
```

Where the `<template>` can be either a constant string (double quoted) or dynamic (anything else is assumed to translate to a function pointer).

And the `<context>` can be either a constant struct definition
`{ .value = 43, .str = "hello" , ...}` or dynamic (which is assumed to be a pointer to a context).

If the template is dynamic, the context must be dynamic. This is due to lack of ability to do type checks (in the generated C code).

When specifying a

Some examples:

Static include:
```
header is here
{% include "partial/header" { .title="some title" } %}
rest follows
```

Static include with dynamic context
```
{* pass the child's context in out context *}
{$
	void *title_ctx;
$}
header is here
{% include "partial/header" $title_ctx %}
rest follows
```

Dynamic name:
```
{$
	// list of templates
	int (**tpl)(FILE *, void*);
	// list of their contexts
	void **ctx;
	int tpl_len;
$}
This has a dynamic number of children, here are them:
----
{% for int i=0; i<$tpl_len; i++ %}
	{% include $tpl[i] $ctx[i] %}
----
{% endfor %}

yey
```

---

## TODO

- Better error reporting
- Take CLI options (input-dir, output-dir)
- Testing
