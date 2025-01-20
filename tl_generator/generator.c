#include "tl_parser.h"
#include "array.h"
#include <err.h>
#include <stdio.h>
#include <string.h>

#define API_LAYER "185"
#define BLEN 1024 

#define STR(buf, len, ...) \
	({snprintf(buf, len-1, __VA_ARGS__); buf[len-1]=0; buf;})

#define API_LAYER_H "../tl/api_layer.h"
#define ID_H "../tl/id.h"
#define METHODS_H "../tl/methods.h"
#define METHODS_C "../tl/methods.c"
#define TYPES_H "../tl/types.h"
#define STRUCT_H "../tl/struct.h"
#define NAMES_H "../tl/names.h"
#define DESERIALIZE_TABLE_H "../tl/deserialize_table.h"
#define DESERIALIZE_TABLE_C "../tl/deserialize_table.c"
#define TOJSON_TABLE_H "../tl/tojson_table.h"
#define TOJSON_TABLE_C "../tl/tojson_table.c"
#define FREE_H "../tl/free.h"
#define FREE_C "../tl/free.c"

typedef struct generator_ {
	FILE *id_h;
	FILE *methods_h;
	FILE *methods_c;
	FILE *types_h;
	FILE *names_h;
	FILE *struct_h;
	FILE *deserialize_table_h;
	FILE *deserialize_table_c;
	FILE *tojson_table_h;
	FILE *tojson_table_c;
	FILE *free_h;
	FILE *free_c;
	array_t *type_names;
	int isMtproto;
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

int open_id_header(generator_t *g)
{
	g->id_h = fopen(ID_H, "w");
	if (!g->id_h)
		err(1, "can't write to: %s", ID_H);	

	fputs(
			"/* generated by tl_generator */\n"
			"#ifndef TL_ID_H_\n"
			"#define TL_ID_H_\n"
			,g->id_h);
	
	fputs("enum {\n", g->id_h);
	return 0;
}

int append_id_header(
		generator_t *g, const struct method_t *m)
{
	char buf[BLEN];
	fputs(STR(buf, BLEN,
				"\tid_%s = 0x%.8x,\n"
				,m->name, m->id),
		 	g->id_h);
	return 0;
}

int close_id_header(generator_t *g)
{
	fputs(
			"};\n"
			"#endif // TL_ID_H_\n"
			, g->id_h);
	fclose(g->id_h);
	return 0;
}

int open_names_header(generator_t *g)
{
	g->names_h = fopen(NAMES_H, "w");
	if (!g->names_h)
		err(1, "can't write to: %s", NAMES_H);	

	fputs(
			"/* generated by tl_generator */\n"
			"#ifndef TL_NAMES_H_\n"
			"#define TL_NAMES_H_\n\n"
			"#include <stdio.h>\n"
			,g->names_h);
	
	fputs("struct TL_METHOD_NAME{char *name; unsigned int id;};\n", g->names_h);
	
	fputs("static struct TL_METHOD_NAME TL_METHOD_NAMES[] = {\n", g->names_h);
	return 0;
}

int append_names_header(
		generator_t *g, const struct method_t *m)
{
	char buf[BLEN];
	fputs(STR(buf, BLEN,
				"\t{(char *)\"%s\", 0x%.8x},\n"
				,m->name, m->id),
		 	g->names_h);
	return 0;
}

int close_names_header(generator_t *g)
{
	fputs(
			"\t{NULL, 0}\n"
			"};\n\n"
			"static const char * TL_NAME_FROM_ID(int id){\n"
			"\tint i = 0;\n"
			"\tstruct TL_METHOD_NAME *names = TL_METHOD_NAMES;\n"
			"\twhile(names[i].name){\n"
			"\t\tif (id == names[i].id)\n"
			"\t\t\treturn names[i].name;\n"
			"\t\ti++;\n"
			"\t}\n"
			"\treturn NULL;\n"
			"}\n\n"
			"#endif // TL_NAMES_H_\n"
			, g->names_h);
	fclose(g->names_h);
	return 0;
}

int open_free_header(generator_t *g)
{
	g->free_h = fopen(FREE_H, "w");
	if (!g->free_h)
		err(1, "can't write to: %s", FREE_H);	

	fputs("/* generated by tl_generator */\n\n",
		 	g->free_h);
	
	fputs("#include \"tl.h\"\n\n"
		 	, g->free_h);
	
	fputs("#define TL_FREES \\\n",
		 	g->free_h);
	
	return 0;
}

int append_free_header(
		generator_t *g, const struct method_t *m)
{
	char buf[BLEN];
	fputs(STR(buf, BLEN,
				"\tTL_FREE(0x%.8x, tl_%s_free) \\\n",
				m->id, m->name),
		 	g->free_h);

	return 0;
}

int close_free_header(generator_t *g)
{
	char buf[BLEN];
	
	fputs("\n\ntypedef void tl_free_function(tl_t*);\n"
			,g->free_h);
		
	fputs("\n\n#define TL_FREE(id, name) \\\n"
			"tl_free_function name;\n"
			"TL_FREES\n"
			"#undef TL_FREE\n"
		 	, g->free_h);

	fputs("\n\nstruct tl_free {unsigned int id; "
				"tl_free_function *fun;};\n"
				"static struct tl_free tl_free_table[] = \n"
				"{\n",
		 	g->free_h);

	fputs("#define TL_FREE(id, name) \\\n"
				"{id, name},\n"
				"TL_FREES\n"
				,g->free_h);
	
	fputs("};\n#undef TL_FREE\n\n",
		 	g->free_h);
	fclose(g->free_h);
	return 0;
}

int open_free(generator_t *g)
{
	g->free_c = fopen(FREE_C, "w");
	if (!g->free_c)
		err(1, "can't write to: %s", FREE_C);	

	fputs("/* generated by tl_generator */\n\n",
		 	g->free_c);
	
	fputs("#include \"free.h\"\n"
	      "#include \"types.h\"\n"
	      "#include <stdlib.h>\n"
	      "#include \"struct.h\"\n\n"
		 	, g->free_c);
	
	return 0;
}

int append_free(
		generator_t *g, const struct method_t *m)
{
	int i;
	char buf[BLEN];
	
	//name
	fputs(STR(buf, BLEN, "void tl_%s_free(tl_t *tl_) {\n"
				, m->name),
		 	g->free_c);
	
	fputs(STR(buf, BLEN, "\ttl_%s_t *tl = (tl_%s_t *)tl_;\n"
				, m->name, m->name),
		 	g->free_c);
	
	fputs("\tint i;\n", g->free_c);
	
	if (strcmp(m->name, "vector") == 0){
		fputs("\tbuf_free(tl->data_);\n", g->free_c);
	}

	// args
	for (i = 0; i < m->argc; ++i) {
		// skip flags
		if (strstr(m->args[i].name, "flags") &&
				m->args[i].type == NULL)
			continue;
	
		char nbuf[64];
		char *type = m->args[i].type;
		char *name = STR(nbuf, 64, "%s_", m->args[i].name);
		char *pointer = "";
		char *len = "";
		if (!type)
			type = "int";

		char *p = strstr(type, "ector<");
		if (p){
			// change Vector to pointer
			p += strlen("ector<");
			type = strndup(p, strstr(p, ">") - p);
			fputs(STR(buf, BLEN, "\tfor(i=0; i<tl->%slen; ++i){\n"
						, name),
					g->free_c);
			
			if (strcmp(type, "bytes") &&
					strcmp(type, "string") &&
					strcmp(type, "true") &&
					strcmp(type, "bool") &&
					strcmp(type, "int") &&
					strcmp(type, "long") &&
					strcmp(type, "int128") &&
					strcmp(type, "int256") &&
					strcmp(type, "\%Message") &&
					strcmp(type, "double"))
			{
				fputs(STR(buf, BLEN, "\t\ttl_free(tl->%s[i]);\n", name),
						g->free_c);
			}
			if (strcmp(type, "bytes") == 0 ||
					strcmp(type, "int256") == 0 ||
					strcmp(type, "int128") == 0)
			{
				fputs(STR(buf, BLEN, "\t\tbuf_free(tl->%s[i]);\n", name),
						g->free_c);
			}
			
			if (strcmp(type, "string") == 0)
			{
				fputs(STR(buf, BLEN, "\t\tbuf_free(tl->%s[i]);\n", name),
						g->free_c);
			}
			
			fputs("\t}\n", g->free_c);
	
			fputs(STR(buf, BLEN, "\tfree(tl->%s);\n", name),
						g->free_c);

			continue;
		}

		if (strcmp(type, "bytes") &&
				strcmp(type, "string") &&
				strcmp(type, "true") &&
				strcmp(type, "bool") &&
				strcmp(type, "int") &&
				strcmp(type, "long") &&
				strcmp(type, "int128") &&
				strcmp(type, "int256") &&
				strcmp(type, "\%Message") &&
				strcmp(type, "double"))
		{
			fputs(STR(buf, BLEN, "\ttl_free(tl->%s);\n", name),
					g->free_c);
		}
		
		if (strcmp(type, "bytes") == 0 ||
		    strcmp(type, "int256") == 0 ||
		    strcmp(type, "int128") == 0)
		{
			fputs(STR(buf, BLEN, "\tbuf_free(tl->%s);\n", name),
					g->free_c);
		}
		
		if (strcmp(type, "string") == 0)
		{
			fputs(STR(buf, BLEN, "\tbuf_free(tl->%s);\n", name),
					g->free_c);
		}
	
		if (strcmp(type, "\%Message") == 0)
			fputs(STR(buf, BLEN, "\tbuf_free(tl->%s.body_);\n", name),
					g->free_c);
				
	}
	
	fputs("\tbuf_free(tl->_buf);\n", g->free_c);
	
	fputs("\tfree(tl);\n", g->free_c);

	fputs("}\n\n", g->free_c);
	
	return 0;
}

int close_free(generator_t *g)
{
	fclose(g->free_c);
	return 0;
}


int open_deserialize_table_header(generator_t *g)
{
	g->deserialize_table_h = fopen(DESERIALIZE_TABLE_H, "w");
	if (!g->deserialize_table_h)
		err(1, "can't write to: %s", DESERIALIZE_TABLE_H);	


	fputs("/* generated by tl_generator */\n\n",
		 	g->deserialize_table_h);
	
	fputs("#include \"tl.h\"\n\n",
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
	
	fputs("\n\ntypedef tl_t * tl_deserialize_function"
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

int open_tojson_table_header(generator_t *g)
{
	g->tojson_table_h = fopen(TOJSON_TABLE_H, "w");
	if (!g->tojson_table_h)
		err(1, "can't write to: %s", TOJSON_TABLE_H);	


	fputs("/* generated by tl_generator */\n\n",
		 	g->tojson_table_h);
	
	fputs("#include \"tl.h\"\n\n",
		 	g->tojson_table_h);
	
	fputs("#define TL_TOJSONS \\\n",
		 	g->tojson_table_h);
	
	return 0;
}

int append_tojson_table_header(
		generator_t *g, const struct method_t *m)
{
	char buf[BLEN];
	fputs(STR(buf, BLEN,
				"\tTL_TOJSON(0x%.8x, tl_tojson_%s) \\\n",
				m->id, m->name),
		 	g->tojson_table_h);

	return 0;
}

int close_tojson_table_header(generator_t *g)
{
	char buf[BLEN];
	
	fputs("\n\ntypedef char * tl_tojson_function"
				"(tl_t*);\n"
			,g->tojson_table_h);
		
	fputs("\n\n#define TL_TOJSON(id, name) \\\n"
			"tl_tojson_function name;\n"
			"TL_TOJSONS\n"
			"#undef TL_TOJSON\n"
		 	, g->tojson_table_h);

	fputs("\n\nstruct tl_tojson {unsigned int id; "
				"tl_tojson_function *fun;};\n"
				"static struct tl_tojson tl_tojson_table[] = \n"
				"{\n",
		 	g->tojson_table_h);

	fputs("#define TL_TOJSON(id, name) \\\n"
				"{id, name},\n"
				"TL_TOJSONS\n"
				,g->deserialize_table_h);
	
	fputs("};\n#undef TL_TOJSON\n\n",
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
	      "#include \"tl.h\"\n"
	      "#include \"deserialize.h\"\n"
	      "#include \"struct.h\"\n"
	      "#include \"alloc.h\"\n"
	      "#include \"id.h\"\n",
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
				"tl_t * tl_deserialize_%s(buf_t *buf){\n",
				m->name),
		 	g->deserialize_table_c);

	//fputs(STR(buf, BLEN,
				//"\tprintf(\"deserialize obj: %s (%.8x)\\n\");\n",
				//m->name, m->id),
			 //g->deserialize_table_c);
	
	fputs(
			STR(buf, BLEN,
				"\ttl_%s_t *obj = NEW(tl_%s_t, return NULL);\n"
				, m->name, m->name)
				, g->deserialize_table_c);

	// copy buf
	fputs(
			"\tobj->_buf = buf_add_buf(*buf);\n"
				, g->deserialize_table_c);


	fputs(
				"\tobj->_id = deserialize_ui32(buf);\n"
				, g->deserialize_table_c);

	if (strcmp(m->name, "vector") == 0){
		fputs("\tobj->len_ = deserialize_ui32(buf);\n"
				, g->deserialize_table_c);
		fputs("\tobj->data_ = buf_add_buf(*buf);\n", 
				g->deserialize_table_c);
	}
		
	int i, nflag = 0;
	for (i = 0; i < m->argc; ++i) {
		fputs(
				STR(buf, BLEN,
				"\t// parse arg %s (%s)\n"
				,m->args[i].name, m->args[i].type)
				, g->deserialize_table_c);
		
		//fputs(
				//STR(buf, BLEN,
				//"\tprintf(\"obj: %s: parse arg %s (%s)\\n\");\n"
				//,m->name, m->args[i].name, m->args[i].type)
				//, g->deserialize_table_c);
		

		// check if flag
		void *p;
		if (m->args[i].type == NULL && 
				strstr(m->args[i].name, "flags"))
		{
			fputs(STR(buf, BLEN,
				"\tuint32_t flag%d = deserialize_ui32(buf);\n"
				, ++nflag)
				, g->deserialize_table_c);

			//fputs(STR(buf, BLEN,
				//"\tprintf(\"flag value: %%.32b\\n\", flag%d);\n"
				//, nflag)
				//, g->deserialize_table_c);

		} else if (m->args[i].type == NULL){
			//skip
			continue;
		} else {
			// skip if flag not match
			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN,
					"\tif ((flag%d & (1 << %d)) == (1 << %d))\n"
					,m->args[i].flagn, m->args[i].flagb, m->args[i].flagb)
					, g->deserialize_table_c);
			fputs(
					"\t{\n"
					, g->deserialize_table_c);

			/*
			if (m->args[i].flagn)
				fputs(
						STR(buf, BLEN,
						"\t\tprintf(\"\\targ is in flag: %.32b\\n\");\n"
						, (1 << m->args[i].flagb))
						, g->deserialize_table_c);
			else 
				fputs("\t\tprintf(\"\\targ is mandatory\\n\");\n", g->deserialize_table_c);
				*/

			// handle arguments
			char *p;
			{
				if (strcmp(m->args[i].type, "int") == 0)
				{
					fputs(STR(buf, BLEN,
						"\t\tobj->%s_ = deserialize_ui32(buf);\n"
						, m->args[i].name)
						, g->deserialize_table_c);
				}
				else if (strcmp(m->args[i].type, "true") == 0)
				{
					// skip buffer update
					fputs(STR(buf, BLEN,
					  "\t\tobj->%s_ = true;\n"
					, m->args[i].name)
					, g->deserialize_table_c);
				}
				else if (strcmp(m->args[i].type, "bool") == 0)
				{
					fputs(STR(buf, BLEN,
					  "\t\tobj->%s_ = deserialize_ui32(buf);\n"
					, m->args[i].name)
					, g->deserialize_table_c);
				}
				else if (strcmp(m->args[i].type, "long") == 0)
				{
					fputs(STR(buf, BLEN,
					  "\t\tobj->%s_ = deserialize_ui64(buf);\n"
					, m->args[i].name)
					, g->deserialize_table_c);
				}
				else if (strcmp(m->args[i].type, "double") == 0)
				{
					fputs(STR(buf, BLEN,
					  "\t\tobj->%s_ = deserialize_double(buf);\n"
					, m->args[i].name)
					, g->deserialize_table_c);				
				}
				else if (strcmp(m->args[i].type, "string") == 0)
				{
					fputs(STR(buf, BLEN,
					  "\t\tobj->%s_ = deserialize_bytes(buf);\n"
					, m->args[i].name)
					, g->deserialize_table_c);
				}
				else if (strcmp(m->args[i].type, "bytes") == 0){
					fputs(STR(buf, BLEN,
					  "\t\tobj->%s_ = deserialize_bytes(buf);\n"
					, m->args[i].name)
					, g->deserialize_table_c);
				}
				else if (strcmp(m->args[i].type, "int128") == 0){
					fputs(STR(buf, BLEN,
					  "\t\tobj->%s_ = deserialize_buf(buf, 16);\n"
					, m->args[i].name)
					, g->deserialize_table_c);
				}
				else if (strcmp(m->args[i].type, "int256") == 0){
					fputs(STR(buf, BLEN,
					  "\t\tobj->%s_ = deserialize_buf(buf, 32);\n"
					, m->args[i].name)
					, g->deserialize_table_c);
				}
				else if (
						(p = strstr(m->args[i].type, "ector<")))
				{
					p += strlen("ector<");
					char * type = 
						strndup(p, strstr(p, ">") - p);

					fputs(STR(buf, BLEN,
					  "\t\tobj->%s_len = deserialize_ui32(buf);\n"
					, m->args[i].name)
					, g->deserialize_table_c);

					fputs(STR(buf, BLEN,
					  "\t\tif(obj->%s_len == id_vector)\n"
					  "\t\t\tobj->%s_len = deserialize_ui32(buf); // skip vector definition\n"
					, m->args[i].name, m->args[i].name)
					, g->deserialize_table_c);
					
					//fputs(STR(buf, BLEN,
						//"\t\tprintf(\"\\tvector len: %%d\\n\", obj->%s_len);\n"
					//, m->args[i].name)
					//, g->deserialize_table_c);

					fputs(STR(buf, BLEN,
					  "\t\tif (obj->%s_len < 0 || obj->%s_len > 100) {printf(\"vector len error\\n\"); return (tl_t*)obj;}; // some error\n"
					, m->args[i].name, m->args[i].name)
					, g->deserialize_table_c);

					fputs(STR(buf, BLEN,
					  "\t\tif (obj->%s_len) {\n"
					, m->args[i].name)
					, g->deserialize_table_c);

					// handle Vector
					if (strcmp(type, "int") == 0)
					{
						fputs(STR(buf, BLEN,
							"\t\t\tobj->%s_ = (uint32_t *)MALLOC(obj->%s_len * sizeof(uint32_t), return NULL);\n"
							"\t\t\tint i;\n"
							"\t\t\tfor(i=0; i<obj->%s_len; ++i){\n"
							"\t\t\t\tobj->%s_[i] = deserialize_ui32(buf);\n"
							"\t\t\t}\n"
						, m->args[i].name, m->args[i].name, m->args[i].name, m->args[i].name)
						, g->deserialize_table_c);
					}
					else if (strcmp(type, "bool") == 0)
					{
						fputs(STR(buf, BLEN,
							"\t\t\tobj->%s_ = (bool *)MALLOC(obj->%s_len * sizeof(bool), return NULL);\n"
							"\t\t\tint i;\n"
							"\t\t\tfor(i=0; i<obj->%s_len; ++i){\n"
							"\t\t\t\tobj->%s_[i] = deserialize_ui32(buf);\n"
							"\t\t\t}\n"
						, m->args[i].name, m->args[i].name, m->args[i].name, m->args[i].name)
						, g->deserialize_table_c);
					}
					else if (strcmp(type, "long") == 0)
					{
						fputs(STR(buf, BLEN,
							"\t\t\tobj->%s_ = (uint64_t *)MALLOC(obj->%s_len * sizeof(uint64_t), return NULL);\n"
							"\t\t\tint i;\n"
							"\t\t\tfor(i=0; i<obj->%s_len; ++i){\n"
							"\t\t\t\tobj->%s_[i] = deserialize_ui64(buf);\n"
							"\t\t\t}\n"
						, m->args[i].name, m->args[i].name, m->args[i].name, m->args[i].name)
						, g->deserialize_table_c);
					}
					else if (strcmp(type, "double") == 0)
					{
						fputs(STR(buf, BLEN,
							"\t\t\tobj->%s_ = (double *)MALLOC(obj->%s_len * sizeof(double), return NULL);\n"
							"\t\t\tint i;\n"
							"\t\t\tfor(i=0; i<obj->%s_len; ++i){\n"
							"\t\t\t\tobj->%s_[i] = deserialize_double(buf);\n"
							"\t\t\t}\n"
						, m->args[i].name, m->args[i].name, m->args[i].name, m->args[i].name)
						, g->deserialize_table_c);
					}
					else if (strcmp(type, "string") == 0)
					{
						fputs(STR(buf, BLEN,
							"\t\t\tobj->%s_ = (buf_t *)MALLOC(obj->%s_len * sizeof(buf_t), return NULL);\n"
							"\t\t\tint i;\n"
							"\t\t\tfor(i=0; i<obj->%s_len; ++i){\n"
							"\t\t\t\tobj->%s_[i] = deserialize_bytes(buf);\n"
							"\t\t\t}\n"
						, m->args[i].name, m->args[i].name, m->args[i].name, m->args[i].name)
						, g->deserialize_table_c);
					}
					else if (strcmp(type, "bytes") == 0)
					{
						fputs(STR(buf, BLEN,
							"\t\t\tobj->%s_ = (buf_t *)MALLOC(obj->%s_len * sizeof(buf_t), return NULL);\n"
							"\t\t\tint i;\n"
							"\t\t\tfor(i=0; i<obj->%s_len; ++i){\n"
							"\t\t\t\tobj->%s_[i] = deserialize_bytes(buf);\n"
							"\t\t\t}\n"
						, m->args[i].name, m->args[i].name, m->args[i].name, m->args[i].name)
						, g->deserialize_table_c);
					}
					else if (strcmp(type, "\%Message") == 0)
					{
						fputs(STR(buf, BLEN,
							"\t\t\tobj->%s_ = (mtp_message_t *)MALLOC(obj->%s_len * sizeof(mtp_message_t), return NULL);\n"
							"\t\t\tint i;\n"
							"\t\t\tfor(i=0; i<obj->%s_len; ++i){\n"
							"\t\t\t\tobj->%s_[i] = deserialize_mtp_message(buf);\n"
							"\t\t\t}\n"
						, m->args[i].name, m->args[i].name, m->args[i].name, m->args[i].name)
						, g->deserialize_table_c);
					}

					else
					{
						fputs(STR(buf, BLEN,
							"\t\t\tobj->%s_ = (tl_t **)MALLOC(obj->%s_len * sizeof(tl_t *), return NULL);\n"
							"\t\t\tint i;\n"
							"\t\t\tfor(i=0; i<obj->%s_len; ++i){\n"
							"\t\t\t\tobj->%s_[i] = tl_deserialize(buf);\n"
							"\t\t\t}\n"
						, m->args[i].name, m->args[i].name, m->args[i].name, m->args[i].name)
						, g->deserialize_table_c);
					}

					fputs("\t\t}\n"
					, g->deserialize_table_c);


				} else {
					fputs(
						STR(buf, BLEN,
						"\t\tobj->%s_ = tl_deserialize(buf);\n"
						, m->args[i].name)
						, g->deserialize_table_c);
				}
			}
			
			fputs(
				"\t}\n"
				, g->deserialize_table_c);
		}
	}
	fputs("\treturn (tl_t *)obj;\n",	g->deserialize_table_c);
	fputs("}\n\n",	g->deserialize_table_c);
	return 0;
}

int close_deserialize_table(generator_t *g)
{

	fclose(g->deserialize_table_c);
	return 0;
}

int open_struct(generator_t *g)
{
	int i, k;
	char buf[BLEN];

	g->struct_h = fopen(STRUCT_H, "w");
	if (!g->struct_h)
		err(1, "can't write to: %s", STRUCT_H);	

	fputs("/* generated by tl_generator */\n\n",
		 	g->struct_h);
	
	fputs("#ifndef TL_STRUCT_H\n"
	      "#define TL_STRUCT_H\n\n"
		 	,g->struct_h);
	
	fputs("#include \"tl.h\"\n"
	         "#include <stdbool.h>\n"
	         "#include \"types.h\"\n\n"
				,g->struct_h);
	return 0;
}

int append_struct(
		generator_t *g, const struct method_t *m)
{
	int i;
	char buf[BLEN];
	
	//name
	fputs(STR(buf, BLEN, "typedef struct tl_%s_ {\n", m->name),
		 	g->struct_h);
	
	fputs("\tuint32_t _id;\n", g->struct_h);
	fputs("\tbuf_t _buf;\n", g->struct_h);

	if (strcmp(m->name, "vector") == 0){
		fputs("\tuint32_t len_;\n", g->struct_h);
		fputs("\tbuf_t data_;\n", g->struct_h);
	}

	// args
	for (i = 0; i < m->argc; ++i) {
		// skip flags
		if (strstr(m->args[i].name, "flags") &&
				m->args[i].type == NULL)
			continue;
	
		char nbuf[64];
		char *type = m->args[i].type;
		char *name = STR(nbuf, 64, "%s_", m->args[i].name);
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
			len = STR(buf, 64, "; int %slen", name);
		}

		if (strcmp(type, "bytes") &&
				strcmp(type, "string") &&
				strcmp(type, "true") &&
				strcmp(type, "bool") &&
				strcmp(type, "int") &&
				strcmp(type, "long") &&
				strcmp(type, "int128") &&
				strcmp(type, "int256") &&
				strcmp(type, "\%Message") &&
				strcmp(type, "double"))
			type = "tl_t *";
		
		if (strcmp(type, "bytes") == 0 ||
		    strcmp(type, "int256") == 0 ||
		    strcmp(type, "int128") == 0)
			type = "buf_t";
		
		if (strcmp(type, "string") == 0)
			type = "string_t";
		
		if (strcmp(type, "true") == 0)
			type = "bool";
		
		if (strcmp(type, "long") == 0)
			type = "uint64_t";
		
		if (strcmp(type, "int") == 0)
			type = "uint32_t";
		
		if (strcmp(type, "false") == 0)
			type = "bool";
		
		if (strcmp(type, "\%Message") == 0)
			type = "mtp_message_t";
				
		fputs(STR(buf, BLEN, "\t%s ", type),
				g->struct_h);
		fputs(STR(buf, BLEN, "%s%s%s", pointer, name, len),
		  	g->struct_h);
		fputs(";\n", g->struct_h);
	}

	fputs(STR(buf, BLEN, "} tl_%s_t;\n\n", m->name),
		 	g->struct_h);
	
	return 0;
}

