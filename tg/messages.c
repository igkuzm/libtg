#include "messages.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "peer.h"
#include "tg.h"
#include "database.h"
#include "../tl/alloc.h"
#include "user.h"
#include "chat.h"
#include "updates.h"

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

#define BUF2STR(_b) strndup((char*)_b.data, _b.size)
#define BUF2IMG(_b) \
	({buf_t i = image_from_photo_stripped(_b); \
	 buf_to_base64(i);}) 

void tg_message_fwd(
		tg_t *tg, tg_message_fwd_header_t *tgh, tl_messageFwdHeader_t *tlh)
{
	//ON_LOG(tg, "%s: start...", __func__);
	memset(tgh, 0, sizeof(tg_message_fwd_header_t));
	#define TG_MESSAGE_FWD_HEADER_ARG(t, arg, ...) \
		tgh->arg = tlh->arg;
	#define TG_MESSAGE_FWD_HEADER_STR(t, arg, ...) \
		if (tlh->arg.data && tlh->arg.size > 0)\
			tgh->arg = buf_strdup(tlh->arg);
	#define TG_MESSAGE_FWD_HEADER_PER(t, arg, ...) \
	if (tlh->arg){\
		tl_peerUser_t *peer = (tl_peerUser_t *)tlh->arg; \
		tgh->arg = peer->user_id_;\
		tgh->type_##arg = peer->_id;\
	}
	TG_MESSAGE_FWD_HEADER_ARGS
	#undef TG_MESSAGE_FWD_HEADER_ARG
	#undef TG_MESSAGE_FWD_HEADER_STR
	#undef TG_MESSAGE_FWD_HEADER_PER
}

void tg_message_reply(
		tg_t *tg, tg_message_reply_header_t *tgh, 
		tl_messageReplyHeader_t *tlh)
{
	//ON_LOG(tg, "%s: start...", __func__);
	memset(tgh, 0, sizeof(tg_message_reply_header_t));
	#define TG_MESSAGE_REPLY_HEADER_ARG(t, arg, ...) \
		tgh->arg = tlh->arg;
	#define TG_MESSAGE_REPLY_HEADER_STR(t, arg, ...) \
		if (tlh->arg.data && tlh->arg.size > 0)\
			tgh->arg = buf_strdup(tlh->arg);
	#define TG_MESSAGE_REPLY_HEADER_PER(t, arg, ...) \
	if (tlh->arg){\
		tl_peerUser_t *peer = (tl_peerUser_t *)tlh->arg; \
		tgh->arg = peer->user_id_;\
		tgh->type_##arg = peer->_id;\
	}
	#define TG_MESSAGE_REPLY_HEADER_FWD(t, arg, ...) \
	if (tlh->arg && tlh->arg->_id == id_messageFwdHeader){\
		tl_messageFwdHeader_t *h = (tl_messageFwdHeader_t *)tlh->arg; \
		tg_message_fwd(tg, &tgh->arg, h);\
	}
	TG_MESSAGE_REPLY_HEADER_ARGS
	#undef TG_MESSAGE_REPLY_HEADER_ARG
	#undef TG_MESSAGE_REPLY_HEADER_STR
	#undef TG_MESSAGE_REPLY_HEADER_PER
	#undef TG_MESSAGE_REPLY_HEADER_FWD
}

