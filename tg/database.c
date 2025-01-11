#include "dialogs.h"
#include "user.h"
#include "messages.h"
#include "chat.h"
#include "channel.h"
#include "tg.h"
#include "../tl/str.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "database.h"

// #include <stdint.h>
#if INTPTR_MAX == INT32_MAX
    #define THIS_IS_32_BIT_ENVIRONMENT
		#define _LD_ "%lld"
#elif INTPTR_MAX == INT64_MAX
    #define THIS_IS_64_BIT_ENVIRONMENT
		#define _LD_ "%ld"
#else
    #error "Environment not 32 or 64-bit."
#endif

sqlite3 * tg_sqlite3_open(tg_t *tg) 
{
	sqlite3 *db = NULL;
	int err = sqlite3_open_v2(
			tg->database_path,
		 	&db, 
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, 
			NULL);
	if (err){
		ON_ERR(tg, "%s", (char *)sqlite3_errmsg(db));
		return NULL;
	}

	return db;
}

int tg_sqlite3_prepare(
		tg_t *tg, sqlite3 *db, const char *sql, sqlite3_stmt **stmt) 
{
	int res = sqlite3_prepare_v2(
			db, 
			sql, 
			-1, 
			stmt,
		 	NULL);
	if (res != SQLITE_OK){
		// parse error
		ON_ERR(tg, "%s", sqlite3_errmsg(db));
		return 1;
	}	

	return 0;
}