int close_struct(generator_t *g)
{
	fputs("\n#endif // TL_STRUCT_H\n", g->struct_h);
	fclose(g->struct_h);
	return 0;
}


int open_types_header(generator_t *g)
{
	g->type_names = 
		array_new(char*, perror("array_new"); return 1);	

	g->types_h = fopen(TYPES_H, "w");
	if (!g->types_h)
		err(1, "can't write to: %s", TYPES_H);	

	fputs("/* generated by tl_generator */\n\n"
	      "#include \"tl.h\"\n\n"
	      "typedef buf_t int128;\n"
	      "typedef buf_t int256;\n"
	      "typedef buf_t string_t;\n"
	    ,g->types_h);
	
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
			
		STR(buf, BLEN, "typedef buf_t %s;\n", name);

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
					 "#include \"tl.h\"\n"
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
	fputs("buf_t ", g->methods_h);
	
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
		char *name = STR(nbuf, 64, "%s_", m->args[i].name);
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
			len = STR(buf, 64, ", int %slen", name);
		}
		
		// add pointer to flag args
		if (m->args[i].flagn && 
				strcmp(type, "string") && 
				strcmp(type, "true"))
			pointer = "*";

		// add pointer for unknown type
		if (
				strcmp(type, "string") &&
				strcmp(type, "true") &&
				strcmp(type, "bool") &&
				strcmp(type, "int") &&
				strcmp(type, "long") &&
				strcmp(type, "int128") &&
				strcmp(type, "int256") &&
				strcmp(type, "double"))
			pointer = "*";
		
		if (strcmp(type, "bytes") == 0)
			type = "buf_t";
		
		if (strcmp(type, "string") == 0 && g->isMtproto)
			type = "buf_t";
		
		if (strcmp(type, "string") == 0)
			type = "const char *";

		if (strcmp(type, "true") == 0)
			type = "bool";

		if (strcmp(type, "long") == 0)
			type = "uint64_t";
		
		if (strcmp(type, "int") == 0)
			type = "uint32_t";
		
		if (strcmp(type, "false") == 0)
			type = "bool";

		if (strcmp(type, "!X") == 0)
			type = "X";
		
		if (strcmp(type, "Object") == 0)
			type = "X";
		
		if (strcmp(type, "future_salt") == 0)
			type = "FutureSalt";
		
		if (strcmp(type, "\%Message") == 0)
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
			"#include \"serialize.h\"\n"
			"#include \"alloc.h\"\n\n",
		 	g->methods_c);
	
	return 0;
}

