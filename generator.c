
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/mman.h>

#include "common.h"

#include "generator.h"
#include "objcopy.h"

// 1000 lines at 80 chars each
#define _MEM_SIZE 8192

struct gdata {
	FILE *input;
	FILE *source_headers;
	FILE *source_c;
	FILE *header_c;
	FILE *data;
	char *basename;
	unsigned long text_index;
	unsigned long text_last;
};

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

/*
	functions
*/

static int _func_include(struct gdata *gdata);
static int _func_for(struct gdata *gdata);
static int _func_end(struct gdata *gdata);
static int _func_if(struct gdata *gdata);
static int _func_elif(struct gdata *gdata);
static int _func_else(struct gdata *gdata);
static int _func_while(struct gdata *gdata);

/*
	a simple "map" string->function

	since this is a "compiler", the time requirements
	are a bit more loose, so it's okay to use simple
	(or basic) search methods
*/
struct func {
	char *fname;
	int(*func)(struct gdata*);
};
static struct func _functions[] = {
	{"include", _func_include},
	{"for", _func_for},
	{"if", _func_if},
	{"elif", _func_elif},
	{"else", _func_else},
	{"while", _func_while},
	// endfor/end/fi
	{"endfor", _func_end},
	{"end", _func_end},
	{"fi", _func_end},

	// nul pointer at the end
	{NULL, NULL}
};


static int _handle_ctx(struct gdata *gdata);	// $
static int _handle_code(struct gdata *gdata);	// {
static int _handle_print(struct gdata *gdata);	// >
static int _handle_func(struct gdata *gdata);	// %
static int _handle_header(struct gdata *gdata);	// #
static int _handle_comment(struct gdata *gdata);// *

static int _code_until(FILE *in, FILE *out, int end)
{
	int c,n;
	while((c = fgetc(in)) != EOF) {
		if(c == end) {
			n = fgetc(in);
			if(n == EOF) {
				PERROR(fgetc);
				return 1;
			}
			if(n == '}')
				// end
				return 0;
			// else
			if(fputc(c, out) == EOF) {
				PERROR(fputc);
				return 1;
			}
			if(fputc(n, out) == EOF) {
				PERROR(fputc);
				return 1;
			}
		} else if(c == '$') {
			// context variable
			/* handle 3 possible cases ? */
			switch (n = fgetc(in)) {
			case '.':
				/* `$.name` ==> `ctx->name` */
				if(fprintf(out, "ctx->") < 0) {
					PERROR(fprintf);
					return 1;
				}
				break;
			case '-':
				/* `$->name` ==> `ctx->name` */
				if(fprintf(out, "ctx-") < 0) {
					PERROR(fprintf);
					return 1;
				}
				break;
			case EOF:
				PERROR(fgetc);
				return 1;
			default:
				/* `$name` ==> `ctx->name` */
				if(fprintf(out, "ctx->%c", n) < 0) {
					PERROR(fprintf);
					return 1;
				}
			}
		} else {
			if(fputc(c, out) == EOF) {
				PERROR(fputc);
				return 1;
			}
		}
	}

	// EOF
	PERROR(fgetc);
	return 1;
}

static int _handle_ctx(struct gdata *gdata)
{
	/* write to header_c until '$}' is found */
	int c,n;
	// struct header
	if(fprintf(gdata->header_c, "struct %s_ctx {\n", gdata->basename) < 0) {
		PERROR(fprintf);
		return 1;
	}

	if(_code_until(gdata->input, gdata->header_c, '$')) {
		return 1;
	}

	if(fprintf(gdata->header_c, "};\n") < 0) {
		PERROR(fprintf);
		return 1;
	}
	return 0;
}

static int _handle_code(struct gdata *gdata)
{
	return _code_until(gdata->input, gdata->source_c, '}');
}

static int _handle_print(struct gdata *gdata)
{
	// get format
	int c,n;

	// `fprintf(file, "%`
	if(fprintf(gdata->source_c, "fprintf(file, \"%%") < 0) {
		PERROR(fprintf);
		return 1;
	}

	// `<format>`
	while((c = fgetc(gdata->input)) != ' ') {
		if(c == EOF) {
			PERROR(fgetc);
			return 1;
		}

		if(fputc(c, gdata->source_c) == EOF) {
			PERROR(fputc);
			return 1;
		}
	}
	// `", `
	if(fprintf(gdata->source_c, "\", ") < 0) {
		PERROR(fprintf);
		return 1;
	}

	// `<variable>`
	return _code_until(gdata->input, gdata->source_c, '>');
}

