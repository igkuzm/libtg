/**
 * File              : dialogs.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 29.11.2024
 * Last Modified Date: 01.12.2024
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */
#include "tg.h"
#include "../tl/id.h"
#include "../mtx/include/net.h"
#include <stdlib.h>
#include <string.h>
#include "time.h"
#include "../tl/alloc.h"

int tg_get_dialogs(
		tg_t *tg, 
		int limit, 
		time_t date, 
		long * hash, 
		int *folder_id, 
		void *data,
		int (*callback)(void *data, 
			const tg_dialog_t *dialog))
{
	int i;
	long h = 0;
	if (hash)
		h = *hash;

	//InputPeer inputPeer = tl_inputPeerSelf();
	InputPeer inputPeer = tl_inputPeerEmpty();
	time_t t = time(NULL);
	if (date)
		t = date;

	buf_t getDialogs = 
		tl_messages_getDialogs(
				NULL,
				folder_id, 
				date,
				0, 
				&inputPeer, 
				limit,
				h);

	tl_t *tl = tg_send_query(tg, getDialogs);
	if (tl && tl->_id == id_messages_dialogsNotModified){
		ON_LOG(tg, "%s: dialogs not modified", __func__);
		return 0;
	}

	if ((tl && tl->_id == id_messages_dialogsSlice) ||
	    (tl && tl->_id == id_messages_dialogs))
	{
		tl_messages_dialogs_t md;

		if ((tl && tl->_id == id_messages_dialogsSlice))
		{
			tl_messages_dialogsSlice_t *mds = 
				(tl_messages_dialogsSlice_t *)tl;
			md.dialogs_ = mds->dialogs_; 
			md.dialogs_len = mds->dialogs_len; 
			md.chats_ = mds->chats_;
			md.chats_len = mds->chats_len;
			md.messages_ = mds->messages_;
			md.messages_len = mds->messages_len;
			md.users_ = mds->users_;
			md.users_len = mds->users_len;
		}

		if (tl && tl->_id == id_messages_dialogs)
		{
			md = *(tl_messages_dialogs_t *)tl;
		}
		
		int k;
		for (k = 0;  k< md.messages_len; ++k) {
			tl_message_t *m = (tl_message_t *)md.messages_[k];
			h = h ^ (h >> 21);
			h = h ^ (h << 35);
			h = h ^ (h >> 4);
			h = h + m->id_;
		}
		if (hash)
			*hash = h;

		for (i = 0; i < md.dialogs_len; ++i) {
			// handle dialogs
			tg_dialog_t d;
			memset(&d, 0, sizeof(d));

			tl_dialog_t dialog;
			memset(&d, 0, sizeof(tl_dialog_t));
			
			if (md.dialogs_[i]->_id == id_dialogFolder){
				tl_dialogFolder_t *df = 
					(tl_dialogFolder_t *)md.dialogs_[i];
				tl_folder_t *folder = 
					(tl_folder_t *)df->folder_;

				dialog.peer_ = df->peer_;
				dialog.top_message_ = df->top_message_;
				dialog.pinned_ = df->pinned_;

			} else if (md.dialogs_[i]->_id != id_dialog){
				ON_LOG(tg, "%s: unknown dialog type: %.8x",
						__func__, md.dialogs_[i]->_id);
				continue;
			}
			dialog = *(tl_dialog_t *)md.dialogs_[i];	
			tl_peerChat_t *peer = (tl_peerChat_t *)dialog.peer_;
			
			d.id = peer->chat_id_;

			int k;

			// iterate users
			for (k = 0; k < md.users_len; ++k) {
				if (md.users_[k]->_id == id_user){
					tl_user_t *user = 
						(tl_user_t *)md.users_[k];
					if (d.id == user->id_){
						d.type = TG_DIALOG_TYPE_USER;
						d.tl = (tl_t *)user;
						if (user->username_.size)
							d.name = 
								strndup((char *)user->username_.data,
										user->username_.size);
						else 
							d.name = 
								strndup((char *)user->first_name_.data,
										user->first_name_.size);
						if (user->photo_ && 
								user->photo_->_id == id_userProfilePhoto)
						{
							tl_userProfilePhoto_t *photo = 
								(tl_userProfilePhoto_t *)user->photo_; 
							d.photo_id = photo->photo_id_;
							d.thumb = 
								image_from_photo_stripped(
										photo->stripped_thumb_);
						}
					}
				} else if (md.users_[k]->_id == id_userEmpty){
					tl_userEmpty_t *user =
						(tl_userEmpty_t *)md.users_[k];
					if (d.id == user->id_){
						d.type = TG_DIALOG_TYPE_USER_EMPTY;
						d.tl = (tl_t *)user;
						d.name = strdup("empty"); 
					}
				}	
			} // done users

			// iterate chats
			for (k = 0; k < md.chats_len; ++k) {
				if (md.chats_[k]->_id == id_channel){
					tl_channel_t *channel = 
						(tl_channel_t *)md.chats_[k];
					if (d.id == channel->id_){
						d.type = TG_DIALOG_TYPE_CHANNEL;
						d.tl = (tl_t *)channel;
						d.name = 
							strndup((char *)channel->title_.data,
									channel->title_.size);
						if (channel->photo_ && 
								channel->photo_->_id == id_chatPhoto)
						{
							tl_chatPhoto_t *photo = 
								(tl_chatPhoto_t *)channel->photo_; 
							d.photo_id = photo->photo_id_;
							d.thumb = 
								image_from_photo_stripped(
										photo->stripped_thumb_);
						}
					}
				}
				else if (md.chats_[k]->_id == id_channelForbidden){
					tl_channelForbidden_t *channel = 
						(tl_channelForbidden_t *)md.chats_[k];
					if (d.id == channel->id_){
						d.type = TG_DIALOG_TYPE_CHANNEL_FORBIDEN;
						d.tl = (tl_t *)channel;
						d.name = 
							strndup((char *)channel->title_.data,
									channel->title_.size);
					}
				}
				else if (md.chats_[k]->_id == id_chat){
					tl_chat_t *chat = 
						(tl_chat_t *)md.chats_[k];
					if (d.id == chat->id_){
						d.type = TG_DIALOG_TYPE_CHAT;
						d.tl = (tl_t *)chat;
						d.name = 
							strndup((char *)chat->title_.data,
									chat->title_.size);
						if (chat->photo_ && 
								chat->photo_->_id == id_chatPhoto)
						{
							tl_chatPhoto_t *photo = 
								(tl_chatPhoto_t *)chat->photo_; 
							d.photo_id = photo->photo_id_;
							d.thumb = 
								image_from_photo_stripped(
										photo->stripped_thumb_);
						}
					}
				}
				else if (md.chats_[k]->_id == id_chatForbidden){
					tl_chatForbidden_t *chat = 
						(tl_chatForbidden_t *)md.chats_[k];
					if (d.id == chat->id_){
						d.type = TG_DIALOG_TYPE_CHAT_FORBIDEN;
						d.tl = (tl_t *)chat;
						d.name = 
							strndup((char *)chat->title_.data,
									chat->title_.size);
					}
				}
			} // done chats
		
			// iterate messages
			for (k = 0; k < md.messages_len; ++k){
				if (md.messages_[k]->_id == id_message){
					tl_message_t *message = 
						(tl_message_t *)md.messages_[k];
					if (message->id_ == dialog.top_message_){
						d.top_message = message;
					}
				}
			} // done messages 

			// callback dialog
			if (d.type == TG_DIALOG_TYPE_NULL) {
				ON_LOG(tg, "%s: can't find dialog data "
						"for peer: %.8x: %ld",
						__func__, peer->_id, peer->chat_id_);
				continue;
			}
			if (callback)
				callback(data, &d);

			// free dialog
			free(d.name);
		} // done dialogs
		
		// free tl
		/* TODO:  <29-11-24, yourname> */
		
		return md.dialogs_len;

	} else { // not dialogs or dialogsSlice
		// throw error
		char *err = tg_strerr(tl); 
		ON_ERR(tg, tl, "%s", err);
		free(err);
		// free tl
		/* TODO:  <29-11-24, yourname> */
	}
	return 0;
}