void tg_message_from_tl(
		tg_t *tg, tg_message_t *tgm, tl_message_t *tlm)
{
	//ON_LOG(tg, "%s: start...", __func__);
	memset(tgm, 0, sizeof(tg_message_t));
	
	#define TG_MESSAGE_ARG(t, arg, ...) \
		tgm->arg = tlm->arg;
	#define TG_MESSAGE_STR(t, arg, ...) \
	if (tlm->arg.data && tlm->arg.size > 0)\
		tgm->arg = buf_strdup(tlm->arg);
	#define TG_MESSAGE_PER(t, arg, ...) \
	if (tlm->arg){\
		tl_peerUser_t *peer = (tl_peerUser_t *)tlm->arg; \
		tgm->arg = peer->user_id_;\
		tgm->type_##arg = peer->_id;\
	}
	#define TG_MESSAGE_RPL(t, arg) \
	if (tlm->arg && tlm->arg->_id == id_messageReplyHeader){\
		tl_messageReplyHeader_t *h = (tl_messageReplyHeader_t *)tlm->arg; \
		tg_message_reply(tg, &tgm->arg, h);\
	}
	#define TG_MESSAGE_SPA(...)
	#define TG_MESSAGE_SPS(...)
	#define TG_MESSAGE_SPB(...)
	TG_MESSAGE_ARGS
	#undef TG_MESSAGE_ARG
	#undef TG_MESSAGE_STR
	#undef TG_MESSAGE_PER
	#undef TG_MESSAGE_SPA
	#undef TG_MESSAGE_SPS
	#undef TG_MESSAGE_SPB
	#undef TG_MESSAGE_RPL

	// handle with media
	if (tlm->media_){
		tgm->media_type = tlm->media_->_id;
		switch (tlm->media_->_id) {
			case id_messageMediaPhoto:
				{
					tl_messageMediaPhoto_t *mmp = 
						(tl_messageMediaPhoto_t *)tlm->media_;
					if (mmp->photo_ && mmp->photo_->_id == id_photo)
					{
						tl_photo_t *photo = (tl_photo_t *)mmp->photo_;
						tgm->photo_access_hash = photo->access_hash_; 
						tgm->photo_id = photo->id_;
						tgm->photo_dc_id = photo->dc_id_;
						tgm->photo_date = photo->date_;
						tgm->photo_file_reference = 
							buf_to_base64(photo->file_reference_);

						// get photo sizes
						struct str photo_sizes;
						if (str_init(&photo_sizes))
							break;

						int i;
						for (i = 0; i < photo->sizes_len; ++i) {
							if (!photo->sizes_ || !photo->sizes_[i])
								continue;
							
							switch (photo->sizes_[i]->_id) {
								case id_photoSize:
									{
										tl_photoSize_t *ps = 
											(tl_photoSize_t *)photo->sizes_[i];
										str_appendf(&photo_sizes, "%s=%dx%d "
												, ps->type_.data , ps->w_, ps->h_);
									}
									break;
								case id_photoCachedSize:
									{
										tl_photoCachedSize_t *ps = 
											(tl_photoCachedSize_t *)photo->sizes_[i];
										
										tgm->photo_cached_size = MALLOC(32, break;);
										snprintf(tgm->photo_cached_size,
											 	31, "%dx%d", ps->w_, ps->h_);
									}
									break;
								case id_photoStrippedSize:
									{
										tl_photoStrippedSize_t *ps = 
											(tl_photoStrippedSize_t *)photo->sizes_[i];
										tgm->photo_stripped = BUF2IMG(ps->bytes_);
									}
									break;
								case id_photoPathSize:
									{
										tl_photoPathSize_t *ps = 
											(tl_photoPathSize_t *)photo->sizes_[i];
										tgm->photo_svg = 
											image_from_svg_path(ps->bytes_);
									}
									break;

								default:
									break;
							}
						}
						tgm->photo_sizes = photo_sizes.str;
					}
				}
				break;
			case id_messageMediaGeo:
				{
					tl_messageMediaGeo_t *mmp = 
						(tl_messageMediaGeo_t *)tlm->media_;
					if (mmp->geo_ && mmp->geo_->_id == id_geoPoint)
					{
						tl_geoPoint_t *geo       = (tl_geoPoint_t *)mmp->geo_;
						tgm->geo_long            = geo->long_;
						tgm->geo_lat             = geo->lat_;
						tgm->geo_access_hash     = geo->access_hash_;
						tgm->geo_accuracy_radius = geo->accuracy_radius_;
					}
				}
				break;
			case id_messageMediaDocument:
				{
					tl_messageMediaDocument_t *mmp = 
						(tl_messageMediaDocument_t *)tlm->media_;
					if (mmp->document_ && mmp->document_->_id == id_document)
					{
						tgm->doc_isVoice = mmp->video_;
						tgm->doc_isRound = mmp->round_;
						tgm->doc_isVoice = mmp->voice_;
						
						tl_document_t *doc = (tl_document_t *)mmp->document_;
						tgm->doc_id = doc->id_;
						tgm->doc_access_hash = doc->access_hash_;
						tgm->doc_file_reference =
							buf_to_base64(doc->file_reference_);
						tgm->doc_date = doc->date_;
						tgm->doc_size = doc->size_;
						tgm->doc_dc_id = doc->dc_id_;

						int i;
						// todo get thumbs
						for (i = 0; i < doc->thumbs_len; ++i) {
									
						}
						
						for (i = 0; i < doc->video_thumbs_len; ++i) {
							
						}

						// get attributes
						for (i = 0; i < doc->attributes_len; ++i) {
							tl_t *attr = doc->attributes_[i];
							if (!attr)
								continue;

							switch (attr->_id) {
								case id_documentAttributeImageSize:
									{
										tl_documentAttributeImageSize_t *a =
											(tl_documentAttributeImageSize_t *)attr;
										tgm->doc_w = a->w_;
										tgm->doc_h = a->h_;
									}
									break;
								case id_documentAttributeVideo:
									{
										tl_documentAttributeVideo_t *a =
											(tl_documentAttributeVideo_t *)attr;
										tgm->doc_vw = a->w_;
										tgm->doc_vh = a->h_;
										tgm->doc_vduration = a->duration_;
										tgm->doc_isVideo = true;
									}
									break;
								case id_documentAttributeAudio:
									{
										tl_documentAttributeAudio_t *a =
											(tl_documentAttributeAudio_t *)attr;
										tgm->doc_title = buf_strdup(a->title_); 
										tgm->doc_aduration = a->duration_;
									}
									break;
								case id_documentAttributeFilename:
									{
										tl_documentAttributeFilename_t *a =
											(tl_documentAttributeFilename_t *)attr;
										tgm->doc_file_name =
											buf_strdup(a->file_name_);
									}
									break;
								case id_documentAttributeSticker:
									{
										tl_documentAttributeSticker_t *a =
											(tl_documentAttributeSticker_t *)attr;
										tgm->message_ =
											buf_strdup(a->alt_);
										tgm->is_sticker = true;
									}
									break;
								

								default:
									break;	
							}
						}
					}
				}
				break;
			case id_messageMediaWebPage:
				{
					tl_messageMediaWebPage_t *mmp = 
						(tl_messageMediaWebPage_t *)tlm->media_;
					if (mmp->webpage_ && mmp->webpage_->_id == id_webPage)
					{
						tl_webPage_t *web = (tl_webPage_t *)mmp->webpage_;
						tgm->web_id = web->id_;
						tgm->web_url = buf_strdup(web->url_);
						tgm->web_display_url = buf_strdup(web->display_url_);
						tgm->web_hash = web->hash_;
						tgm->web_type = buf_strdup(web->type_);
						tgm->web_site_name = buf_strdup(web->site_name_);
						tgm->web_title = buf_strdup(web->title_);
						tgm->web_description = buf_strdup(web->description_);

						if (web->photo_ && web->photo_->_id == id_photo)
						{
							tl_photo_t *photo = (tl_photo_t *)web->photo_;
							tgm->web_photo_id = photo->id_;
							tgm->web_photo_access_hash = photo->access_hash_;
							tgm->web_photo_dc_id = photo->dc_id_;
							tgm->web_photo_date = photo->date_;
							tgm->web_photo_file_reference =
								buf_to_base64(photo->file_reference_);
						}
					}
				}
				break;
			case id_messageMediaContact:
				{					
					tl_messageMediaContact_t *mmp = 
						(tl_messageMediaContact_t *)tlm->media_;
					
					tgm->contact_phone_number = 
							buf_strdup(mmp->phone_number_);
					tgm->contact_first_name = 
							buf_strdup(mmp->first_name_);
					tgm->contact_last_name = 
							buf_strdup(mmp->last_name_);
					tgm->contact_vcard = 
							buf_strdup(mmp->vcard_);
					tgm->contact_user_id = mmp->user_id_;
				}
				break;
			
			default:
				break;
		}
	}
}

