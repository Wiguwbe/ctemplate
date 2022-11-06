
## generated files

Files generated are the C source file, the C header file with the interface
and an ELF object file.

- `<template>.c`: the generated C source file;
- `<template>.h`: the generated C header file;
- `<template>_text.o`: the generated ELF object with the text.

_See below for remarks on file naming_

## generated interface

the context struct

```
/* the context structure */
struct <tpl-name>_ctx {
	# should get from template file
	...
};

/* TODO should text be on separate .o and compiled together? */
/* internal */
char *text = "...";

/* to print to file_des */
int <tpl-name>_fd(int fd, struct <tpl-name>_ctx ctx);

/* to write to buffer (uses fmemopen) */
int <tpl-name>_fd(char *output, size_t *output_len, struct <tpl-name>_ctx ctx);

/* TODO should it only expose a FILE* interface? */
int <tpl-name>_gen(FILE *file, struct <tpl-name>_ctx *ctx);
```

## notes on the tool runtime

The tool will create a temporary directory with some files.

> Or shall the tool allocate memory (mmap/malloc?) and use fmemopen?

## some remarks on the template file

instead of just copying C code from the template file, this could have more
conventional control structs, like

`{% for <something> %} .. {% endfor %}`

would be replaced with

`for(<something>) { .. }`

This would still require to have actual blocks of code, maybe using the previous
`{{ ... code ... }}` notation

## the template file

define the context `{$ ... $}`
```
{$
	int value;
	char *string;
	...
$}
```

to access the context `... $.value ...` (example below)
> **TODO**
>
>	access can be a lot simplified,
>
>	it can be:
>
>		- `$->value`, which would require just replacing '$' with 'ctx';
>		- `$.value`, which would require replacing '$.' with 'ctx->';
>		- `$value`, which would require replacing '$' with 'ctx->'
>
>	the `$value` seems to be the most simple form, nevertheless,
>	this doc will use `$.value`


block of code `{{ ... }}`
```
{{ for(int i=0;i<$.value;i++) { }}
	do something here
{{ } }}
```

to print something (using printf) `{> "fmt_string", arg1, arg2... >}`

> **TODO**
>
>	should this have a more simpler form? specially without the `fmt_string`
>	although, user may need to specify the type
>	one possible way would be to use `{><fmt> <value> >}`, like:
>	the value of i is {>d i >}

```
{{ for(int i=0; i<$.value; i++) { }}
	the value of i is {> "%d", i >}
{{ } }}
```

"special" keywords are to be used with `{% <keyword> args... %}`
these are mostly to make this more high-level-looking :)

`{% include "partial/footer" { .current_page = "index" } %}`

to specify the C file headers, use `{# ... #}`, which will be copied to the
begining of the file

> **TODO**
>
>	should these use other then the '#' symbol?

> _Note_
>
> Some includes are already there (like `stdio.h`)
```
{#
#include <stdio.h>
#include "custom-data.h"
#}
```

comments are (multiline and specified with) `{* ... *}`
```
{*
	a comment, duh
*}
```

## hierarchy or partials

This generator will probably only include an `include` keyword, which
would go something like:

`{% include <name> <ctx> %}`

where the <name> is either a literal quoted string:

`{% include "partial/header" <ctx> %}`

or a pointer to a generator function:

`{% include $.inner_template <ctx> %}`

and the `<ctx>` would be the definition of the context to pass to the included
template. The context can be a struct definition such as:

`{% include <name> { .value = 34, .name = "hello world" } %}`

in which case the generator will create the code to statically allocate the
structure and pass a pointer to the included template, or be an assignment:

`{% include <name> $.inner_ctx %}`

in which case, it will use that as a pointer to the context.

When the <name> is not a literal, it is not possible to do type checking, so, in
that case, the <ctx> can only be passed by pointer.

With <name> and <ctx> having the possibility of being dynamic, a "parent" file
which then calls an inner template is then possible, instead of every template
including partials.

## include and file names

Because C names can't have slashes (`/`) (or directory structure), slashes in names
will be replaced with an underscore (`_`), eg:

`"hello/world.tpl"` would become `"hello_world"`.

The tool should make that transparent.
