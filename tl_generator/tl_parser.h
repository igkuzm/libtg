#ifndef TL_PARSER_H
#define TL_PARSER_H
struct method_t {
	char *name;
	int  id;
	char *ret;
	struct {char *name; char *type; int flag;} args[32];
	int argc;
};

int tl_parse(
		const char *schema_file, 
		void *userdata,
		int (*callback)(
			void *userdata, 
			const struct method_t *m,
			const char *error));

void print_method(const struct method_t *m);

#endif /* ifndef TL_PARSER_H */
