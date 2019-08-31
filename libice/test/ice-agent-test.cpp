#include "sockutil.h"
#include "ice-agent.h"
#include "stun-agent.h"
#include "stun-proto.h"
#include "ice-transport.h"
#include "sys/system.h"
#include "base64.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//#define STUN_SERVER "numb.viagenie.ca"
//#define STUN_SERVER "stun.linphone.org"
#define STUN_SERVER "10.95.49.51"
#define TURN_USR "test"
#define TURN_PWD "12345678"

struct ice_agent_test_t
{
	struct ice_transport_t* avt;
	int stream;
	int component;
	int onbind;
};

static void ice_agent_test_parse_remote_candidates(struct ice_agent_test_t *ctx)
{
	char buffer[4 * 1024];
	printf("Enter remote data (single line, no wrapping)\n>");
	fgets(buffer, sizeof(buffer), stdin);

	char sdp[4 * 1024];
	int n = strlen(buffer);
	assert(n > 0);
	n = base64_decode(sdp, buffer, buffer[n-1]=='\n' ? n-1 : n);
	sdp[n] = 0;
	struct rtsp_media_t m[2];
	n = rtsp_media_sdp(sdp, m, 2);
	assert(ctx->stream == n);
	ice_transport_connect(ctx->avt, m, n);
}

static void ice_agent_test_onbind(void* param, int code)
{
	assert(0 == code);
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;
	char buffer[2 * 1024];
	u_short port;
	char ip[SOCKET_ADDRLEN];
	struct sockaddr_storage addr;
	
	int n = 0;
	for (int i = 0; i < ctx->stream; i++)
	{
		ice_transport_getaddr(ctx->avt, i, 1, &addr);
		socket_addr_to((sockaddr*)&addr, socket_addr_len((sockaddr*)&addr), ip, &port);
		n += snprintf(buffer + n, sizeof(buffer) - n, "m=%s %hu ICE/SDP\n", 0==i?"video":"text", port);
		n += snprintf(buffer + n, sizeof(buffer) - n, "c=IN IP4 %s\n", ip);
		ice_transport_getaddr(ctx->avt, i, 2, &addr);
		socket_addr_to((sockaddr*)&addr, socket_addr_len((sockaddr*)&addr), ip, &port);
		n += snprintf(buffer + n, sizeof(buffer) - n, "a=rtcp:%hu\n", port);
		n += ice_transport_getsdp(ctx->avt, i, buffer + n, sizeof(buffer) - n);
	}

	char base64[3 * 1024];
	n = base64_encode(base64, buffer, n);
	printf("Copy this line to remote client:\n\n  %.*s\n\n", n, base64);
	ctx->onbind = 1;
}

static void ice_agent_test_onconnected(void* param, int64_t streams)
{
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;
	printf("ice connected: %lld\n", (long long)streams);
}

static void ice_agent_test_ondata(void* param, int stream, int component, const void* data, int bytes)
{
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;
	printf("[%d][%d] recv: %.*s\n", stream, component, bytes, (char*)data);
}

extern "C" void ice_agent_test(void)
{
	socket_init();

	struct ice_agent_test_t ctx;
	struct ice_transport_handler_t handler;
	
	memset(&handler, 0, sizeof(handler));
	handler.ondata = ice_agent_test_ondata;
	handler.onbind = ice_agent_test_onbind;
	handler.onconnected = ice_agent_test_onconnected;
	ctx.avt = ice_transport_create(0, &handler, &ctx);
	ctx.onbind = 0;
	ctx.stream = 2;
	ctx.component = 2;

	struct sockaddr_storage stun;
	socklen_t len = sizeof(stun);
	//assert(0 == socket_addr_from(&stun, &len, "numb.viagenie.ca", STUN_PORT));
	assert(0 == socket_addr_from(&stun, &len, STUN_SERVER, STUN_PORT));
	ice_transport_bind(ctx.avt, ctx.stream, ctx.component, (const sockaddr*)&stun, 1, TURN_USR, TURN_PWD);

	while (1)
	{
		if (1 == ctx.onbind)
		{
			ctx.onbind = 2;
			ice_agent_test_parse_remote_candidates(&ctx);
		}

		system_sleep(5);
	}

	ice_transport_destroy(ctx.avt);
	socket_cleanup();
}
