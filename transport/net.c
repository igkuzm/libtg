/**
 * File              : net.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 21.11.2024
 * Last Modified Date: 25.11.2024
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */
#include "../tg/tg.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

int tg_net_open(tg_t *tg)
{
  struct sockaddr_in serv_addr;
  struct hostent * server;

  tg->sockfd = 
		socket(AF_INET, SOCK_STREAM, 0);

  if (tg->sockfd < 0) {
		ON_ERR(tg, NULL, "%s: can't open socket", __func__);
    return 1;
  }

  server = gethostbyname(tg->ip);

  if (server == 0) {
		ON_ERR(tg, NULL, "%s: no host with ip: '%s'", __func__, tg->ip);
    return -1;
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy(
			(char *)server->h_addr_list[0],
		 	(char *)&serv_addr.sin_addr.s_addr,
		 	server->h_length);
  serv_addr.sin_port = htons(tg->port);

  if (connect(
				tg->sockfd, 
				(struct sockaddr *) &serv_addr, 
				sizeof(serv_addr)) < 0) 
	{
    ON_ERR(tg, NULL, "%s: can't connect", __func__);
    return 1;
  }

	// send intermediate protocol
	/*char init[] = {0xee, 0xee, 0xee, 0xee};*/
	/*send(tg->sockfd, init, 4, 0);*/

	tg->seqn = -1;

	return 0;
}

void tg_net_close(tg_t *tg)
{
  close(tg->sockfd);
}

void tg_net_send(tg_t *tg, const buf_t buf)
{
	ON_LOG_BUF(tg, buf, "%s: ", __func__);
  int32_t n = (int32_t)send(
			tg->sockfd, 
			buf.data, 
			buf.size, 0);

  if (n < 0) {
    ON_ERR(tg, NULL, "%s: can't write to socket", __func__);
  }
}

buf_t tg_net_receive(tg_t *tg)
{
	buf_t data, buf;
	buf_init(&data);
	buf.size = BUFSIZ;	
	do
	{		
			buf_init(&buf);
			buf.size = recv(tg->sockfd,
				 	buf.data, BUFSIZ, 0);
			if (buf.size == 0)
			{
				ON_LOG(tg, "%s: received nothing", __func__);
			}
			else if (buf.size > 0)
			{
				data = buf_cat(data, buf);
				ON_LOG(tg, "%s: received %d bytes", __func__, buf.size);
			}
			else /* < 0 */
			{
				ON_ERR(tg, NULL, "%s: socket error: %d", __func__, buf.size);
			}
			buf_free(buf);
	} while (buf.size == BUFSIZ);

	ON_LOG_BUF(tg, data, "%s: ", __func__);
  return data;
}
