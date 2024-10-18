#include "tl_parser.h"
#include "array.h"
#include <err.h>
#include <stdio.h>
#include <string.h>

#define BLEN 1024 

#define STR(buf, len, ...) \
	({snprintf(buf, len-1, __VA_ARGS__); buf[len-1]=0; buf;})

#define METHODS_H "../tg/methods.h"
#define METHODS_C "../tg/methods.c"
#define TYPES_H "../tg/types.h"
#define MACRO_H "../tg/macro.h"
#define DESERIALIZE_TABLE_H "../tg/deserialize_table.h"
#define DESERIALIZE_TABLE_C "../tg/deserialize_table.c"

typedef struct generator_ {
	FILE *methods_h;
	FILE *methods_c;
	FILE *types_h;
	FILE *macro_h;
	FILE *deserialize_table_h;
	FILE *deserialize_table_c;
	array_t *type_names;
} generator_t;


int change_dots_to_underline(char *s, int len)
{
	if (!s)
		return 1;
	
	int i;
	char *dot = NULL;
	for (dot = strrchr(s, '.'), i=0;
			 dot && i < len;
			 dot = strrchr(s, '.'), i++)
	{
		*dot = '_';
	}

	return 0;
}

int method_change_dots_to_underline(
		const struct method_t *m)
{
	int i;
	change_dots_to_underline(m->name, BLEN);
	change_dots_to_underline(m->ret, BLEN);
	for (i = 0; i < m->argc; ++i) {
		change_dots_to_underline(m->args[i].name, BLEN);
		change_dots_to_underline(m->args[i].type, BLEN);
	}	

	return 0;
}

int open_deserialize_table_header(generator_t *g)
{
	g->deserialize_table_h = fopen(DESERIALIZE_TABLE_H, "w");
	if (!g->deserialize_table_h)
		err(1, "can't write to: %s", DESERIALIZE_TABLE_H);	


	fputs("/* generated by tl_generator */\n\n",
		 	g->deserialize_table_h);
	
	fputs("#include \"tl_object.h\"\n\n",
		 	g->deserialize_table_h);
	
	fputs("#define TL_DESERIALIZES \\\n",
		 	g->deserialize_table_h);
	
	return 0;
}

int append_deserialize_table_header(
		generator_t *g, const struct method_t *m)
{
	char buf[BLEN];
	fputs(STR(buf, BLEN,
				"\tTL_DESERIALIZE(0x%.8x, tl_deserialize_%s) \\\n",
				m->id, m->name),
		 	g->deserialize_table_h);

	return 0;
}

int close_deserialize_table_header(generator_t *g)
{
	char buf[BLEN];
	
	fputs("\n\ntypedef tlo_t * tl_deserialize_function"
				"(buf_t*);\n"
			,g->deserialize_table_h);
	
	fputs("\n\n#define TL_DESERIALIZE(id, name) \\\n"
			"tl_deserialize_function name;\n"
			"TL_DESERIALIZES\n"
			"#undef TL_DESERIALIZE\n"
		 	, g->deserialize_table_h);


	fputs("\n\nstruct tl_deserialize {unsigned int id; "
				"tl_deserialize_function *fun;};\n"
				"static struct tl_deserialize tl_deserialize_table[] = \n"
				"{\n",
		 	g->deserialize_table_h);

	fputs("#define TL_DESERIALIZE(id, name) \\\n"
				"{id, name},\n"
				"TL_DESERIALIZES\n"
				,g->deserialize_table_h);
	
	fputs("};\n#undef TL_DESERIALIZE\n\n",
		 	g->deserialize_table_h);
	fclose(g->deserialize_table_h);
	return 0;
}