void tg_message_from_tl_service(
		tg_t *tg, tg_message_t *tgm, tl_messageService_t *tlm)
{
	//ON_LOG(tg, "%s: start...", __func__);
	memset(tgm, 0, sizeof(tg_message_t));

	tgm->is_service = true;
	tgm->out_ = tlm->out_;
	tgm->mentioned_ = tlm->mentioned_;
	tgm->media_unread_ = tlm->media_unread_;
	tgm->silent_ = tlm->silent_;
	tgm->post_ = tlm->post_;
	tgm->legacy_ = tlm->legacy_;
	tgm->id_ = tlm->id_;
	if (tlm->from_id_){
		tl_peerUser_t *peer = (tl_peerUser_t *)tlm->from_id_;
		tgm->from_id_ = peer->user_id_;
		tgm->type_from_id_ = peer->_id;
	}
	if (tlm->peer_id_){
		tl_peerUser_t *peer = (tl_peerUser_t *)tlm->peer_id_;
		tgm->peer_id_ = peer->user_id_;
		tgm->type_peer_id_ = peer->_id;
	}
	if (tlm->reply_to_ && tlm->reply_to_->_id == id_messageReplyHeader){
		tl_messageReplyHeader_t *h = 
			(tl_messageReplyHeader_t *)tlm->reply_to_; 
		tg_message_reply(tg, &tgm->reply_to_, h);
	}
	tgm->date_ = tlm->date_;

	/* TODO: MESSAGE ACTION <01-01-25, yourname> */

	tgm->ttl_period_ = tlm->ttl_period_;
}


