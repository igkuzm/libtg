# C99 API for Telegram

```c
char * callback(
    void *userdata,
    TG_AUTH auth,
    tl_t *tl)
{
    switch (auth) {
        case TG_AUTH_PHONE_NUMBER:
            // ask user for phone number 
            // return phone_number; 
            break;
        case TG_AUTH_SENDCODE:
            // ask user for phone code 
            // return phone_code; 
            break;
        case TG_AUTH_PASSWORD_NEEDED:
            // ask user for password 
            // return password; 
            break;
        case TG_AUTH_ERROR:
            // handle error (returned in %tl)
            break; 
        case TG_AUTH_SUCCESS:
            // You are logged in! (current user in %tl)
            break; 
        default:
            break;
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    tg_t *tg = tg_new("test.db", API_ID, API_HASH);

    tg_connect(tg, NULL, callback);
    
    tg_close(tg);

    return 0;
}
```