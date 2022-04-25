#if defined(__FreeBSD__)
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h> 
#include <netinet/in.h>

#include "etherbone.h"
#include "litexcnc.h"

struct eb_connection {
    int fd;
    int read_fd;
    int is_direct;
    struct addrinfo* addr;
};


unsigned long timestampUsec()  // this can roll over! for debug only
{
	struct timeval timestamp;
	
	gettimeofday(&timestamp, NULL);
	
	return(timestamp.tv_sec * 1000000 + ( timestamp.tv_usec));
}

// sleep for some microseconds.
// this method will allow for rescheduling
void usecSleep(long usec) {
    long ts1=timestampUsec();
	
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = usec;
	select(0, NULL, NULL, NULL, &tv);
	
    long ts2=timestampUsec();
    long dura = ts2 - ts1; // can roll...
    if ( dura > 0 && dura > usec + 100) {
	    fprintf(stderr, "colorcnc debug: overslept %ld instad of %ld usec\n", dura, usec);
    }
}


void eb_wait_for_tx_buffer_empty(struct eb_connection *conn) {
    int tx_queue_len = 0, first = 1;
    int tx_queue_max_len = 0;
    int peekCount = 0;
    long time1 = timestampUsec();
    
    while ( first || tx_queue_len > 0) {
	    if (tx_queue_len > 0) usecSleep(10);
	    int rc = ioctl(conn->fd, TIOCOUTQ, &tx_queue_len);
	    if ( rc < 0) {
		    perror("colorcnc: cannot ioctl?");
		    exit(1);
	    }
	    if ( tx_queue_len > tx_queue_max_len ) tx_queue_max_len = tx_queue_len;
	    first = 0;
	    ++peekCount;
    }
    
    long time2 = timestampUsec();
    long dura = time2 - time1;
    if (dura > 300) {
	    //fprintf(stderr, "colorcnc debug: waited for socket tx_queue to clear for %ld usec, %d samples, max len %d\n", dura, peekCount, tx_queue_max_len);
    }
    
}

int eb_send(struct eb_connection *conn, const void *bytes, size_t len) {
    if (conn->is_direct)
        return sendto(conn->fd, bytes, len, 0, conn->addr->ai_addr, conn->addr->ai_addrlen);
    return write(conn->fd, bytes, len);

}

int eb_recv(struct eb_connection *conn, void *bytes, size_t max_len) {
    if (conn->is_direct)
        return recvfrom(conn->read_fd, bytes, max_len, 0, NULL, NULL);
    return read(conn->fd, bytes, max_len);
}

int eb_create_packet(uint8_t* eth_pkt, uint32_t address, const uint8_t* data, size_t size, int is_read) {
    // Etherbone header
    eth_pkt[0] = 0x4e;	     // Magic byte 0
    eth_pkt[1] = 0x6f;	     // Magic byte 1
    eth_pkt[2] = 0x10;	     // Version 1, all other flags 0
    eth_pkt[3] = 0x44;	     // Address is 32-bits, port is 32-bits
    eth_pkt[4] = 0;		     // Padding
    eth_pkt[5] = 0;		     // Padding
    eth_pkt[6] = 0;	 	     // Padding
    eth_pkt[7] = 0;	 	     // Padding
    // Record
    eth_pkt[8] = 0;		     // No Wishbone flags are set (cyc, wca, wff, etc.)
    eth_pkt[9] = 0x0f;	     // Byte enable
    
    if (is_read) {
        // Indicate a read action
        eth_pkt[10] = 0;         // Write count 
        eth_pkt[11] = size >> 2; // Read count (in WORD-count, bitshift to divide by 4)
        // Copy the data
        memcpy(&eth_pkt[16], data, size);
    } else {
        // Indicate a write action
        eth_pkt[10] = size >> 2; // Write count (in WORD-count, bitshift to divide by 4)
        eth_pkt[11] = 0;	     // Read count
        // Store the address
        address = htobe32(address);
        memcpy(&eth_pkt[12], &address, sizeof(address));
        // Copy the data
        memcpy(&eth_pkt[16], data, size);
    }
}