static int _handle_func(struct gdata *gdata)
{
#define _F_SIZE 31
	char function[_F_SIZE+1];
	int c;
	int f_index = 0;
	struct func *fptr;

	// it's the most complex so far (i think)

	// 1. ignore white space
	while(isspace(c = fgetc(gdata->input)));
	if(c == EOF) {
		PERROR(fgetc);
		return 1;
	}
	// 2. copy function name
	do {
		if(c == EOF) {
			PERROR(fgetc);
			return 1;
		}
		if(c == '%') {
			// some functions may not have arguments
			ungetc(c, gdata->input);
			break;
		}
		function[f_index++] = (char)c;
	} while(!isspace(c = fgetc(gdata->input)) && f_index<_F_SIZE);
	if(f_index == _F_SIZE) {
		function[_F_SIZE-1] = 0;
		fprintf(stderr, "ctemplate: invalid function '%s...'\n", function);
		return 1;
	}
	// got a space (next is text?)
	function[f_index] = 0;

	// 3. find the target function
	for(fptr=_functions; fptr->fname; fptr++) {
		if(!strncmp(fptr->fname, function, f_index)) {
			return fptr->func(gdata);
		}
	}
	// else, no function
	fprintf(stderr, "ctemplate: unknown function '%s'\n", function);
	return 1;
#undef _F_SIZE
}

static int _handle_header(struct gdata *gdata)
{
	return _code_until(gdata->input, gdata->source_headers, '#');
}

static int _handle_comment(struct gdata *gdata)
{
	FILE *dev_null = fopen("/dev/null", "a");
	if(!dev_null) {
		PERROR(fopen);
		return 1;
	}

	int status = _code_until(gdata->input, dev_null, '*');

	fclose(dev_null);

	return status;
}

static int _print_text(struct gdata *gdata)
{
	if(gdata->text_index <= gdata->text_last)
		return 0;

	if(fprintf(
		gdata->data,
		"fwrite(_data_%s+%d, 1, %d, file);\n",
		gdata->basename,
		gdata->text_last,
		gdata->text_index - gdata->text_last
	) < 0) {
		PERROR(fprintf);
		return 1;
	}

	return 0;
}