int open_deserialize_table(generator_t *g)
{
	g->deserialize_table_c = fopen(DESERIALIZE_TABLE_C, "w");
	if (!g->deserialize_table_c)
		err(1, "can't write to: %s", DESERIALIZE_TABLE_C);	

	char buf[BLEN];

	fputs("/* generated by tl_generator */\n\n",
		 	g->deserialize_table_c);
	fputs("#include \"deserialize_table.h\"\n"
	      "#include \"deserialize.h\"\n"
	      "#include \"alloc.h\"\n",
		 	g->deserialize_table_c);
	fputs("#include \"../mtx/include/sel.h\"\n\n",
		 	g->deserialize_table_c);
	
	return 0;
}

int append_deserialize_table(
		generator_t *g, const struct method_t *m)
{
	char buf[BLEN];
	fputs(STR(buf, BLEN,
				"tlo_t * tl_deserialize_%s(buf_t *buf){\n",
				m->name),
		 	g->deserialize_table_c);

	fputs(STR(buf, BLEN,
				"\tint nobjs = %d;\n",
				m->argc),
		 	g->deserialize_table_c);
	
	fputs(
			"\ttlo_t *obj = NEW(tlo_t, return NULL);\n"
				, g->deserialize_table_c);

	fputs(STR(buf, BLEN,
				"\tstrcpy(obj->name, \"%s\");\n",
				m->name),
		 	g->deserialize_table_c);
	
	fputs(
				"\tobj->id = *(ui32_t *)buf->data;\n"
				"\t*buf = buf_add(buf->data + 4, buf->size - 4);\n"
				, g->deserialize_table_c);
		
	if (m->argc)
		fputs(
				"\tobj->objs = (tlo_t **)MALLOC(nobjs * sizeof(tlo_t*), return NULL);\n"
					, g->deserialize_table_c);
	
	fputs(
				"\tui32_t flag = 0;\n"
				, g->deserialize_table_c);
			
	int i;
	for (i = 0; i < m->argc; ++i) {
		fputs(
				STR(buf, BLEN,
				"\t// parse arg %s\n"
				,m->args[i].type)
				, g->deserialize_table_c);
		
		// check if flag
		void *p;
		if (m->args[i].type == NULL && 
				strstr(m->args[i].name, "flags"))
		{
			fputs(
		 "\tflag = *(ui32_t *)(buf->data);\n"
				"\t*buf = buf_add(buf->data + 4, buf->size - 4);\n"
				, g->deserialize_table_c);

		} else if (m->args[i].type == NULL){
			//skip
			continue;
		} else {
		
			// skip if flag not match
			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN,
					"\tif ((flag & (1 << %d)) == (1 << %d))\n"
					,m->args[i].flagb, m->args[i].flagb)
					, g->deserialize_table_c);
			fputs(
					"\t{\n"
					, g->deserialize_table_c);

			// handle arguments
			char *p;
			{
				if (strcmp(m->args[i].type, "int") == 0){
				
					fputs(
					STR(buf, BLEN,
					"\t\ttlo_t *param = NEW(tlo_t, return NULL);\n"
					"\t\tobj->objs[%d] = param;\n"
					,i)
					, g->deserialize_table_c);

					fputs(
						"\t\tparam->value = buf_add(buf->data, 4);\n"
						"\t\tparam->type = TYPE_INT;\n"
						"\t\t*buf = buf_add(buf->data + 4, buf->size - 4);\n"
						, g->deserialize_table_c);

					fputs(STR(buf, BLEN,
							"\t\tstrcpy(param->name, \"%s\");\n",
							m->args[i].name),
							g->deserialize_table_c);
				}
				else if (strcmp(m->args[i].type, "bool") == 0 ||
				    strcmp(m->args[i].type, "true") == 0){
					
					fputs(
					STR(buf, BLEN,
					"\t\ttlo_t *param = NEW(tlo_t, return NULL);\n"
					"\t\tobj->objs[%d] = param;\n"
					,i)
					, g->deserialize_table_c);

					fputs(
						"\t\tparam->value = buf_add(buf->data, 4);\n"
						"\t\tparam->type = TYPE_BOOL;\n"
						"\t\t*buf = buf_add(buf->data + 4, buf->size - 4);\n"
						, g->deserialize_table_c);

					fputs(STR(buf, BLEN,
							"\t\tstrcpy(param->name, \"%s\");\n",
							m->args[i].name),
							g->deserialize_table_c);

				}
				else if (strcmp(m->args[i].type, "long") == 0){
					
					fputs(
					STR(buf, BLEN,
					"\t\ttlo_t *param = NEW(tlo_t, return NULL);\n"
					"\t\tobj->objs[%d] = param;\n"
					,i)
					, g->deserialize_table_c);


					fputs(
						"\t\tparam->value = buf_add(buf->data, 8);\n"
						"\t\tparam->type = TYPE_LONG;\n"
						"\t\t*buf = buf_add(buf->data + 8, buf->size - 8);\n"
						, g->deserialize_table_c);

					fputs(STR(buf, BLEN,
							"\t\tstrcpy(param->name, \"%s\");\n",
							m->args[i].name),
							g->deserialize_table_c);

				}
				else if (strcmp(m->args[i].type, "double") == 0){
					
					fputs(
					STR(buf, BLEN,
					"\t\ttlo_t *param = NEW(tlo_t, return NULL);\n"
					"\t\tobj->objs[%d] = param;\n"
					,i)
					, g->deserialize_table_c);


					fputs(
						"\t\tparam->value = buf_add(buf->data, 8);\n"
						"\t\tparam->type = TYPE_DOUBLE;\n"
						"\t\t*buf = buf_add(buf->data + 8, buf->size - 8);\n"
						, g->deserialize_table_c);

						fputs(STR(buf, BLEN,
							"\t\tstrcpy(param->name, \"%s\");\n",
							m->args[i].name),
							g->deserialize_table_c);

				}
				else if (strcmp(m->args[i].type, "string") == 0){
					fputs(
					STR(buf, BLEN,
					"\t\ttlo_t *param = NEW(tlo_t, return NULL);\n"
					"\t\tobj->objs[%d] = param;\n"
					,i)
					, g->deserialize_table_c);


					fputs(
						"\t\tparam->value = sel_deserialize_string(*buf);\n"
						"\t\tparam->type = TYPE_STRING;\n"
						"\t\tint size = param->value.size + 4;\n"
						"\t\t*buf = buf_add(buf->data + size, buf->size - size);\n"
						, g->deserialize_table_c);

					fputs(STR(buf, BLEN,
							"\t\tstrcpy(param->name, \"%s\");\n",
							m->args[i].name),
							g->deserialize_table_c);
				}
				else if (strcmp(m->args[i].type, "bytes") == 0){
					fputs(
					STR(buf, BLEN,
					"\t\ttlo_t *param = NEW(tlo_t, return NULL);\n"
					"\t\tobj->objs[%d] = param;\n"
					,i)
					, g->deserialize_table_c);


					fputs(
							"\t\tint size = *(ui32_t *)buf->data;\n"
						"\t\t*buf = buf_add(buf->data + 4, buf->size - 4);\n"
						"\t\tparam->value = buf_add(buf->data, size);\n"
						"\t\tparam->type = TYPE_BYTES;\n"
						"\t\t*buf = buf_add(buf->data + size - 4, buf->size - size + 4);\n"
						, g->deserialize_table_c);

					fputs(STR(buf, BLEN,
							"\tstrcpy(param->name, \"%s\");\n",
							m->args[i].name),
							g->deserialize_table_c);
				}
				else if (
						(p = strstr(m->args[i].type, "ector<")))
				{
					p += strlen("ector<");
					char * type = 
						strndup(p, strstr(p, ">") - p);

					fputs(
					STR(buf, BLEN,
					"\t\ttlo_t *param = NEW(tlo_t, return NULL);\n"
					"\t\tobj->objs[%d] = param;\n"
					,i)
					, g->deserialize_table_c);

					fputs(
							"\t\tparam->id = *(ui32_t *)buf->data;\n"
						"\t\t*buf = buf_add(buf->data + 4, buf->size - 4);\n"
						"\t\tparam->type = TYPE_VECTOR;\n"
						, g->deserialize_table_c);
					
						// handle Vector
						/* TODO:  <15-10-24, yourname> */
				
				} else {
					fputs(
						"\t\ttlo_t *param = tg_deserialize(buf);\n"
						, g->deserialize_table_c);
					
					fputs(
					STR(buf, BLEN,
					"\t\tobj->objs[%d] = param;\n"
					,i)
					, g->deserialize_table_c);


				}
			}
			
			fputs(
				"\t}\n"
				, g->deserialize_table_c);
		}
	}
	fputs("\treturn obj;\n",	g->deserialize_table_c);
	fputs("}\n\n",	g->deserialize_table_c);
	return 0;
}