void tg_message_from_tl_unknown(
		tg_t *tg, tg_message_t *tgm, tl_t *tlm)
{
	if (!tlm)
		return;
	if (tlm->_id == id_message)
		return tg_message_from_tl(tg, tgm, (tl_message_t *)tlm);
	if (tlm->_id == id_messageService)
		return tg_message_from_tl_service(tg, tgm, (tl_messageService_t *)tlm);
}


static int parse_msgs(
		tg_t *tg, uint64_t peer_id, 
		int argc, tl_t **argv,
		void *data,
		int (*callback)(void *, const tg_message_t*))
{
	int i;
	for (i = 0; i < argc; ++i) {
		if (!argv[i] || argv[i]->_id != id_message)
			continue;
				
		tg_message_t m;
		tg_message_from_tl(tg, &m, (tl_message_t *)argv[i]);
		
		// save message to database
		if (tg_message_to_database(tg, &m) == 0){
			// update hash
			uint64_t hash = 
				messages_hash_from_database(tg, peer_id);
			update_hash(&hash, m.id_);
			messages_hash_to_database(tg, peer_id, hash);
		}

		// callback
		if (callback)
			if (callback(data, &m))
				break;
		
		tg_message_free(&m);
	}

	return i;
}	

int tg_messages_get_history(
		tg_t *tg,
		tg_peer_t peer,
		int offset_id,
		int offset_date,
		int add_offset,
		int limit,
		int max_id,
		int min_id,
		uint64_t *hash,
		void *userdata,
		int (*callback)(void *, const tg_message_t *))
{
	int i, k, c = 0;
	uint64_t h = 0;
	if (hash)
		h = *hash;

	buf_t peer_ = tg_inputPeer(peer); 

	buf_t getHistory = 
		tl_messages_getHistory(
				&peer_, 
				offset_id, 
				offset_date, 
				add_offset, 
				limit, 
				max_id, 
				min_id, 
				h);
	buf_free(peer_);

	tl_t * tl = tg_send_query_sync(tg, &getHistory);
	buf_free(getHistory);

	if (tl == NULL){
		ON_ERR(tg, "%s: tl is NULL", __func__);
		return 0;
	}

	printf("GOT MESSAGES: %s\n", TL_NAME_FROM_ID(tl->_id));

	switch (tl->_id) {
		case id_messages_channelMessages:
			{
				tl_messages_channelMessages_t *msgs = 
					(tl_messages_channelMessages_t *)tl;
				
				// update users
				tg_users_save(tg, msgs->users_len, msgs->users_);
				// update chats
				tg_chats_save(tg, msgs->chats_len, msgs->chats_);

				c = parse_msgs(
						tg, peer.id, 
						msgs->messages_len, 
						msgs->messages_, 
						userdata, 
						callback);
			}
			break;
			
		case id_messages_messages:
			{
				tl_messages_messages_t *msgs = 
					(tl_messages_messages_t *)tl;
				
				// update users
				tg_users_save(tg, msgs->users_len, msgs->users_);
				// update chats
				tg_chats_save(tg, msgs->chats_len, msgs->chats_);
				
				c = parse_msgs(
						tg, peer.id, 
						msgs->messages_len, 
						msgs->messages_, 
						userdata, 
						callback);
			}
			break;
			
		case id_messages_messagesSlice:
			{
				tl_messages_messagesSlice_t *msgs = 
					(tl_messages_messagesSlice_t *)tl;
				
				// update users
				tg_users_save(tg, msgs->users_len, msgs->users_);
				// update chats
				tg_chats_save(tg, msgs->chats_len, msgs->chats_);
				
				c = parse_msgs(
						tg, peer.id, 
						msgs->messages_len, 
						msgs->messages_, 
						userdata, 
						callback);
			}
			break;
			
		default:
			break;
	}

	// free tl
	tl_free(tl);
	
	return c;
}