static int _func_include(struct gdata *gdata)
{
	char template[128];
	int t_index = 0;
	int c;
	int is_fptr = 0;

	// 1. skip whitespace
	while(isspace(c = fgetc(gdata->input)));
	if(c == EOF) {
		fprintf(stderr, "ctemplate: include: unexpected EOF\n");
		return 1;
	}
	if(c == '%') {
		fprintf(stderr, "ctemplate: include: unexpected '%%' at include\n");
		return 1;
	}

	// 2. get template name/ptr
	// if starts with a '"', it's a constant, (#include <that file>)
	// else, it's a function pointer
	if(c == '"') {
		while((c = fgetc(gdata->input)) != '"') {
			if(c==EOF) {
				fprintf(stderr, "unexpected EOF looking for end of string\n");
				return 1;
			}
			if(c == '/') c = '_';
			template[t_index++] = c;
			// no length check, let it blow
		}
		template[t_index] = 0;
		// write it (create a scope for it)
		if( fprintf(
				gdata->source_c,
				"{\n"
				"\tint (*inner_template)(FILE *file, void*ctx) = %s_gen;\n"
				,
				template
			) < 0) {
			PERROR(fprintf);
			return 1;
		}
		// include that file
		if(fprintf(gdata->source_headers, "#include \"tpl-%s.h\"\n", template) < 0) {
			PERROR(fprintf);
			return 1;
		}
	} else {
		is_fptr = 1;
		// prepare scope
		if(fprintf(gdata->source_c, "{\n\tint (*inner_template)(FILE*file,void*ctx) = ") < 0) {
			PERROR(fprintf);
			return 1;
		}
		do {
			if(c == EOF) {
				fprintf(stderr, "unexpected EOF while parsing template name\n");
				return 1;
			}
			if(c == '%') {
				fprintf(stderr, "incomplete 'include' function\n");
				return 1;
			}
			if(c == '$') {
				int n;
				switch(n = fgetc(gdata->input)) {
				case '.':	/* $.name */
					if(fprintf(gdata->source_c, "ctx->")<0) {
						PERROR(fprintf);
						return 1;
					}
					break;
				case '-':	/* $->name */
					if(fprintf(gdata->source_c, "ctx-")<0) {
						PERROR(fprintf);
						return 1;
					}
					break;
				case EOF:
					PERROR(fgetc);
					return 1;
				default:	/* $name */
					if(fprintf(gdata->source_c, "ctx->%c", n)) {
						PERROR(fprintf);
						return 1;
					}
				}
			} else {
				if(fputc(c, gdata->source_c)==EOF) {
					PERROR(fputc);
					return 1;
				}
			}
		} while(!isspace(c = fgetc(gdata->input)));

		// close line
		if(fprintf(gdata->source_c, ";\n") < 0) {
			PERROR(fprintf);
			return 1;
		}
	}

	// 3. skip more whitespace
	while(isspace(c = fgetc(gdata->input)));
	if(c == EOF) {
		PERROR(fgetc);
		return 1;
	}
	if(c == '%') {
		fprintf(stderr, "ctemplate: unexpected '%%' at include\n");
		return 1;
	}

	// 4. get context definition
	if(c == '{') {
		// static definition
		if(is_fptr) {
			fprintf(stderr, "ctemplate: static context definition with function pointer is invalid\n");
			return 1;
		}
		// alright (inside scope)
		if(fprintf(gdata->source_c, "\tstruct %s_ctx inner_ctx_struct = ", template) < 0) {
			PERROR(fprintf);
			return 1;
		}
		// copy
		if(_code_until(gdata->input, gdata->source_c, '%'))
			return 1;

		// end line (plus pointer)
		if(fprintf(gdata->source_c, ";\n\tvoid *inner_ctx = &inner_ctx_struct\n") < 0) {
			PERROR(fprintf);
			return 1;
		}
	} else {
		// dynamic, simpler
		if(fprintf(gdata->source_c, "\tvoid *inner_ctx = ") < 0) {
			PERROR(fprintf);
			return 1;
		}
		if(_code_until(gdata->input, gdata->source_c, '%'))
			return 1;

		// end line
		if(fprintf(gdata->source_c, ";\n") < 0) {
			PERROR(fprintf);
			return 1;
		}
	}

	// make the call
	if(fprintf(stderr, "\tinner_template(file, inner_ctx);\n}\n") < 0) {
		PERROR(fprintf);
		return 1;
	}

	return 0;
}

static int _generic_func(struct gdata *gdata, char *keyword)
{
	if(fprintf(gdata->source_c, "%s(", keyword)< 0) {
		PERROR(fprintf);
		return 1;
	}

	if(_code_until(gdata->input, gdata->source_c, '%')) {
		return 1;
	}

	if(fprintf(gdata->source_c, ") {\n") < 0) {
		PERROR(fprintf);
		return 1;
	}

	return 0;
}

static int _func_for(struct gdata *gdata)
{
	return _generic_func(gdata, "for");
}
static int _func_if(struct gdata *gdata)
{
	return _generic_func(gdata, "if");
}
static int _func_elif(struct gdata *gdata)
{
	// mix of "end" and "elif"
	return _generic_func(gdata, "} else if");
}
static int _func_else(struct gdata *gdata)
{
	// simple "end" and "else"
	if(fprintf(gdata->source_c, "} else {\n") < 0) {
		PERROR(fprintf);
		return 1;
	}

	return 0;
}
static int _func_while(struct gdata *gdata)
{
	return _generic_func(gdata, "while");
}

static int _func_end(struct gdata *gdata)
{
	if(fprintf(gdata->source_c, "}\n") < 0) {
		PERROR(fprintf);
		return 1;
	}
	return 0;
}

