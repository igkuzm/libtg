#include "config.h"
#include <stdio.h>
#include "libtg.h"
#include "mtx/include/api.h"
#include "mtx/include/buf.h"
#include "mtx/include/setup.h"
#include "mtx/include/types.h"
#include "tg/dialogs.h"
#include "tg/files.h"
#include "tg/messages.h"
#include "tg/peer.h"
#include "tg/tg.h"
#include "tl/buf.h"
#include "tl/deserialize.h"
#include "tl/id.h"
#include <stdbool.h>
#include <string.h>
#include "tl/alloc.h"
#include "tl/libtl.h"
#include "tl/names.h"
#include <time.h>
#include <unistd.h>

#include "api_id.h"
#include "tl/tl.h"
#include "transport/net.h"
#include "transport/queue.h"
#include "transport/transport.h"

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
				tl_auth_sentCode_t *sentCode =
					(tl_auth_sentCode_t *)tl;
				
				char *type = NULL;
				switch (sentCode->type_->_id) {
					case id_auth_sentCodeTypeFlashCall:
						type = "FlashCall";
						break;
					case id_auth_sentCodeTypeApp:
						type = "Application";
						break;
					case id_auth_sentCodeTypeCall:
						type = "Call";
						break;
					case id_auth_sentCodeTypeMissedCall:
						type = "MissedCall";
						break;
					case id_auth_sentCodeTypeEmailCode:
						type = "Email";
						break;
					
					default:
						break;
				}

				int code;
				printf("The code was send via %s\n", type);
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

void on_err(void *d, const char *err){
	printf("!!!ERR: %s\n", err);
}

void on_log(void *d, const char *msg){
	printf("%s\n", msg);
}

int messages_callback(void *data, const tg_message_t *m)
{
	printf("%d: %s\n", m->id_, m->message_);
	//if (m->photo_id){
		//printf("HAS PHOTO!\n");
		//tg_message_t *msg = data;
		//*msg = *m;
		//return 1;
	//}
	return 0;
}

int messages_callback2(void *data, int c, const tg_message_t *m)
{
	printf("%d: %s\n", m->id_, m->message_);
	//if (m->photo_id){
		//printf("HAS PHOTO!\n");
		//tg_message_t *msg = data;
		//*msg = *m;
		//return 1;
	//}
	return 0;
}

int file_cb(void *d, const tg_file_t *f){
	int len = 0;
	if (f->bytes_)
		len = strlen(f->bytes_);
	printf("FILE type: %.8x, len: %d\n", f->type_, len);

	return 0;
}

void progress(void *p, int down, int total){
	printf("DOWNLOADED: %d%%\n", down/total*100);
}

void on_done (void *d){
	printf("ON_DONE!\n");
}

int query_cb(void *d, const tl_t *tl){
	if (!tl){
		printf("got nothing\n");
		return 0;
	}

	if (tl && tl->_id == id_user){
		tl_user_t *user = (tl_user_t *)tl;
		printf("USERNAME: %s\n", user->last_name_.data);

		tg_peer_t peer = {
			TG_PEER_TYPE_USER,
			user->id_,
			user->access_hash_
		}; 
		tg_t *tg = d;
		tg_send_message(
				tg, 
				peer, 
				"hello world", 
				NULL, NULL);
	}
		
	return 0;
}

int photo_callback2(void *data, const char *photo)
{
	if (photo)
		printf("PHOTO OK\n");
	else
		printf("PHOTO ERR\n");

	return 0;
}

int dialogs_callback(void *data, const tg_dialog_t *d)
{
	printf("DIALOG: %s\n", d->name);
	/*tg_t *tg = data;*/
	//printf("%lld: %lld\n", d->peer_id, d->photo_id);
	//tg_dialog_t *dialog = data;
	//dialog->name = strdup(d->name);
	//dialog->peer_id = d->peer_id;
	//dialog->peer_type = d->peer_type;
	//dialog->access_hash = d->access_hash;
	//dialog->photo_id = d->photo_id;
	
	/*tg_message_t *msg = */
		/*tg_message_get(tg, d->top_message_id);*/

	/*if (msg){*/
		/*printf("GOT MESSAGE\n");*/
	/*} else {*/
		/*printf("NO MESSAGE\n");*/
	/*}*/
	
	//tg_peer_t peer = {
				//.type = d->peer_type,
				//.id =  d->peer_id,
				//.access_hash = d->access_hash,
	//};
	//tg_get_peer_photo_file(
			//tg,
			//&peer, 
			//false, 
			//d->photo_id,
			//NULL, 
			//photo_callback2);

		return 0;
}