int close_deserialize_table(generator_t *g)
{

	fclose(g->deserialize_table_c);
	return 0;
}

int open_macro(generator_t *g)
{
	int i, k;
	char buf[BLEN];

	g->macro_h = fopen(MACRO_H, "w");
	if (!g->types_h)
		err(1, "can't write to: %s", MACRO_H);	

	fputs("/* generated by tl_generator */\n\n",
		 	g->macro_h);
	
	for (i = 0; i < 64; ++i) {
		fputs("#define DECL_", g->macro_h);
		fputs(STR(buf, BLEN, "%d", i), g->macro_h);
		fputs("(", g->macro_h);
		for (k = 0;  k < i; ++k) {
			fputs(STR(buf, BLEN, 
						"type%d, arg%d", k+1, k+1), g->macro_h);
			if (k < i - 1)
				fputs(", ", g->macro_h);
		}
		fputs(") ", g->macro_h);
		
		for (k = 0;  k < i; ++k) {
			fputs(STR(buf, BLEN, 
						"type%d arg%d", k+1, k+1), g->macro_h);
			if (k < i - 1)
				fputs(", ", g->macro_h);
		}
		fputs("\n", g->macro_h);
	}

	return 0;
}

int close_macro(generator_t *g)
{

	fclose(g->macro_h);
	return 0;
}