static int _generate(struct gdata *gdata)
{
	/*
		Iterate:
			1. parse text until '{x' is found;
			1.1 if len(text) -> write it out
			2. handle '{x'
			3. reset text/index params
			(repeat)
	*/
	int c;
	int n;
	while((c = fgetc(gdata->input)) != EOF) {
		// 1.
		if(c == '{') {
			// possible code
			n = fgetc(gdata->input);
			if(n == EOF) {
				ungetc(n, gdata->input);
				if(fputc(c, gdata->data)==EOF) {
					PERROR(fputc);
					return 1;
				}
				gdata->text_index++;
				continue;
			}
			// 2. else
			int (*handler)(struct gdata *) = NULL;
			if(n=='$')		handler = _handle_ctx;
			else if(n=='{')	handler = _handle_code;
			else if(n=='>')	handler = _handle_print;
			else if(n=='%')	handler = _handle_func;
			else if(n=='#')	handler = _handle_header;
			else if(n=='*')	handler = _handle_comment;
			else {
				// nothing much then
				ungetc(n, gdata->input);
				if(fputc(c, gdata->data)==EOF) {
					PERROR(fputc);
					return 1;
				}
				gdata->text_index++;
				continue;
			}

			// 1.1
			if(_print_text(gdata)) {
				return 1;
			}
			if(handler(gdata)) {
				// some error
				return 1;
			}

			// 3. reset indexes
			gdata->text_last = gdata->text_index;

		} else {
			if(fputc(c, gdata->data) == EOF) {
				PERROR(fputc);
				return 1;
			}
			gdata->text_index++;
		}
	}

	// 1.1 final

	return _print_text(gdata);
}