struct tg_messages_get_history_async_t {
	tg_t *tg;
	tg_peer_t peer;
	void *userdata;
	int (*callback)(void *userdata, const tg_message_t *msg);
	void (*on_done)(void *userdata);
}; 

void tg_messages_get_history_async_cb(void *d, const tl_t *tl)
{
	assert(d);
	struct tg_messages_get_history_async_t *t = d;
	ON_LOG(t->tg, "%s", __func__);
	
	if (tl ==  NULL){
		if (t->on_done)
			t->on_done(t->userdata);
		return;
	}

	ON_LOG(t->tg, "GOT MESSAGES: %s\n", TL_NAME_FROM_ID(tl->_id));

	switch (tl->_id) {
		case id_messages_channelMessages:
			{
				tl_messages_channelMessages_t *msgs = 
					(tl_messages_channelMessages_t *)tl;
				
				// update users
				tg_users_save(t->tg, msgs->users_len, msgs->users_);
				// update chats
				tg_chats_save(t->tg, msgs->chats_len, msgs->chats_);

				parse_msgs(
						t->tg, t->peer.id, 
						msgs->messages_len, 
						msgs->messages_, 
						t->userdata, 
						t->callback);

			}
			break;
			
		case id_messages_messages:
			{
				tl_messages_messages_t *msgs = 
					(tl_messages_messages_t *)tl;
				
				// update users
				tg_users_save(t->tg, msgs->users_len, msgs->users_);
				// update chats
				tg_chats_save(t->tg, msgs->chats_len, msgs->chats_);

				parse_msgs(
						t->tg, t->peer.id, 
						msgs->messages_len, 
						msgs->messages_, 
						t->userdata, 
						t->callback);

			}
			break;
			
		case id_messages_messagesSlice:
			{
				tl_messages_messagesSlice_t *msgs = 
					(tl_messages_messagesSlice_t *)tl;
				
				// update users
				tg_users_save(t->tg, msgs->users_len, msgs->users_);
				// update chats
				tg_chats_save(t->tg, msgs->chats_len, msgs->chats_);

				parse_msgs(
						t->tg, t->peer.id, 
						msgs->messages_len, 
						msgs->messages_, 
						t->userdata, 
						t->callback);
			}
			break;
			
		default:
			break;
	}

	if (t->on_done)
		t->on_done(t->userdata);

	free(t);
}	

pthread_t tg_messages_get_history_async(
		tg_t *tg,
		tg_peer_t peer,
		int offset_id,
		int offset_date,
		int add_offset,
		int limit,
		int max_id,
		int min_id,
		uint64_t *hash,
		void *userdata,
		int (*callback)(void *userdata, const tg_message_t *msg),
		void (*on_done)(void *userdata))
{
	ON_LOG(tg, "%s", __func__);
	int i, k, c = 0;
	uint64_t h = 0;
	if (hash)
		h = *hash;

	buf_t peer_ = tg_inputPeer(peer); 

	buf_t getHistory = 
		tl_messages_getHistory(
				&peer_, 
				offset_id, 
				offset_date, 
				add_offset, 
				limit, 
				max_id, 
				min_id, 
				h);
	buf_free(peer_);

	struct tg_messages_get_history_async_t *t = 
		NEW(struct tg_messages_get_history_async_t, 
				ON_ERR(tg, "%s: can't allocate memory", __func__);
				return 0;);
	t->tg = tg;
	t->peer = peer;
	t->userdata = userdata;
	t->callback = callback;
	t->on_done = on_done;

	pthread_t p = tg_send_query_async(
			tg,
		 	&getHistory, 
			t, 
			tg_messages_get_history_async_cb);
	buf_free(getHistory);
	return p;
}

