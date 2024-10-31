#include "tg.h"

int tg_connect(
		tg_t *tg,
		void *userdata,
		char * (*callback)(
			void *userdata,
			TG_AUTH auth,
			const tl_t *tl))
{
	// try to load auth_key_id from database
	ui64_t auth_key_id = auth_key_id_from_database(tg);

	if (auth_key_id){
		api.app.open();
		api.log.info(".. new session");
	  shared_rc.ssid = api.buf.rand(8);
		api.srl.ping();
		
		// check if authorized
		InputUser iuser = tl_inputUserSelf();
		buf_t getUsers = tl_users_getUsers(&iuser, 1);	
		buf_t answer = tl_send(getUsers); 
		printf("ANSWER ID: %.8x\n", id_from_tl_buf(answer));
	}

	// try to get phone_number
	char * phone_number = phone_number_from_database(tg);
	if (!phone_number){
		if (callback){
			phone_number = 
					callback(userdata, TG_AUTH_PHONE_NUMBER, NULL);
		if (!phone_number)
			return 1;
		}
		// save to database
		phone_number_to_database(tg, phone_number);
	}

	return 0;

	// if not authorized start authorization
	// get tokens from database 
	
	// authorize with new key
  api.app.open();
  api.log.info(".. new session");
  shared_rc.ssid = api.buf.rand(8);
	api.srl.ping();
	
	CodeSettings codeSettings = tl_codeSettings(
			false,
		 	false,
		 	false,
		 	false,
		 	false, 
			false,
		 	NULL,
		 	0,
		 	NULL,
		 	NULL);

	printf("codeSettings:\n");
	buf_dump(codeSettings);

	buf_t sendCode = 
		tl_auth_sendCode(
				phone_number, 
				tg->apiId, 
				tg->apiHash, 
				&codeSettings);
	
	printf("sendCode:\n");
	buf_dump(sendCode);

	buf_t initConnection = 
		tl_initConnection(
				tg->apiId,
				"libtg", 
				"1.0", 
				"1.0", 
				"ru", 
				"LibTg", 
				"ru", 
				NULL, 
				NULL, 
				&sendCode);
	
	printf("initConnection:\n");
	buf_dump(initConnection);

	buf_t invokeWithLayer = 
		tl_invokeWithLayer(
				API_LAYER, &initConnection);

	buf_t answer = tl_send(invokeWithLayer); 
	if (!answer.size)
		return 1;
	/*printf("ANSWER: %.8x\n", id_from_tl_buf(answer));*/
	
	tl_t *tl = tl_deserialize(&answer);
	switch (tl->_id) {
		case id_auth_sentCode:
			{
				// handle sent code
				if (callback){
					char *code = 
						callback(userdata, TG_AUTH_SENDCODE, tl);
					if (code){
						buf_t signIn = 
							tl_auth_signIn(
									phone_number, 
									((tl_auth_sentCode_t *)tl)->phone_code_hash_, 
									code, 
									NULL);
						buf_t answer = 
							tl_send(signIn); 
						tl_t *tl = tl_deserialize(&answer);
						/*free(code);*/
					}
				}
				/*tl_auth_sentCode_free((tl_auth_sentCode_t *)tl);*/
			}
			break;
		
		default:
			break;
	}

	//free(phone_number);
	return 0;
}