int open_types_header(generator_t *g)
{
	g->type_names = 
		array_new(char*, perror("array_new"); return 1);	

	g->types_h = fopen(TYPES_H, "w");
	if (!g->types_h)
		err(1, "can't write to: %s", TYPES_H);	

	fputs("/* generated by tl_generator */\n\n",
		 	g->types_h);
	fputs("#include \"tl_object.h\"\n\n",
		 	g->types_h);
	
	return 0;
}
int append_types_header(
		generator_t *g, const struct method_t *m)
{
	int has_type = 0;
	array_t *a = g->type_names;
	array_for_each(a, char*, name){
		if (strcmp(name, m->ret) == 0){
			has_type = 1;
			break;
		}
	}
	if (!has_type)
		array_append(a, char*, strdup(m->ret), 
				perror("array_append"); return 1);
	
	return 0;
}

int close_types_header(generator_t *g)
{
	char buf[BLEN];
	array_t *a = g->type_names;
	array_for_each(a, char*, name){
		char *p = strstr(name, "Vector<");
		if (p)
			break;
			
		STR(buf, BLEN, "typedef tlo_t * %s;\n", name);

		fputs(buf, g->types_h);
		free(name);
	}
	fclose(g->types_h);
	return 0;
}


int open_methods_header(generator_t *g)
{
	g->methods_h = fopen(METHODS_H, "w");
	if (!g->methods_h)
		err(1, "can't write to: %s", METHODS_H);	

	fputs("/* generated by tl_generator */\n\n",
		 	g->methods_h);
	fputs("#include \"types.h\"\n"
					 "#include \"tl_object.h\"\n"
					 "#include \"macro.h\"\n"
				   "#include <stdbool.h>\n\n",
		 	g->methods_h);
	
	return 0;
}