int tg_message_send(tg_t *tg, tg_peer_t peer_, const char *message)
{
	ON_LOG(tg, "%s", __func__);
	buf_t peer = tg_inputPeer(peer_); 
	buf_t random_id = buf_rand(8);
	buf_t m = tl_messages_sendMessage(
			NULL, 
			NULL, 
			NULL, 
			NULL, 
			NULL, 
			NULL, 
			NULL, 
			&peer, 
			NULL, 
			message, 
			buf_get_ui64(random_id), 
			NULL, 
			NULL, 
			0, 
			NULL, 
			NULL, 
			NULL, 
			NULL);
	buf_free(peer);
	buf_free(random_id);

	tl_t *tl = tg_send_query_sync(tg, &m);
	buf_free(m);

	if (tl == NULL)
	{
		ON_ERR(tg, "%s: answer is NULL", __func__);
		return 1;
	}
	/*if (tl->_id != id_updatesTooLong &&*/
			/*tl->_id != id_updateShortMessage &&*/
			/*tl->_id != id_updateShortChatMessage &&*/
			/*tl->_id != id_updateShort &&*/
			/*tl->_id != id_updatesCombined &&*/
			/*tl->_id != id_updates &&*/
			/*tl->_id != id_updateShortSentMessage)*/
	/*{*/
		/*ON_ERR(tg, "%s: expected Updates but got: %s", */
				/*__func__, TL_NAME_FROM_ID(tl->_id));*/
		/*tl_free(tl);*/
		/*return 1;*/
	/*}*/

	// do updates
	tg_do_updates(tg, tl);
	tl_free(tl);

	return 0;
}