int tg_sqlite3_exec(
		tg_t *tg, const char *sql) 
{
	sqlite3 *db =	tg_sqlite3_open(tg);
	if (!db)
		return 1;

	char *errmsg = NULL;

	int res = 
		sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	if (errmsg){
		// parse error
		ON_ERR(tg, "%s", errmsg);
		sqlite3_free(errmsg);	
		sqlite3_close(db);
		return 1;
	}	
	if (res != SQLITE_OK){
		// parse error
		ON_ERR(tg, "%s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}

	sqlite3_close(db);
	return 0;
}

int database_init(tg_t *tg, const char *database_path)
{
	sqlite3 *db;
	int err = sqlite3_open_v2(
			tg->database_path, &db, 
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, 
			NULL);
	if (err){
		perror("database init");
		ON_ERR(tg, "%s", (char *)sqlite3_errmsg(db));
		return 1;
	}
	sqlite3_close(db);

	tg_sqlite3_exec(tg, "PRAGMA journal_mode = wal;");
	tg_sqlite3_exec(tg, "PRAGMA busy_timeout = 5000;");

	// create tables
	char sql[] = 
		"CREATE TABLE IF NOT EXISTS phone_numbers (id INT); "
		"CREATE TABLE IF NOT EXISTS auth_tokens (id INT); "
		"CREATE TABLE IF NOT EXISTS auth_keys (id INT); "
		"CREATE TABLE IF NOT EXISTS dialogs_hash (id INT); "
		"CREATE TABLE IF NOT EXISTS messages_hash (id INT); "
		"CREATE TABLE IF NOT EXISTS photos (id INT); "
		"CREATE TABLE IF NOT EXISTS peer_photos (id INT); "
	;
	
	tg_sqlite3_exec(tg, sql);

	//
	tg_messages_create_table(tg);
	tg_dialogs_create_table(tg);
	tg_chat_create_table(tg);
	tg_channel_create_table(tg);
	tg_user_create_table(tg);
	

	return 0;
}

buf_t auth_key_from_database(tg_t *tg)
{
	char sql[BUFSIZ];
	sprintf(sql, 
			"SELECT auth_key FROM auth_keys WHERE id = %d;"
			, tg->id);
	buf_t auth_key;
	memset(&auth_key, 0, sizeof(buf_t));
	tg_sqlite3_for_each(tg, sql, stmt){
		auth_key = buf_add(
			(uint8_t*)sqlite3_column_blob(stmt, 0),
			sqlite3_column_bytes(stmt, 0));
	}
	
	return auth_key;
}

char * phone_number_from_database(tg_t *tg)
{
	char sql[BUFSIZ];
	sprintf(sql, 
			"SELECT phone_number FROM phone_numbers WHERE id = %d;"
			, tg->id);
	char buf[BUFSIZ] = {0};
	tg_sqlite3_for_each(tg, sql, stmt)
		strcpy(buf, (char *)sqlite3_column_text(stmt, 0));

	if (*buf)
		return strdup(buf);
	else
		return NULL;
}

int phone_number_to_database(
		tg_t *tg, const char *phone_number)
{
	char sql[BUFSIZ];

	sprintf(sql, 
			"BEGIN TRANSACTION;"
			"ALTER TABLE \'phone_numbers\' ADD COLUMN \'phone_number\' TEXT; "
			"INSERT INTO \'phone_numbers\' (\'id\') "
			"SELECT %d "
			"WHERE NOT EXISTS (SELECT 1 FROM phone_numbers WHERE id = %d); "
			"UPDATE \'phone_numbers\' SET \'phone_number\' = \'%s\', id = %d; "
			"COMMIT TRANSACTION;"
		,tg->id, tg->id, phone_number, tg->id);
	return tg_sqlite3_exec(tg, sql);
}

char * auth_tokens_from_database(tg_t *tg)
{
	char sql[BUFSIZ];
	sprintf(sql, 
		"SELECT * FROM ((SELECT ROW_NUMBER() OVER (ORDER BY ID) "
		"AS Number, auth_token FROM auth_tokens)) WHERE id = %d "
		"ORDER BY Number DESC "	
		"LIMIT 20;", tg->id);
	struct str s;
	if (str_init(&s))
		return NULL;

	int i = 0;
	tg_sqlite3_for_each(tg, sql, stmt){
		if (i > 0)
			str_append(&s, ";", 1);
		if (sqlite3_column_bytes(stmt, 1) > 0){
			str_append(&s, 
					(char *)sqlite3_column_text(stmt, 1),
					sqlite3_column_bytes(stmt, 1));
			i++;
		}
	}

	if (s.len){
		return s.str;
	} else{
		free(s.str);
		return NULL;
	}
}

int auth_token_to_database(
		tg_t *tg, const char *auth_token)
{
	tg_sqlite3_exec(tg,
"ALTER TABLE \'auth_tokens\' ADD COLUMN \'auth_token\' TEXT; ");
	
	char sql[BUFSIZ];
	sprintf(sql, 
			"INSERT INTO \'auth_tokens\' (id, \'auth_token\') VALUES (%d, \'%s\'); "
		, tg->id, auth_token);
	return tg_sqlite3_exec(tg, sql);
}

int auth_key_to_database(
		tg_t *tg, buf_t auth_key)
{
	tg_sqlite3_exec(tg, 
			"ALTER TABLE \'auth_keys\' ADD COLUMN \'auth_key\' BLOB; ");	
	
	char sql[BUFSIZ];
	sprintf(sql, 
			"BEGIN TRANSACTION;"
			"INSERT INTO \'auth_keys\' (id) "
			"SELECT %d "
			"WHERE NOT EXISTS (SELECT 1 FROM auth_keys WHERE id = %d); "
			, tg->id, tg->id);
	
	tg_sqlite3_exec(tg, sql);
			
	sprintf(sql, 
			"UPDATE \'auth_keys\' SET \'auth_key\' = (?) "
			"WHERE id = %d; "
			"COMMIT TRANSACTION;"
			, tg->id);
	
	sqlite3 *db = tg_sqlite3_open(tg);
	sqlite3_stmt *stmt;
	int res = sqlite3_prepare_v2(
			db, sql, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		ON_ERR(tg, "%s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}	

	res = sqlite3_bind_blob(stmt, 1, auth_key.data, auth_key.size, SQLITE_TRANSIENT);
	if (res != SQLITE_OK) {
		ON_ERR(tg, "%s", sqlite3_errmsg(db));
	}	
	
	sqlite3_step(stmt);
	
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return 0;
}

uint64_t dialogs_hash_from_database(tg_t *tg)
{
	char sql[BUFSIZ];
	sprintf(sql, 
			"SELECT hash FROM dialogs_hash WHERE id = %d;"
			, tg->id);
	uint64_t hash;
	tg_sqlite3_for_each(tg, sql, stmt)
		hash = sqlite3_column_int64(stmt, 0);

	return hash;
}

int dialogs_hash_to_database(tg_t *tg, uint64_t hash)
{
	char sql[BUFSIZ];
	sprintf(sql, 
			"BEGIN TRANSACTION;"
			"ALTER TABLE \'dialogs_hash\' ADD COLUMN \'hash\' INT; "
			"INSERT INTO \'dialogs_hash\' (\'id\') "
			"SELECT %d "
			"WHERE NOT EXISTS (SELECT 1 FROM dialogs_hash WHERE id = %d); "
			"UPDATE \'dialogs_hash\' SET \'hash\' = \'%ld\', id = %d; "
			"COMMIT TRANSACTION;"
		,tg->id, tg->id, hash, tg->id);
	
	return tg_sqlite3_exec(tg, sql);
}

uint64_t messages_hash_from_database(tg_t *tg, uint64_t peer_id)
{
	char sql[BUFSIZ];
	sprintf(sql, 
			"SELECT hash FROM messages_hash WHERE id = %d "
			"AND peer_id = "_LD_";"
			, tg->id, peer_id);
	uint64_t hash;
	tg_sqlite3_for_each(tg, sql, stmt)
		hash = sqlite3_column_int64(stmt, 0);

	return hash;
}

int messages_hash_to_database(tg_t *tg, uint64_t peer_id, uint64_t hash)
{
	char sql[BUFSIZ];
	sprintf(sql, 
			"BEGIN TRANSACTION;"
			"ALTER TABLE \'messages_hash\' ADD COLUMN \'hash\' INT; "
			"ALTER TABLE \'messages_hash\' ADD COLUMN \'peer_id\' INT; "
			"INSERT INTO \'messages_hash\' (\'peer_id\') "
			"SELECT "_LD_" "
			"WHERE NOT EXISTS (SELECT 1 FROM messages_hash WHERE peer_id = "_LD_"); "
			"UPDATE \'messages_hash\' SET \'hash\' = "_LD_", id = %d " 
			"WHERE \'peer_id\' = "_LD_";"
			"COMMIT TRANSACTION;"
		,peer_id, peer_id, hash, tg->id, peer_id);
	
	return tg_sqlite3_exec(tg, sql);
}

char *photo_file_from_database(tg_t *tg, uint64_t photo_id)
{
	char sql[BUFSIZ];
	sprintf(sql, 
			"SELECT data FROM photos WHERE id = %d "
			"AND photo_id = "_LD_";"
			, tg->id, photo_id);
	char *photo = NULL;
	tg_sqlite3_for_each(tg, sql, stmt)
		if (sqlite3_column_bytes(stmt, 0) > 0){
			photo = 
				strndup(	
					(char *)sqlite3_column_text(stmt, 0),
					sqlite3_column_bytes(stmt, 0));
			sqlite3_close(db);
			break;
		}

	return photo;
}

int photo_to_database(tg_t *tg, uint64_t photo_id, const char *data)
{
	tg_sqlite3_exec(tg, 
			"BEGIN TRANSACTION;"
			"ALTER TABLE \'photos\' ADD COLUMN \'data\' TEXT; "
			"ALTER TABLE \'photos\' ADD COLUMN \'photo_id\' INT; "
			);

	struct str sql;
	str_init(&sql);
	str_appendf(&sql, 
			"INSERT INTO \'photos\' (\'photo_id\') "
			"SELECT "_LD_" "
			"WHERE NOT EXISTS (SELECT 1 FROM photos "
			"WHERE photo_id = "_LD_"); "
			"UPDATE \'photos\' SET \'photo_id\' = "_LD_", id = %d, data = \'"
		,photo_id, photo_id, photo_id, tg->id);
	str_append(&sql, data, strlen(data));
	str_appendf(&sql, "\' WHERE photo_id = "_LD_";"
			, photo_id);
	str_appendf(&sql, "COMMIT TRANSACTION;");	
	int ret = tg_sqlite3_exec(tg, sql.str);
	free(sql.str);
	return ret;
}

char *peer_photo_file_from_database(
		tg_t *tg, uint64_t peer_id, uint64_t photo_id)
{
	char sql[BUFSIZ];
	sprintf(sql, 
			"SELECT data FROM peer_photos WHERE id = %d "
			"AND peer_id = "_LD_" AND photo_id = "_LD_";"
			, tg->id, peer_id, photo_id);
	char *photo = NULL;
	tg_sqlite3_for_each(tg, sql, stmt)
		if (sqlite3_column_bytes(stmt, 0) > 0){
			photo = 
				strndup(	
					(char *)sqlite3_column_text(stmt, 0),
					sqlite3_column_bytes(stmt, 0));
			sqlite3_close(db);
			break;
		}

	return photo;
}

int peer_photo_to_database(tg_t *tg, 
		uint64_t peer_id, uint64_t photo_id,
		const char *data)
{
	printf("%s\n", __func__);
	tg_sqlite3_exec(tg, 
			"BEGIN TRANSACTION;"
			"ALTER TABLE \'peer_photos\' ADD COLUMN \'data\' TEXT; "
			"ALTER TABLE \'peer_photos\' ADD COLUMN \'peer_id\' INT; "
			"ALTER TABLE \'peer_photos\' ADD COLUMN \'photo_id\' INT; "
			);
	
	struct str sql;
	str_init(&sql);
	str_appendf(&sql, 
			"INSERT INTO \'peer_photos\' (\'peer_id\') "
			"SELECT "_LD_" "
			"WHERE NOT EXISTS (SELECT 1 FROM peer_photos WHERE peer_id = "_LD_"); "
			"UPDATE \'peer_photos\' SET id = %d, \'photo_id\' = "_LD_", "
			"data = \'"
		,peer_id, peer_id, tg->id, photo_id);
	str_append(&sql, data, strlen(data));
	str_appendf(&sql, "\' WHERE peer_id = "_LD_";"
			, peer_id);
	str_appendf(&sql, "COMMIT TRANSACTION;");	

	fprintf(stderr, "%s: %d\n", __func__, __LINE__);
	int ret = tg_sqlite3_exec(tg, sql.str);
	free(sql.str);
	return ret;
}