int main(int argc, char *argv[])
{
	int SETUP_API_ID(apiId)
	char * SETUP_API_HASH(apiHash)
	
	tg_t *tg = tg_new(
			"test.db", 
			0,
			apiId, 
			apiHash, "pub.pkcs");

	if (tg_connect(tg, NULL, callback))
		return 1;	
	
	tg_set_on_log  (tg, NULL, on_log);
	tg_set_on_error  (tg, NULL, on_err);

	InputUser iuser = tl_inputUserSelf();
		buf_t getUsers = 
			tl_users_getUsers(&iuser, 1);	

	//tg_queue_manager_send_query(
			//tg, getUsers, 
			//tg, query_cb, 
			//NULL, NULL);

		//tg_peer_t peer = {
		//TG_PEER_TYPE_CHANNEL,
		//1326223284,
		//-5244509236001112417,
	//};

	//tg_get_peer_photo_file(
			//tg, 
			//&peer, 
			//true, 
			//5379844007854718198, 
			//NULL, photo_callback2);

	
	//buf_t h = tg_header(tg, getUsers, true);
	//buf_t e = tg_encrypt(tg, h, true);
	//buf_t t = tg_transport(tg, e);
		
	//tg_net_add_query(tg, t, 0, 
			//NULL, NULL, 
			//NULL, NULL);


	//tg_sync_dialogs_to_database(
			//tg,
			//10, time(NULL),	
			//NULL, 
			//on_done);

	//tg_dialog_t d;
	/*tg_get_dialogs_from_database(tg, tg, */
			/*dialogs_callback);*/

	tg_get_dialogs(tg, 40,
			 time(NULL),
			 NULL, NULL,
			 NULL, dialogs_callback, NULL);

	//printf("NAME: %s\n", d.name);
	//printf("PEER ID: %.16lx\n", d.peer_id);
	//tg_peer_t peer = 
	//{d.peer_type, d.peer_id, d.access_hash};

	//char *image = tg_get_peer_photo_file(
			//tg, 
			//&peer, 
			//false, 
			//d.photo_id);
	//printf("IMAGE: %s\n", image);

	/*tg_message_t m;*/
	/*tg_get_messages_from_database(*/
			/*tg, */
			/*peer, */
			/*&m, */
			/*messages_callback);*/

	//char *image = tg_get_photo_file(
			//tg, 
			//m.photo_id, 
			//m.photo_access_hash, 
			//m.photo_file_reference, 
			//"s");
	//printf("IMAGE: %s\n", image);

	//tg_sync_messages_to_database(
			//tg, 
			//time(NULL), 
			//peer,
			//10,
			//NULL, 
			//on_done);
	//tg_async_dialogs_to_database(tg, 40);
	//sleep(10);
	
	//tg_message_t m;
	/*tg_messages_get_history(*/
			/*tg,*/
			/*peer, */
			/*0, */
			/*time(NULL), */
			/*0, */
			/*20, */
			/*0, */
			/*0, */
			/*NULL, */
			/*NULL, */
			/*messages_callback, on_done);*/

	/*printf("MESSAGE: %s\n", m.message_);*/
	/*printf("file reference: %s\n", m.photo_file_reference);*/

	/*buf_t fr = buf_from_base64(m.photo_file_reference);*/
	/*printf("FILE REF BUF SIZE: %d\n", fr.size);*/
	
	/*InputFileLocation location = */
		/*tl_inputPhotoFileLocation(*/
				/*m.photo_id, */
				/*m.photo_access_hash, */
				/*&fr, */
				/*"s");*/
	
	/*buf_dump(location);*/

	//tg_sync_dialogs_to_database(tg,  10, time(NULL), NULL, on_done);

	//tg_get_dialogs_from_database(tg, tg, 
			//dialogs_callback);
	
	//buf_t peer_ = tg_inputPeer(peer);
	// download photo
	//InputFileLocation location = 
		//tl_inputPeerPhotoFileLocation(
				//true, 
				//&peer_, 
				//d.photo_id);

	//tg_get_file(
			//tg, 
			//&location, 
			//NULL, 
			//file_cb,
			//NULL,
			//progress);
	
	printf("press any key to exit\n");
	getchar();

	tg_close(tg);
	return 0;
}