int tg_message_to_database(tg_t *tg, const tg_message_t *m)
{
	ON_LOG(tg, "%s: disabled", __func__);
	return 0;

	ON_LOG(tg, "%s", __func__);
	// save message to database
	pthread_mutex_lock(&tg->databasem); // lock
	struct str s;
	str_init(&s);

	str_appendf(&s,
		"INSERT INTO \'messages\' (\'msg_id\') "
		"SELECT %d "
		"WHERE NOT EXISTS (SELECT 1 FROM \'messages\' WHERE msg_id = %d);\n"
		, m->id_, m->id_);

	str_appendf(&s, "UPDATE \'messages\' SET ");
	
	#define TG_MESSAGE_STR(t, n, type, name) \
	if (m->n && m->n[0]){\
		str_appendf(&s, "\'" name "\'" " = \'"); \
		str_append(&s, (char*)m->n, strlen((char*)m->n)); \
		str_appendf(&s, "\', "); \
	}
		
	#define TG_MESSAGE_ARG(t, n, type, name) \
		str_appendf(&s, "\'" name "\'" " = "_LD_", ", (uint64_t)m->n);
	
	#define TG_MESSAGE_PER(t, n, type, name) \
		str_appendf(&s, "\'" name "\'" " = "_LD_", ", (uint64_t)m->n); \
		str_appendf(&s, "\'type_%s\' = "_LD_", ", name, (uint64_t)m->type_##n);
	
	#define TG_MESSAGE_SPA(t, n, type, name) \
		str_appendf(&s, "\'" name "\'" " = "_LD_", ", (uint64_t)m->n);
	
	#define TG_MESSAGE_SPS(t, n, type, name) \
	if (m->n && m->n[0]){\
		str_appendf(&s, "\'" name "\'" " = \'"); \
		str_append(&s, (char*)m->n, strlen((char*)m->n)); \
		str_appendf(&s, "\', "); \
	}
	#define TG_MESSAGE_RPL(t, n)

	TG_MESSAGE_ARGS
	#undef TG_MESSAGE_ARG
	#undef TG_MESSAGE_STR
	#undef TG_MESSAGE_PER
	#undef TG_MESSAGE_SPA
	#undef TG_MESSAGE_SPS
	#undef TG_MESSAGE_RPL

	str_appendf(&s, "id = %d WHERE msg_id = %d;\n"
			, tg->id, m->id_);
	
	ON_LOG(tg, "%s: msg_id: %d", __func__, m->id_);
	int ret = tg_sqlite3_exec(tg, s.str);
	
	free(s.str);
	
	pthread_mutex_unlock(&tg->databasem); // unlock
	return ret;
}

void tg_messages_create_table(tg_t *tg){
	ON_LOG(tg, "%s", __func__);
	char sql[BUFSIZ]; 
	
	sprintf(sql,
		"CREATE TABLE IF NOT EXISTS messages (id INT, msg_id INT UNIQUE); ");
	ON_LOG(tg, "%s", sql);
	tg_sqlite3_exec(tg, sql);	
	
	#define TG_MESSAGE_ARG(t, n, type, name) \
		sprintf(sql, "ALTER TABLE \'messages\' ADD COLUMN "\
				"\'" name "\' " type ";");\
		ON_LOG(tg, "%s", sql);\
		tg_sqlite3_exec(tg, sql);	
	#define TG_MESSAGE_STR(t, n, type, name) \
		sprintf(sql, "ALTER TABLE \'messages\' ADD COLUMN "\
				"\'" name "\' " type ";");\
		ON_LOG(tg, "%s", sql);\
		tg_sqlite3_exec(tg, sql);	
	#define TG_MESSAGE_PER(t, n, type, name) \
		sprintf(sql, "ALTER TABLE \'messages\' ADD COLUMN "\
				"\'" name "\' " type ";");\
		ON_LOG(tg, "%s", sql);\
		tg_sqlite3_exec(tg, sql);	\
		sprintf(sql, "ALTER TABLE \'messages\' ADD COLUMN "\
				"\'type_%s\' " type ";", name);\
		ON_LOG(tg, "%s", sql);\
		tg_sqlite3_exec(tg, sql);	
	#define TG_MESSAGE_SPA(t, n, type, name) \
		sprintf(sql, "ALTER TABLE \'messages\' ADD COLUMN "\
				"\'" name "\' " type ";");\
		ON_LOG(tg, "%s", sql);\
		tg_sqlite3_exec(tg, sql);
	#define TG_MESSAGE_SPS(t, n, type, name) \
		sprintf(sql, "ALTER TABLE \'messages\' ADD COLUMN "\
				"\'" name "\' " type ";");\
		ON_LOG(tg, "%s", sql);\
		tg_sqlite3_exec(tg, sql);
	#define TG_MESSAGE_RPL(t, n)
	TG_MESSAGE_ARGS
	#undef TG_MESSAGE_ARG
	#undef TG_MESSAGE_STR
	#undef TG_MESSAGE_PER
	#undef TG_MESSAGE_SPA
	#undef TG_MESSAGE_SPS
	#undef TG_MESSAGE_RPL
} 

void tg_message_free(tg_message_t *m)
{
	#define TG_MESSAGE_ARG(t, n, ...)
	#define TG_MESSAGE_STR(t, n, ...) if (m->n) free(m->n);
	#define TG_MESSAGE_PER(t, n, ...)
	#define TG_MESSAGE_SPA(t, n, ...)
	#define TG_MESSAGE_SPS(t, n, ...) if (m->n) free(m->n);
	#define TG_MESSAGE_RPL(t, n)
	TG_MESSAGE_ARGS
	#undef TG_MESSAGE_ARG
	#undef TG_MESSAGE_STR
	#undef TG_MESSAGE_PER
	#undef TG_MESSAGE_SPA
	#undef TG_MESSAGE_SPS
	#undef TG_MESSAGE_RPL
}

int tg_get_messages_from_database(tg_t *tg, tg_peer_t peer, void *data,
		int (*callback)(void *data, const tg_message_t *message))
{
	ON_LOG(tg, "%s", __func__);
	//pthread_mutex_lock(&tg->databasem); // lock
	struct str s;
	str_init(&s);
	str_appendf(&s, "SELECT ");
	
	#define TG_MESSAGE_ARG(t, n, type, name) \
		str_appendf(&s, name ", ");
	#define TG_MESSAGE_STR(t, n, type, name) \
		str_appendf(&s, name ", ");
	#define TG_MESSAGE_PER(t, n, type, name) \
		str_appendf(&s, name ", type_%s, ", name);
	#define TG_MESSAGE_SPA(t, n, type, name) \
		str_appendf(&s, name ", ");
	#define TG_MESSAGE_SPS(t, n, type, name) \
		str_appendf(&s, name ", ");
	#define TG_MESSAGE_RPL(t, n)
	TG_MESSAGE_ARGS
	#undef TG_MESSAGE_ARG
	#undef TG_MESSAGE_STR
	#undef TG_MESSAGE_PER
	#undef TG_MESSAGE_SPA
	#undef TG_MESSAGE_SPS
	#undef TG_MESSAGE_RPL
		
	str_appendf(&s, 
			"id FROM messages WHERE id = %d AND peer_id = "_LD_" "
			"ORDER BY \'date\';", tg->id, peer.id);
	
	//"ORDER BY \'date\' DESC LIMIT 20;", tg->id, peer.id);

	int i = 0;
	tg_sqlite3_for_each(tg, s.str, stmt){
		tg_message_t m;
		memset(&m, 0, sizeof(m));
		
		int col = 0;
		#define TG_MESSAGE_ARG(t, n, type, name) \
			m.n = sqlite3_column_int64(stmt, col++);
		#define TG_MESSAGE_STR(t, n, type, name) \
			if (sqlite3_column_bytes(stmt, col) > 0){ \
				m.n = strndup(\
					(char *)sqlite3_column_text(stmt, col),\
					sqlite3_column_bytes(stmt, col));\
			}\
			col++;
		#define TG_MESSAGE_PER(t, n, type, name) \
			m.n = sqlite3_column_int64(stmt, col); \
			m.type_##n = sqlite3_column_int64(stmt, col); \
			col++; col++;
		#define TG_MESSAGE_SPA(t, n, type, name) \
			m.n = sqlite3_column_int64(stmt, col++);
		#define TG_MESSAGE_SPS(t, n, type, name) \
			if (sqlite3_column_bytes(stmt, col) > 0){ \
				m.n = strndup(\
					(char *)sqlite3_column_text(stmt, col),\
					sqlite3_column_bytes(stmt, col));\
			}\
			col++;
		#define TG_MESSAGE_RPL(t, n)
		TG_MESSAGE_ARGS

		#undef TG_MESSAGE_ARG
		#undef TG_MESSAGE_STR
		#undef TG_MESSAGE_PER
		#undef TG_MESSAGE_SPA
		#undef TG_MESSAGE_SPS
		#undef TG_MESSAGE_RPL

		i++;
		
		if (callback){
			int ret = callback(data, &m);
			if (ret){
				tg_message_free(&m);
				sqlite3_close(db);
				//pthread_mutex_unlock(&tg->databasem); // unlock
				return i;
			}
		}
		// free data
		tg_message_free(&m);
	}	
	
	//pthread_mutex_unlock(&tg->databasem); // unlock
	free(s.str);
	return i;
}

int tg_messages_set_typing(tg_t *tg, tg_peer_t peer_, bool typing)
{
	Peer peer = tg_inputPeer(peer_); 
	SendMessageAction action;
	if (typing){
		action = tl_sendMessageTypingAction();
	} else {
		action = tl_sendMessageCancelAction();
	}

	buf_t setTyping = tl_messages_setTyping(
			&peer,
			NULL,
			&action);
	buf_free(peer);
	buf_free(action);

	tg_send_query_sync(tg, &setTyping);
	buf_free(setTyping);
	return 0;
}

int tg_messages_set_read(tg_t *tg, tg_peer_t peer, uint32_t max_id)
{
	InputPeer inputPeer = tg_inputPeer(peer); 

	buf_t readHistory = tl_messages_readHistory(
			&inputPeer, max_id);
	buf_free(inputPeer);
	
	tg_send_query_sync(tg, &readHistory);
	buf_free(readHistory);

	// update database
	char sql[BUFSIZ];
	sprintf(sql, 
			"UPDATE TABLE \'dialogs\' "
			"SET \'unread_count\' = 0 "
			"WHERE \'peer_id\' = "_LD_" AND id = %d;"
			, peer.id, tg->id);
	pthread_mutex_lock(&tg->databasem); // lock
	tg_sqlite3_exec(tg, sql);
	pthread_mutex_unlock(&tg->databasem); // unlock
	
	/* TODO: messages.AffectedMessages */
	return 0;
}

int tg_messages_get_read_date(
		tg_t *tg, tg_peer_t peer, uint32_t msg_id)
{
	InputPeer inputPeer = tg_inputPeer(peer);
	buf_t query = tl_messages_getOutboxReadDate(
			&inputPeer, msg_id);
	buf_free(inputPeer);

	tl_t *tl = tg_send_query_sync(tg, &query);
	buf_free(query);
	
	if (tl == NULL)
		return -1;

	if (tl->_id == id_rpc_error){
		tl_rpc_error_t *e = (tl_rpc_error_t *)tl;
		if (strcmp((char *)e->error_message_.data, 
				"MESSAGE_NOT_READ_YET") == 0)
		{
			free(tl);
			return 0;
		}
		free(tl);
		return -1;
	}

	if (tl->_id == id_outboxReadDate){
		tl_outboxReadDate_t *ord = 
			(tl_outboxReadDate_t *)tl;
		
		uint32_t date = ord->date_;
		free(tl);
		return date;
	}

	free(tl);
	return -1;
}