int append_methods_header(
		generator_t *g, const struct method_t *m)
{
	int i;
	char buf[BLEN];
	//return
	fputs("tlo_t * ", g->methods_h);
	
	//name
	fputs(STR(buf, BLEN, "tl_%s(", m->name),
		 	g->methods_h);

	// args
	for (i = 0; i < m->argc; ++i) {
		// skip flags
		if (strstr(m->args[i].name, "flags") &&
				m->args[i].type == NULL)
			continue;

		char nbuf[64];
		char *type = m->args[i].type;
		char *name = STR(nbuf, 64, "arg_%s", m->args[i].name);
		char *pointer = "";
		char *len = "";
		if (!type)
			type = "int";
		
		char *p = strstr(type, "ector<");
		if (p){
			// change Vector to pointer
			pointer = "*";
			p += strlen("ector<");
			//p[strnlen(p, BLEN)-1] = 0;
			type = strndup(p, strstr(p, ">") - p);
			char buf[64];
			len = STR(buf, 64, ", int len_%s", name);
		}
		if (strcmp(type, "bytes") == 0){
			type = "unsigned char *";
			char buf[64];
			len = STR(buf, 64, ", int len_%s", name);
		}
		if (strcmp(type, "string") == 0)
			type = "const char *";
		if (strcmp(type, "true") == 0)
			type = "bool";
		if (strcmp(type, "false") == 0)
			type = "bool";
		if (strcmp(type, "X") == 0)
			type = "buf_t";
		if (strcmp(type, "!X") == 0)
			type = "buf_t";
		
		fputs(STR(buf, BLEN, "%s ", type),
				g->methods_h);
		fputs(STR(buf, BLEN, "%s%s%s", pointer, name, len),
		  	g->methods_h);
		if (i < m->argc -1)
			fputs(", ", g->methods_h);

	}
	fputs(");\n", g->methods_h);

	return 0;
}

int close_methods_header(generator_t *g)
{
	fclose(g->methods_h);
	return 0;
}

int open_methods(generator_t *g)
{
	g->methods_c = fopen(METHODS_C, "w");
	if (!g->methods_h)
		err(1, "can't write to: %s", METHODS_C);	

	fputs("/* generated by tl_generator */\n\n",
		 	g->methods_c);
	fputs(
			"#include \"methods.h\"\n"
			"#include \"stb_ds.h\"\n"
			"#include \"alloc.h\"\n"
			"#include \"../mtx/include/sel.h\"\n\n",
		 	g->methods_c);
	
	return 0;
}

