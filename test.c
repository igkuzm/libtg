#include "config.h"
#include <stdio.h>
#include "libtg.h"
#include "mtx/include/api.h"
#include "mtx/include/buf.h"
#include "mtx/include/types.h"
#include "tl/deserialize.h"
#include "tl/id.h"
#include <stdbool.h>
#include <string.h>
#include "tl/alloc.h"
#include "tl/libtl.h"
#include "tl/names.h"

#include "api_id.h"


char * callback(
			void *userdata,
			TG_AUTH auth,
			const tl_t *tl, 
			const char *msg)
{
	switch (auth) {
		case TG_AUTH_PHONE_NUMBER_NEEDED:
			{
				char phone[32];
				printf("enter phone number (+7XXXXXXXXXX): \n");
				scanf("%s", phone);
				return strdup(phone);
			}
			break;
		case TG_AUTH_PHONE_CODE_NEEDED:
			{
				int code;
				printf("enter code: \n");
				scanf("%d", &code);
				printf("code: %d\n", code);
				char phone_code[32];
				sprintf(phone_code, "%d", code);
				return strdup(phone_code);
			}
			break;
		case TG_AUTH_PASSWORD_NEEDED:
			{
				char password[64];
				printf("enter password: \n");
				scanf("%s", password);
				printf("password: %s\n", password);
				return strdup(password);
			}
			break;
		case TG_AUTH_SUCCESS:
			{
				printf("Connected as ");
				tl_user_t *user = (tl_user_t *)tl;
				printf("%s (%s)!\n", 
						(char *)user->username_.data, 
						(char *)user->phone_.data);
			}
			break;
		case TG_AUTH_ERROR:
			{
				if (msg)
					printf("tg_connect error: %s\n", msg);
			}
			break;
		
		case TG_AUTH_INFO:
			{
				if (msg)
					printf("tg_connect info: %s\n", msg);
			}
			break;

		default:
			break;
	}

	return NULL;
}

static void on_err(void *d, tl_t *tl, const char *err)
{
	printf("ERR: %s\n", err);
}

int main(int argc, char *argv[])
{
	int apiId = 0;
	char *apiHash = NULL;

	SETUP_API_ID(apiId)
	SETUP_API_HASH(apiHash)
	
	tg_t *tg = tg_new(
			"test.db", 
			apiId, 
			apiHash);
	
	tg_connect(tg, NULL, callback);

	tl_messages_dialogsSlice_t *md = 
		tg_get_dialogs(tg, NULL, on_err);

	if (md){
		printf("dialogs: %d\n", md->dialogs_len);
		printf("chats: %d\n", md->chats_len);
		printf("users: %d\n", md->users_len);
		printf("messages: %d\n", md->messages_len);
	}

	tg_close(tg);
	return 0;
}
