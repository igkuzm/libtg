#ifndef TG_DIALOGS_H_
#define TG_DIALOGS_H_

#include "tg.h"

#define TG_DIALOG_ARGS\
	TG_DIALOG_STR(char*,    name, "TEXT", "name") \
	TG_DIALOG_ARG(bool,     pinned, "INT", "pinned") \
	TG_DIALOG_ARG(bool,     unread_mark, "INT", "unread_mark") \
	TG_DIALOG_ARG(uint32_t, top_message_id, "INT", "top_message_id") \
	TG_DIALOG_ARG(uint32_t, top_message_from_peer_type, "INT", "top_message_from_peer_type") \
	TG_DIALOG_ARG(uint64_t, top_message_from_peer_id, "INT", "top_message_from_peer_id") \
	TG_DIALOG_ARG(uint32_t, top_message_date, "INT", "top_message_date") \
	TG_DIALOG_STR(char*,    top_message_text, "TEXT", "top_message_text") \
	TG_DIALOG_ARG(uint32_t, read_inbox_max_id, "INT", "read_inbox_max_id") \
	TG_DIALOG_ARG(uint32_t, read_outbox_max_id, "INT", "read_outbox_max_id") \
	TG_DIALOG_ARG(uint32_t, unread_count, "INT", "unread_count") \
	TG_DIALOG_ARG(uint32_t, unread_mentions_count, "INT", "unread_mentions_count") \
	TG_DIALOG_ARG(uint32_t, unread_reactions_count, "INT", "unread_reactions_count") \
	TG_DIALOG_ARG(uint32_t, folder_id, "INT", "folder_id") \
	TG_DIALOG_ARG(bool,     silent, "INT", "silent") \
	TG_DIALOG_ARG(uint32_t, mute_until, "INT", "mute_until") \
	TG_DIALOG_ARG(uint32_t, peer_type, "INT", "peer_type") \
	TG_DIALOG_ARG(uint64_t, peer_id, "INT", "peer_id") \
	TG_DIALOG_ARG(uint64_t, photo_id, "INT", "photo_id") \
	TG_DIALOG_STR(char*,    thumb, "TEXT", "thumb") \
	TG_DIALOG_ARG(uint64_t, access_hash, "INT", "access_hash") \


typedef struct tg_dialog_ {
	#define TG_DIALOG_ARG(type, name, ...) type name;
	#define TG_DIALOG_STR(type, name, ...) type name;
	TG_DIALOG_ARGS
	#undef TG_DIALOG_ARG
	#undef TG_DIALOG_STR
} tg_dialog_t;

/* get %limit number of dialogs older then %date and
 * %top_msg_id, callback dialogs array and update messages
 * %hash (if not NULL) 
 * set folder_id NULL to get all folders, pointer to 0 for 
 * non-hidden dialogs, pointer to 1 for hidden dialogs 
 * return number of dialogs*/ 
int tg_get_dialogs(
		tg_t *tg, 
		int limit,
		time_t date, 
		uint64_t * hash, 
		uint32_t *folder_id, 
		void *data,
		int (*callback)(void *data, 
			const tg_dialog_t *dialog));

/* load dialogs to database - set limit to 0 to load all */
void tg_sync_dialogs_to_database(tg_t *tg, int limit, int date,
		void *userdata, void (*on_done)(void *userdata));

/* load all dialogs to database in thread */
void tg_async_dialogs_to_database(tg_t *tg,
		void *userdata, void (*on_done)(void *userdata));

int tg_get_dialogs_from_database(tg_t *tg, void *data,
		int (*callback)(void *data, const tg_dialog_t *dialog));

#endif /* ifndef TG_DIALOGS_H_ */