int append_methods(
		generator_t *g, const struct method_t *m)
{
	int i;
	char buf[BLEN];

	//return
	fputs("buf_t ", g->methods_c);
	
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
		char *name = STR(nbuf, 64, "%s_", m->args[i].name);
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
			len = STR(buf, 64, ", int %slen", name);
		}

		// add pointer to flag args
		if (m->args[i].flagn && 
				strcmp(type, "string") && 
				strcmp(type, "true"))
			pointer = "*";

		if (
				strcmp(type, "string") &&
				strcmp(type, "true") &&
				strcmp(type, "bool") &&
				strcmp(type, "int") &&
				strcmp(type, "long") &&
				strcmp(type, "int128") &&
				strcmp(type, "int256") &&
				strcmp(type, "double"))
			pointer = "*";
		
		if (strcmp(type, "string") == 0 && g->isMtproto)
			type = "buf_t";
		
		if (strcmp(type, "bytes") == 0)
			type = "buf_t";
		
		if (strcmp(type, "string") == 0)
			type = "const char *";

		if (strcmp(type, "true") == 0)
			type = "bool";
		
		if (strcmp(type, "long") == 0)
			type = "uint64_t";
		
		if (strcmp(type, "int") == 0)
			type = "uint32_t";
		
		if (strcmp(type, "false") == 0)
			type = "bool";
		
		if (strcmp(type, "!X") == 0)
			type = "X";
		
		if (strcmp(type, "Object") == 0)
			type = "X";
		
		if (strcmp(type, "future_salt") == 0)
			type = "FutureSalt";
		
		if (strcmp(type, "\%Message") == 0)
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
	
	// set id
	fputs(STR(buf, BLEN, "\tbuf_t buf = buf_add_ui32(0x%.8x);\n", m->id),
			g->methods_c);

	int nflag = 0;

	// set objects
	for (i = 0; i < m->argc; ++i) {
		fputs(STR(buf, BLEN,
					"\t//parse argument %s (%s)\n"
					, m->args[i].name, m->args[i].type)
				,g->methods_c);
		
		// flags
		char *p;
		if (m->args[i].type == NULL && 
				strstr(m->args[i].name, "flags"))
		{
			// handle flags
			fputs(STR(buf, BLEN,
					"\tuint32_t *flag%d = (uint32_t *)(&buf.data[buf.size]);\n"
					"\tbuf = buf_cat_ui32(buf, 0);\n"
					, ++nflag)
				,g->methods_c);

		} else if (m->args[i].type == NULL){
		
		} else if 
			 (strcmp(m->args[i].type, "true") == 0)
		{
			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n" 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].name, m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

		} else if 
			(strcmp(m->args[i].type, "int")  == 0 ||
			 strcmp(m->args[i].type, "bool") == 0)
		{
			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			if (m->args[i].flagn != 0)
				fputs(
						STR(buf, BLEN, 
							"\t\tbuf = buf_cat_ui32(buf, *%s_);\n", 
							m->args[i].name), 
						g->methods_c);
			else
				fputs(
						STR(buf, BLEN, 
							"\t\tbuf = buf_cat_ui32(buf, %s_);\n", 
							m->args[i].name), 
						g->methods_c);

			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);
		
		} else if (strcmp(m->args[i].type, "int128") == 0){
			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			fputs(
						STR(buf, BLEN, 
							"\t\t\tbuf = buf_cat_data(buf, %s_.data, 16);\n", 
							m->args[i].name), 
						g->methods_c);

			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);
		
		} else if (strcmp(m->args[i].type, "int256") == 0){
			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			fputs(
						STR(buf, BLEN, 
							"\t\t\tbuf = buf_cat_data(buf, %s_.data, 32);\n", 
							m->args[i].name), 
						g->methods_c);

			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);
		
		} else if 
			(strcmp(m->args[i].type, "long")   == 0 )
		{
			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			if (m->args[i].flagn != 0)
				fputs(
						STR(buf, BLEN, 
							"\t\tbuf = buf_cat_ui64(buf, *%s_);\n", 
							m->args[i].name), 
						g->methods_c);
			else
				fputs(
						STR(buf, BLEN, 
							"\t\tbuf = buf_cat_ui64(buf, %s_);\n", 
							m->args[i].name), 
						g->methods_c);


			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);


		} else if 
			(strcmp(m->args[i].type, "double")   == 0 )
		{
			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			if (m->args[i].flagn != 0)
				fputs(
						STR(buf, BLEN, 
							"\t\tbuf = buf_cat_double(buf, *%s_);\n", 
							m->args[i].name), 
						g->methods_c);
			else
				fputs(
						STR(buf, BLEN, 
							"\t\tbuf = buf_cat_double(buf, %s_);\n", 
							m->args[i].name), 
						g->methods_c);


			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);

		} else if (strcmp(m->args[i].type, "string") == 0 && g->isMtproto)
		{
			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			fputs(
					STR(buf, BLEN, 
						"\t\tbuf = buf_cat(buf, serialize_str(%s_));\n", 
						m->args[i].name), 
					g->methods_c);

			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);

		} else if (strcmp(m->args[i].type, "bytes") == 0)
		{

			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			fputs(
					STR(buf, BLEN, 
						"\t\tbuf = buf_cat(buf, serialize_bytes(%s_->data, %s_->size));\n", 
						m->args[i].name, m->args[i].name), 
					g->methods_c);

			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);

		} else if (strcmp(m->args[i].type, "string") == 0){

			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			fputs(
					STR(buf, BLEN, 
						"\t\tbuf = buf_cat(buf, serialize_string(%s_));\n", 
						m->args[i].name), 
					g->methods_c);

			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);

		} else if 
			((p = strstr(m->args[i].type, "ector<"))){
				p += strlen("ector<");
			
			char * type = 
				strndup(p, strstr(p, ">") - p);
		
			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			fputs(
					STR(buf, BLEN, 
						"\t\tbuf = buf_cat(buf, tl_vector());\n"
						"\t\tbuf = buf_cat_ui32(buf, %s_len);\n"
						"\t\tint i;\n"
						"\t\tfor (i=0; i<%s_len; ++i){\n"
						, m->args[i].name, m->args[i].name), 
					g->methods_c);

			if (strcmp(type, "bytes")  == 0)
				fputs(
						STR(buf, BLEN, 
							"\t\t\tbuf = buf_cat(buf, serialize_bytes(%s_[i].data, %s_[i].size));\n", 
							m->args[i].name, m->args[i].name), 
						g->methods_c);

			else if (strcmp(type, "string")  == 0)
				fputs(
						STR(buf, BLEN, 
							"\t\t\tbuf = buf_cat(buf, serialize_string(%s_[i]));\n", 
							m->args[i].name), 
						g->methods_c);

			else if (strcmp(type, "int")   == 0 ||
			         strcmp(type, "bool") == 0) 
				fputs(
						STR(buf, BLEN, 
							"\t\t\tbuf = buf_cat_ui32(buf, %s_[i]);\n", 
							m->args[i].name), 
						g->methods_c);
			
			else if (strcmp(type, "long")   == 0 )
				fputs(
						STR(buf, BLEN, 
							"\t\t\tbuf = buf_cat_ui64(buf, %s_[i]);\n", 
							m->args[i].name), 
						g->methods_c);

			else if (strcmp(type, "double")   == 0 )
				fputs(
						STR(buf, BLEN, 
							"\t\t\tbuf = buf_cat_double(buf, %s_[i]);\n", 
							m->args[i].name), 
						g->methods_c);


			else // type X
				fputs(
						STR(buf, BLEN, 
							"\t\t\tbuf = buf_cat(buf, %s_[i]);\n", 
							m->args[i].name), 
						g->methods_c);

			fputs("\t\t}\n",g->methods_c);

			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);

		} else {
			
			if (m->args[i].flagn != 0)
				fputs(
					STR(buf, BLEN, 
						"\tif (%s_)\n", m->args[i].name), 
					g->methods_c);
			
			fputs("\t{\n",g->methods_c);

			fputs(
					STR(buf, BLEN, 
						"\t\tbuf = buf_cat(buf, *%s_);\n", 
						m->args[i].name), 
					g->methods_c);

			if (m->args[i].flagn)
				fputs(
					STR(buf, BLEN, 
						"\t\t*flag%d |= (1 << %d);\n", 
						m->args[i].flagn, m->args[i].flagb), 
					g->methods_c);

			fputs("\t}\n",g->methods_c);
		}
	}

	fputs("\treturn buf;\n",g->methods_c);
	//
	fputs("}\n\n", g->methods_c);

	return 0;
}