int append_methods(
		generator_t *g, const struct method_t *m)
{
	int i;
	char buf[BLEN];

	//return
	fputs("tlo_t * ", g->methods_c);
	
	//name
	fputs(STR(buf, BLEN, "tl_%s(", m->name),
		 	g->methods_c);

	// args
	for (i = 0; i < m->argc; ++i) {
		// skip flags
		if (strstr(m->args[i].name, "flags") &&
				m->args[i].type == NULL)
			continue;

		char nbuf[64];
		char *type = m->args[i].type;
		char *name = STR(nbuf, 64, "arg_%s", m->args[i].name);
		char *pointer = "";
		char *len = "";
		if (!type)
			type = "int";
		
		char *p = strstr(type, "ector<");
		if (p){
			// change Vector to pointer
			pointer = "*";
			p += strlen("ector<");
			type = strndup(p, strstr(p, ">") - p);
			char buf[64];
			len = STR(buf, 64, ", int len_%s", name);
		}
		if (strcmp(type, "bytes") == 0){
			type = "unsigned char *";
			char buf[64];
			len = STR(buf, 64, ", int len_%s", name);
		}
		if (strcmp(type, "string") == 0)
			type = "const char *";
		if (strcmp(type, "true") == 0)
			type = "bool";
		if (strcmp(type, "false") == 0)
			type = "bool";
		if (strcmp(type, "X") == 0)
			type = "buf_t";
		if (strcmp(type, "!X") == 0)
			type = "buf_t";
		
		fputs(STR(buf, BLEN, "%s ", type),
				g->methods_c);
		fputs(STR(buf, BLEN, "%s%s%s", pointer, name, len),
		  	g->methods_c);
		if (i < m->argc -1)
			fputs(", ", g->methods_c);

	}
	fputs(")\n", g->methods_c);
	fputs("{\n", g->methods_c);
	
	// handle method
	fputs("\ttlo_t * obj = NEW(tlo_t, return NULL);\n",
			g->methods_c);

	// set id
	fputs(STR(buf, BLEN, "\tint i;\n\tobj->id = 0x%.8x;\n", m->id),
			g->methods_c);

	// create flags
	if (m->nflags){
		fputs(STR(buf, BLEN, 
					"\tint flags[%d]; int nflags = 0; //flags position\n ", m->nflags),
				g->methods_c);
		fputs(STR(buf, BLEN, 
					"\tint flagsv[%d]; //flags values\n ", m->nflags),
				g->methods_c);
	}

	// set objects
	if (m->argc){
		fputs(STR(buf, BLEN, "\tobj->nobjs = %d;\n", m->argc),
				g->methods_c);
		fputs(STR(buf, BLEN, "\tobj->objs = \n"
					"\t\t(tlo_t **)MALLOC(%d * sizeof(tlo_t*),"
					" return NULL);\n", m->argc),
				g->methods_c);
	}
	for (i = 0; i < m->argc; ++i) {
		fputs(STR(buf, BLEN,
					"\n\t//parse argument %s\n", m->args[i].type)
				,g->methods_c);
		fputs(STR(buf, BLEN,
					"\ttlo_t *arg%d = NEW(tlo_t, return NULL);\n" 
					"\tobj->objs[%d] = arg%d;\n" 
					,i,i,i)
				,g->methods_c);

		if (m->args[i].flagn)
			fputs(
					STR(buf, BLEN, 
						"\targ%d->flag_num = %d;\n" 
						"\targ%d->flag_bit = %d;\n", 
						i, m->args[i].flagn, i, m->args[i].flagb), 
					g->methods_c);

		// flags
		char *p;
		if (m->args[i].type == NULL && 
				strstr(m->args[i].name, "flags"))
		{
			// handle flags
			fputs(STR(buf, BLEN,
					"\tflags[nflags++] = %d;\n", i)
				,g->methods_c);

		} else if (m->args[i].type == NULL){
			fputs(
					STR(buf, BLEN, 
						"\targ%d->type = TYPE_INT;\n", i), 
					g->methods_c);
		} else if 
			(strcmp(m->args[i].type, "int")  == 0 ||
			 strcmp(m->args[i].type, "bool") == 0 ||
			 strcmp(m->args[i].type, "true") == 0)
		{
			fputs(
					STR(buf, BLEN, 
						"\targ%d->type = TYPE_INT;\n", i), 
					g->methods_c);
			fputs(
					STR(buf, BLEN, 
						"\targ%d->value = \n"
						"\t\tbuf_add_ui64(arg_%s);\n", 
						i, m->args[i].name), 
					g->methods_c);
	
			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\tif (arg_%s)\n\t\tflagsv[nflags-1] |= (1 << %d);\n", 
						m->args[i].name, m->args[i].flagb), 
					g->methods_c);

		} else if 
			(strcmp(m->args[i].type, "long")   == 0 ||
			 strcmp(m->args[i].type, "double") == 0)
			{
			fputs(
					STR(buf, BLEN, 
						"\targ%d->type = TYPE_LONG;\n", i), 
					g->methods_c);
			fputs(
					STR(buf, BLEN, 
						"\targ%d->value = \n"
						"\t\tbuf_add_ui32(arg_%s);\n", 
						i, m->args[i].name), 
					g->methods_c);
			
			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\tif (arg_%s)\n\t\tflagsv[nflags-1] |= (1 << %d);\n", 
						m->args[i].name, m->args[i].flagb), 
					g->methods_c);

		} else if (strcmp(m->args[i].type, "string") == 0){
			fputs(
				STR(buf, BLEN, 
					"\tif (arg_%s){\n", 
					m->args[i].name), 
				g->methods_c);

			fputs(
					STR(buf, BLEN, 
						"\t\targ%d->type = TYPE_STRING;\n", i), 
					g->methods_c);
			fputs(
					STR(buf, BLEN, 
						"\t\targ%d->value = \n"
						"\t\t\tbuf_add((ui8_t *)arg_%s, strlen(arg_%s));\n", 
						i, m->args[i].name, m->args[i].name), 
					g->methods_c);
			
			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\tflagsv[nflags-1] |= (1 << %d);\n", 
						m->args[i].flagb), 
					g->methods_c);
		
			fputs(
					"\t}\n\n", 
				g->methods_c);

		} else if (strcmp(m->args[i].type, "X") == 0 ||
			         strcmp(m->args[i].type, "!X")	== 0)
		{
			fputs(
					STR(buf, BLEN, 
						"\t\targ%d->type = TYPE_X;\n", i), 
					g->methods_c);
			fputs(
					STR(buf, BLEN, 
						"\t\targ%d->value = \n"
						"\t\t\targ_%s;\n", 
						i, m->args[i].name), 
					g->methods_c);
			
		} else if (strcmp(m->args[i].type, "bytes") == 0){
			fputs(
				STR(buf, BLEN, 
					"\tif (arg_%s){\n", 
					m->args[i].name), 
				g->methods_c);

			fputs(
					STR(buf, BLEN, 
						"\t\targ%d->type = TYPE_BYTES;\n", i), 
					g->methods_c);
			fputs(
					STR(buf, BLEN, 
						"\t\targ%d->value = \n"
						"\t\t\tbuf_add((ui8_t *)arg_%s, len_arg_%s);\n", 
						i, m->args[i].name, m->args[i].name), 
					g->methods_c);
			
			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\tflagsv[nflags-1] |= (1 << %d);\n", 
						m->args[i].flagb), 
					g->methods_c);
			
			fputs(
					"\t}\n\n", 
				g->methods_c);

		} else if 
			((p = strstr(m->args[i].type, "ector<"))){
				p += strlen("ector<");
			
			char * type = 
				strndup(p, strstr(p, ">") - p);
			
			fputs(
					STR(buf, BLEN, 
						"\targ%d->type = TYPE_VECTOR;\n", i), 
					g->methods_c);
			
			fputs(
				STR(buf, BLEN, 
					"\tif (arg_%s){\n", 
					m->args[i].name), 
				g->methods_c);

			if (strcmp(type, "bytes")  == 0)
			{
				fputs(
					STR(buf, BLEN, 
						"\t\tfor(i=0; i<len_arg_%s; ++i){\n"
						"\t\t\tint len = *(int *)(arg_%s[i]);\n"
						"\t\t\tui8_t *p = &(arg_%s[i][4]);\n"
						"\t\t\tbuf_t b = buf_add(p, len);\n"
						"\t\t\tbuf_cat(arg%d->value, b);\n"
						"\t\t}\n",
						m->args[i].name, m->args[i].name, m->args[i].name, i), 
					g->methods_c);
			
			}	else if (strcmp(type, "string")  == 0 )
			{
				fputs(
					STR(buf, BLEN, 
						"\t\tfor(i=0; i<len_arg_%s; ++i){\n"
						"\t\t\tint len = strlen(arg_%s[i]);\n"
						"\t\t\tbuf_t b = buf_add((ui8_t *)arg_%s[i], len);\n"
						"\t\t\tbuf_cat(arg%d->value, b);\n"
						"\t\t}\n",
						m->args[i].name, m->args[i].name, m->args[i].name, i), 
					g->methods_c);
			

			}	else if (strcmp(type, "int")   == 0 ||
			           strcmp(type, "bool") == 0) 
			{
				fputs(
					STR(buf, BLEN, 
						"\t\tfor(i=0; i<len_arg_%s; ++i){\n"
						"\t\t\tbuf_t b = buf_add_ui32(arg_%s[i]);\n"
						"\t\t\tbuf_cat(arg%d->value, b);\n"
						"\t\t}\n",
						m->args[i].name, m->args[i].name, i), 
					g->methods_c);

			}	else if (strcmp(type, "long")   == 0 ||
			           strcmp(type, "double") == 0) 
			{
				fputs(
					STR(buf, BLEN, 
						"\t\tfor(i=0; i<len_arg_%s; ++i){\n"
						"\t\t\tbuf_t b = buf_add_ui64(arg_%s[i]);\n"
						"\t\t\tbuf_cat(arg%d->value, b);\n"
						"\t\t}\n",
						m->args[i].name, m->args[i].name, i), 
					g->methods_c);
			} else {
				fputs(
					STR(buf, BLEN, 
						"\t\tfor(i=0; i<len_arg_%s; ++i){\n"
						"\t\t\targ%d->value = buf_add_ui32(arg_%s[i]->id);\n"
						"\t\t\tbuf_cat(arg%d->value, arg_%s[i]->value);\n"
						"\t\t}\n",
						m->args[i].name, i,  m->args[i].name, i, m->args[i].name), 
					g->methods_c);
			}
			
			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\tflagsv[nflags-1] |= (1 << %d);\n", 
						m->args[i].flagb), 
					g->methods_c);
		
			fputs(
					"\t}\n\n", 
				g->methods_c);

		} else {
			
			fputs(
				STR(buf, BLEN, 
					"\tif (arg_%s){\n", 
					m->args[i].name), 
				g->methods_c);

			fputs(
					STR(buf, BLEN, 
						"\t\targ%d->type = TYPE_X;\n", i), 
					g->methods_c);
			fputs(
					STR(buf, BLEN, 
						"\t\targ%d->value = buf_add_ui32(arg_%s[i].id);\n"
						"\t\tbuf_cat(arg%d->value, arg_%s[i].value);\n"
						,i, m->args[i].name, i, m->args[i].name), 
					g->methods_c);
			
			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\tflagsv[nflags-1] |= (1 << %d);\n", 
						m->args[i].flagb), 
					g->methods_c);
			
			fputs(
					"\t}\n\n", 
				g->methods_c);

		}
	}
	// write flags
	if (m->nflags)
		fputs("\n\t//write flags\n",g->methods_c);
	for (i = 0; i < m->nflags; ++i) {
		fputs(
				STR(buf, BLEN, 
					"\tint flag%d = flags[%d];\n" 
					"\tobj->objs[flag%d]->value = buf_add_ui32(flagsv[%d]);\n", 
					i, i, i, i), 
				g->methods_c);
	}

	fputs("\treturn obj;\n",g->methods_c);
	//
	fputs("}\n\n", g->methods_c);

	return 0;
}