int generate_template(FILE *input, char *basename)
{
	/*
		TODO need FILE for:
			source-headers,
			source-c,
			header-c,
			data file (text),
			?
	*/
	void *mem_source_headers;
	void *mem_source_c;
	void *mem_header_c;
	void *mem_data_file;
	struct gdata gdata;
	int status_code = 1;

	/* TODO all this munmap/fclose can just be ignored when the application exits */

	/* allocate memory (mmap, anon private) */
	{
		if((mem_source_headers = mmap(NULL, _MEM_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
			PERROR(mmap);
			goto _exit_a;
		}
		if((mem_source_c = mmap(NULL, _MEM_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
			PERROR(mmap);
			goto _exit_b;
		}
		if((mem_header_c = mmap(NULL, _MEM_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
			PERROR(mmap);
			goto _exit_c;
		}
		if((mem_data_file = mmap(NULL, _MEM_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
			PERROR(mmap);
			goto _exit_d;
		}
	}

	/* open files */
	{
		if(!(gdata.source_headers = fmemopen(mem_source_headers, _MEM_SIZE, "w+"))) {
			PERROR(fmemopen);
			goto _exit_e;
		}
		if(!(gdata.source_c = fmemopen(mem_source_c, _MEM_SIZE, "w+"))) {
			PERROR(fmemopen);
			goto _exit_f;
		}
		if(!(gdata.header_c = fmemopen(mem_header_c, _MEM_SIZE, "w+"))) {
			PERROR(fmemopen);
			goto _exit_g;
		}
		if(!(gdata.data = fmemopen(mem_data_file, _MEM_SIZE, "w+"))) {
			PERROR(fmemopen);
			goto _exit_h;
		}
	}

	/* prepare gdata */
	gdata.input = input;
	gdata.basename = basename;
	gdata.text_index = gdata.text_last = 0;

	// now, do the thing
	if(_generate(&gdata)) {
		goto _exit_i;
	}

	// close the mem files
	{
		if(fclose(gdata.data) == EOF) {
			PERROR(fclose);
			goto _exit_h;
		}
		if(fclose(gdata.header_c) == EOF) {
			PERROR(fclose);
			goto _exit_g;
		}
		if(fclose(gdata.source_c) == EOF) {
			PERROR(fclose);
			goto _exit_f;
		}
		if(fclose(gdata.source_headers) == EOF) {
			PERROR(fclose);
			goto _exit_e;
		}
	}

	// build .C file
	{
		FILE *c_file;
		char cfname[128];
		int i_status = 1;
		if(snprintf(cfname, 128, "tpl-%s.c", basename) > 128) {
			fprintf(stderr, "filename would be too long\n");
			goto _exit_e;
		}
		if(!(c_file = fopen(cfname, "w"))) {
			fprintf(stderr, "failed to open/create .C file\n");
			goto _exit_e;
		}

		// our headers
		if(fprintf(c_file, "#include <stdio.h>\n\n") < 0) {
			PERROR(fprintf);
			goto _c_end;
		}
		// template header
		if(fprintf(c_file, "#include \"tpl-%s.h\"\n\n", basename) < 0) {
			PERROR(fprintf);
			goto _c_end;
		}

		// source headers
		if(fprintf(c_file, "%s\n", mem_source_headers)<0) {
			PERROR(fprintf);
			goto _c_end;
		}

		// the data pointer
		if(fprintf(c_file, "extern char *_data_%s;\n\n", basename) < 0) {
			PERROR(fprintf);
			goto _c_end;
		}

		// the function interface
		if(fprintf(c_file, "int %s_gen(FILE *file, struct %s_ctx *ctx)\n{\n", basename, basename) < 0) {
			PERROR(fprintf);
			goto _c_end;
		}

		// the source code
		if(fprintf(c_file, "%s\n}\n", mem_source_c) < 0) {
			PERROR(fprintf);
			goto _c_end;
		}

		// all good then
		i_status = 0;

		_c_end:
		if(fclose(c_file) == EOF) {
			PERROR(fclose);
			goto _exit_e;
		}
		if(i_status) {
			fprintf(stderr, "failed to write C file to disk\n");
			goto _exit_e;
		}
	}

	// build .H file
	{
		FILE *h_file;
		char hfname[128];
		int i_status = 1;
		if(snprintf(hfname, 128, "tpl-%s.h", basename) > 128) {
			fprintf(stderr, "filename would be too long\n");
			goto _exit_e;
		}
		if(!(h_file = fopen(hfname, "w"))) {
			fprintf(stderr, "failed to open/create .H file\n");
			goto _exit_e;
		}

		// ifndef/define
		if(fprintf(h_file, "#ifndef _TPL_") < 0) {
			PERROR(fprintf);
			goto _h_end;
		}
		for(char *ptr = basename; *ptr; ptr++) {
			char tp = *ptr;
			if(isalpha(tp)) tp &= 0x5f;
			if(fputc(tp, h_file) == EOF) {
				PERROR(fputc);
				goto _h_end;
			}
		}
		if(fprintf(h_file, "_H_\n#define _TPL_") < 0) {
			PERROR(fprintf);
			goto _h_end;
		}
		for(char *ptr = basename; *ptr; ptr++) {
			char tp = *ptr;
			if(isalpha(tp)) tp &= 0x5f;
			if(fputc(tp, h_file) == EOF) {
				PERROR(fputc);
				goto _h_end;
			}
		}
		if(fprintf(h_file, "_H_\n\n") < 0) {
			PERROR(fprintf);
			goto _h_end;
		}

		// context struct
		if(fprintf(h_file, "%s\n", mem_header_c) < 0) {
			PERROR(fprintf);
			goto _h_end;
		}

		// generator template
		if(fprintf(h_file, "int %s_gen(FILE *file, struct %s_ctx *ctx);\n\n", basename, basename) < 0) {
			PERROR(fprintf);
			goto _h_end;
		}

		// endif
		if(fprintf(h_file, "#endif\n") < 0) {
			PERROR(fprintf);
			goto _h_end;
		}

		// all good thne
		i_status = 0;

		_h_end:
		if(fclose(h_file) == EOF) {
			PERROR(fclose);
			goto _exit_e;
		}
		if(i_status) {
			fprintf(stderr, "failed to write H file to disk\n");
			goto _exit_e;
		}
	}

	// build .O file
	if(objcopy(mem_data_file, basename)) {
		goto _exit_e;
	}

	// otherwise, all seems good
	status_code = 0;

	// files are already closed
	goto _exit_e;
_exit_i:
	fclose(gdata.data);
_exit_h:
	fclose(gdata.header_c);
_exit_g:
	fclose(gdata.source_c);
_exit_f:
	fclose(gdata.source_headers);
_exit_e:
	munmap(mem_data_file, _MEM_SIZE);
_exit_d:
	munmap(mem_header_c, _MEM_SIZE);
_exit_c:
	munmap(mem_source_headers, _MEM_SIZE);
_exit_b:
	munmap(mem_source_headers, _MEM_SIZE);
_exit_a:
	return status_code;
}