int eb_read8(struct eb_connection *conn, uint32_t address, uint8_t* data, size_t size) {
    // Create a buffer for the header (16 bytes) + payload (<size> bytes) and
    // copy the header + data to it
    uint8_t *eth_pkt = malloc((16+size) * sizeof(*eth_pkt));
    memset((void*) eth_pkt, 0, sizeof(eth_pkt));

    // Convert the addresses to a list and set it to the device
    for (size_t i=0; i<size; i+=4) {
        uint32_t temp;
        temp = htobe32(address + i);
        memcpy(&data[i], &temp, 4);
    }
    eb_create_packet(eth_pkt, address, data, size, 1);

    // Send the data to the device
    eb_send(conn, eth_pkt, 16+size);

    // Check response
    uint8_t response[16+size];
    int count = eb_recv(conn, response, sizeof(response));
    if (count != sizeof(response)) {
        fprintf(stderr, "Unexpected read length: %d\n", count);
        return -1;
    }

    // Unpack results
    memcpy(data, &response[16], size);

    // LITEXCNC_PRINT_NO_DEVICE("Read:\n");
    // for (size_t i=0; i<(size); i+=4) {
    //     LITEXCNC_PRINT_NO_DEVICE("%02X %02X %02X %02X\n",
    //         (unsigned char)data[i+0],
    //         (unsigned char)data[i+1],
    //         (unsigned char)data[i+2],
    //         (unsigned char)data[i+3]);
    // }
}


void eb_write8(struct eb_connection *conn, uint32_t address, const uint8_t* data, size_t size) {
    // Create a buffer for the header (16 bytes) + payload (<size> bytes) and
    // copy the header + data to it
    uint8_t *eth_pkt = malloc((16+size) * sizeof(*eth_pkt));
    memset((void*) eth_pkt, 0, sizeof(eth_pkt));
    eb_create_packet(eth_pkt, address, data, size, 0);

    // LITEXCNC_PRINT_NO_DEVICE("Write:\n");
    // for (size_t i=0; i<(16+size); i+=4) {
    //     LITEXCNC_PRINT_NO_DEVICE("%02X %02X %02X %02X\n",
    //         (unsigned char)eth_pkt[i+0],
    //         (unsigned char)eth_pkt[i+1],
    //         (unsigned char)eth_pkt[i+2],
    //         (unsigned char)eth_pkt[i+3]);
    // }

    // Send the data to the device
    eb_send(conn, eth_pkt, 16+size);
}

// https://stackoverflow.com/questions/38071732/how-to-check-if-udp-packet-received-in-c-linux
void eb_discard_pending_packet(struct eb_connection *conn, size_t size)
{
	
    int retval;
    fd_set rfds;
    // NO wait
    struct timeval tv = {0,0};

    uint8_t buffer[size];

    if (!conn->is_direct) {
	    return;
    }

    FD_ZERO(&rfds);
    FD_SET(conn->fd, &rfds);

    retval = select(1, &rfds, NULL, NULL, &tv);

    if (retval == -1) {
        perror("select()");
        exit(1);
    }
    
    if (retval > 0) {
        // read data and ignore
        int rc = eb_recv(conn, buffer, sizeof(buffer));
        fprintf(stderr,"colorcnc: found qeued packet. Ignore that data, size %d\n", rc);
    }
}


int eb_unfill_read32(uint8_t wb_buffer[20]) {
    int buffer;
    uint32_t intermediate;
    memcpy(&intermediate, &wb_buffer[16], sizeof(intermediate));
    intermediate = be32toh(intermediate);
    memcpy(&buffer, &intermediate, sizeof(intermediate));
    return buffer;
}

int eb_fill_readwrite32(uint8_t wb_buffer[20], uint32_t data, uint32_t address, int is_read) {
    memset(wb_buffer, 0, 20);
    wb_buffer[0] = 0x4e;	// Magic byte 0
    wb_buffer[1] = 0x6f;	// Magic byte 1
    wb_buffer[2] = 0x10;	// Version 1, all other flags 0
    wb_buffer[3] = 0x44;	// Address is 32-bits, port is 32-bits
    wb_buffer[4] = 0;		// Padding
    wb_buffer[5] = 0;		// Padding
    wb_buffer[6] = 0;		// Padding
    wb_buffer[7] = 0;		// Padding

    // Record
    wb_buffer[8] = 0;		// No Wishbone flags are set (cyc, wca, wff, etc.)
    wb_buffer[9] = 0x0f;	// Byte enable

    if (is_read) {
        wb_buffer[10] = 0;  // Write count
        wb_buffer[11] = 1;	// Read count
        data = htobe32(address);
        memcpy(&wb_buffer[16], &data, sizeof(data));
    } else {
        wb_buffer[10] = 1;	// Write count
        wb_buffer[11] = 0;  // Read count
        address = htobe32(address);
        memcpy(&wb_buffer[12], &address, sizeof(address));

        data = htobe32(data);
        memcpy(&wb_buffer[16], &data, sizeof(data));
    }
    return 20;
}

int eb_fill_write32(uint8_t wb_buffer[20], uint32_t data, uint32_t address) {
    return eb_fill_readwrite32(wb_buffer, data, address, 0);
}