int close_methods(generator_t *g)
{
	fclose(g->methods_c);
	return 0;
}

int callback(
		void *userdata,
		const struct method_t *m,
		const char *error)
{
	if (error){
		warn("%s", error);
		return 0;
	}
	
	//print_method(m); return 0; //debug

	generator_t *g = userdata;

	printf("generate method: ");
	printf("%.8x: ", m->id);
	printf("%s: ", m->name);
	int i;
	for (i = 0; i < m->argc; ++i) {
		printf("%s:%s ", 
				m->args[i].name, m->args[i].type);
	}
	printf("-> %s\n", m->ret);

	method_change_dots_to_underline(m);
	
	append_methods_header(g, m);
	append_types_header(g, m);
	append_methods(g, m);
	append_deserialize_table_header(g, m);
	append_deserialize_table(g, m);

	return 0;
}

int main(int argc, char *argv[])
{
	generator_t g;
	
	if (open_methods_header(&g))return 1;
	if (open_methods(&g))return 1;
	if (open_types_header(&g)) return 1;
	if (open_macro(&g)) return 1;
	if (open_deserialize_table_header(&g)) return 1;
	if (open_deserialize_table(&g)) return 1;

	tl_parse("telegram_api.tl", &g, callback);

	close_methods_header(&g);
	close_methods(&g);
	close_types_header(&g);
	close_macro(&g);
	close_deserialize_table_header(&g);
	close_deserialize_table(&g);
	return 0;
}