int close_methods(generator_t *g)
{
	fclose(g->methods_c);
	return 0;
}

static int cb(
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
	
	append_id_header(g, m);
	append_methods_header(g, m);
	append_types_header(g, m);
	append_methods(g, m);
	append_deserialize_table_header(g, m);
	append_deserialize_table(g, m);
	append_struct(g, m);
	append_names_header(g, m);
	append_free_header(g, m);
	append_free(g, m);

	return 0;
}

int main(int argc, char *argv[])
{
	generator_t g;

	FILE *api_layer = fopen(API_LAYER_H, "w");
	if (!api_layer)
		return 1;
	fputs("#ifndef API_LAYER\n#define API_LAYER " 
			      API_LAYER "\n#endif", api_layer);
	fclose(api_layer);
	
	if (open_names_header(&g))return 1;
	if (open_methods_header(&g))return 1;
	if (open_id_header(&g))return 1;
	if (open_methods(&g))return 1;
	if (open_types_header(&g)) return 1;
	if (open_struct(&g)) return 1;
	if (open_deserialize_table_header(&g)) return 1;
	if (open_deserialize_table(&g)) return 1;
	if (open_free_header(&g)) return 1;
	if (open_free(&g)) return 1;

	g.isMtproto = 1;
	tl_parse("mtproto_api.tl",
		 	&g,cb);
	
	g.isMtproto = 0;
	tl_parse("telegram_api.tl",
		 	&g, cb);

	close_names_header(&g);
	close_id_header(&g);
	close_methods_header(&g);
	close_methods(&g);
	close_types_header(&g);
	close_struct(&g);
	close_deserialize_table_header(&g);
	close_deserialize_table(&g);
	close_free_header(&g);
	close_free(&g);
	return 0;
}
