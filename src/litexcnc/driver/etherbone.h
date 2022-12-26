#ifndef __ETHERBONE_H__
#define __ETHERBONE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

/*

The EtherBone record has a struct that looks like this:

struct etherbone_record {
	// 1...
	uint8_t bca : 1;
	uint8_t rca : 1;
	uint8_t rff : 1;
	uint8_t ign1 : 1;
	uint8_t cyc : 1;
	uint8_t wca : 1;
	uint8_t wff : 1;
	uint8_t ign2 : 1;

	uint8_t byte_enable;

	uint8_t wcount;

	uint8_t rcount;

	uint32_t write_addr;
    union {
    	uint32_t value;
        read_addr;
    };
} __attribute__((packed));

This is wrapped inside of an EtherBone network packet header:

struct etherbone_packet {
	uint8_t magic[2]; // 0x4e 0x6f
	uint8_t version : 4;
	uint8_t ign : 1;
	uint8_t no_reads : 1;
	uint8_t probe_reply : 1;
	uint8_t probe_flag : 1;
	uint8_t port_size : 4;
	uint8_t addr_size : 4;
	uint8_t padding[4];

	struct etherbone_record records[0];
} __attribute__((packed));

LiteX only supports a single record per packet, so either wcount or rcount
is set to 1.  For a read, the read_addr is specified.  For a write, the
write_addr is specified along with a value.

The same type of record is returned, so your data is at offset 16.
*/
#define SEND_TIMEOUT_US 10

struct eb_connection;
static const uint8_t etherbone_header[16] = { 0x4e, 0x6f, 0x10, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f };

int eb_send(struct eb_connection *conn, const void *bytes, size_t len);
int eb_recv(struct eb_connection *conn, void *bytes, size_t max_len);

int eb_create_packet(uint8_t* eth_buffer, uint32_t address, const uint8_t* data, size_t size, int is_read);
void eb_write8(struct eb_connection *conn, uint32_t address, const uint8_t* data, size_t size, bool debug);
int eb_read8(struct eb_connection *conn, uint32_t address, uint8_t* data, size_t size, bool debug);
void usecSleep(long usec);

void eb_wait_for_tx_buffer_empty(struct eb_connection *conn);
void eb_discard_pending_packet(struct eb_connection *conn, size_t size);

struct eb_connection *eb_connect(const char *addr, const char *port, int is_direct);
void eb_disconnect(struct eb_connection **conn);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* __ETHERBONE_H__ */