int eb_fill_read32(uint8_t wb_buffer[20], uint32_t address) {
    return eb_fill_readwrite32(wb_buffer, 0, address, 1);
}



void eb_write32(struct eb_connection *conn, uint32_t val, uint32_t addr) {
    uint8_t raw_pkt[20];
    eb_fill_write32(raw_pkt, val, addr);
    eb_send(conn, raw_pkt, sizeof(raw_pkt));
}

uint32_t eb_read32(struct eb_connection *conn, uint32_t addr) {
    uint8_t raw_pkt[20];
    eb_fill_read32(raw_pkt, addr);

    eb_send(conn, raw_pkt, sizeof(raw_pkt));

    int count = eb_recv(conn, raw_pkt, sizeof(raw_pkt));
    if (count != sizeof(raw_pkt)) {
        fprintf(stderr, "unexpected read length: %d\n", count);
        return -1;
    }
    return eb_unfill_read32(raw_pkt);
}

struct eb_connection *eb_connect(const char *addr, const char *port, int is_direct) {

    struct addrinfo hints;
    struct addrinfo* res = 0;
    int err;
    int sock;

    struct eb_connection *conn = malloc(sizeof(struct eb_connection));
    if (!conn) {
        perror("couldn't allocate memory for eb_connection");
        return NULL;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = is_direct ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = is_direct ? IPPROTO_UDP : IPPROTO_TCP;
    hints.ai_flags = AI_ADDRCONFIG;
    err = getaddrinfo(addr, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "failed to resolve remote socket address (err=%d / %s)\n", err, gai_strerror(err));
        free(conn);
        return NULL;
    }

    conn->is_direct = is_direct;

    if (is_direct) {
        // Rx half
        struct sockaddr_in si_me;

        memset((char *) &si_me, 0, sizeof(si_me));
        si_me.sin_family = res->ai_family;
        si_me.sin_port = ((struct sockaddr_in *)res->ai_addr)->sin_port;
        si_me.sin_addr.s_addr = htobe32(INADDR_ANY);

        int rx_socket;
        if ((rx_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
            fprintf(stderr, "Unable to create Rx socket: %s\n", strerror(errno));
            freeaddrinfo(res);
            free(conn);
            return NULL;
        }
        if (bind(rx_socket, (struct sockaddr*)&si_me, sizeof(si_me)) == -1) {
            fprintf(stderr, "Unable to bind Rx socket to port: %s\n", strerror(errno));
            close(rx_socket);
            freeaddrinfo(res);
            free(conn);
            return NULL;
        }
		struct timeval timeout;
		timeout.tv_sec = 0;        
		timeout.tv_usec = 10000; //FIRST_PACKETS
		err = setsockopt(rx_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
		if (err < 0) {
            close(rx_socket);
            freeaddrinfo(res);
            fprintf(stderr,"etherbone: unable set socket: %s\n", strerror(errno));
            free(conn);
            return NULL;
		}
		
        // Tx half
        int tx_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (tx_socket == -1) {
            fprintf(stderr,"etherbone: Unable to create socket: %s\n", strerror(errno));
            close(rx_socket);
            close(tx_socket);
            freeaddrinfo(res);
            fprintf(stderr,"etherbone: unable to create socket: %s\n", strerror(errno));
            free(conn);
            return NULL;
        }

		timeout.tv_usec = 10000;
		err = setsockopt(tx_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
		if (err < 0) {
            close(rx_socket);
            close(tx_socket);
            freeaddrinfo(res);
            fprintf(stderr,"etherbone: unable set socket: %s\n", strerror(errno));
            free(conn);
            return NULL;
		}

        conn->read_fd = rx_socket;
        conn->fd = tx_socket;
        conn->addr = res;
    }
    else {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            fprintf(stderr,"etherbone:  failed to create socket: %s\n", strerror(errno));
            freeaddrinfo(res);
            free(conn);
            return NULL;
        }

        int connection = connect(sock, res->ai_addr, res->ai_addrlen);
        if (connection == -1) {
            close(sock);
            freeaddrinfo(res);
            fprintf(stderr,"etherbone: unable to create socket: %s\n", strerror(errno));
            free(conn);
            return NULL;
        }

        conn->fd = sock;
        conn->addr = res;
    }

    return conn;
}

void eb_disconnect(struct eb_connection **conn) {
    if (!conn || !*conn)
        return;

    freeaddrinfo((*conn)->addr);
    close((*conn)->fd);
    if ((*conn)->read_fd)
        close((*conn)->read_fd);
    free(*conn);
    *conn = NULL;
    return;
}
