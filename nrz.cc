
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * the CRC routines are stolen from multimon by Thomas Sailer who stole them
 * from WAMPES by Dieter Deyke 
 */

static const unsigned short crc_ccitt_table[] = {
        0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
        0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
        0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
        0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
        0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
        0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
        0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
        0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
        0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
        0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
        0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
        0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
        0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
        0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
        0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
        0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
        0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
        0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
        0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
        0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
        0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
        0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
        0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
        0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
        0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
        0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
        0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
        0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
        0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
        0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
        0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
        0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static inline bool check_crc_ccitt(const unsigned char *buf, int cnt)
{
        unsigned int crc = 0xffff;

        for (; cnt > 0; cnt--)
                crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ *buf++) & 0xff];
        return (crc & 0xffff) == 0xf0b8;
}

uint encode_prev_bit = 0;
uint decode_prev_bit = 0;

/*
 * nrzi is the logical inverse of nrzm
 */
bool nrzi_encode(unsigned char *buf, uint len)
{
    for (uint i = 0; i < len; i++)
    {
	for (int j = 7; j >= 0; j--)
	{
	    /*
	     * If a 0 is being transmitted, toggle the data bit.  If a logical
	     * 1 is being transmitted, maintain the same bit value.
	     */
	    const uint bit = (buf[i] >> j) & 0x1;
	    if (bit == 0)
	    {
		/*
		 * Toggle the encoded bit.
		 */
		encode_prev_bit = (~encode_prev_bit)&0x1;
		buf[i] &= ~(1<<j);
		buf[i] |= (encode_prev_bit<<j);
	    } else {
		/*
		 * Maintain the last encoded level.
		 */
		buf[i] &= ~(1<<j);
		buf[i] |= (encode_prev_bit<<j);
	    }
	}
    }

    return true;
}

/*
 * nrzi is the logical inverse of nrzm
 */
bool nrzi_decode(unsigned char *buf, uint len)
{
    for (uint i = 0; i < len; i++)
    {
	for (int j = 7; j >= 0; j--)
	{
	    /*
	     * If no transition, a logical 1 has been transmitted.  If the
	     * current bit doesn't equal the previous bit, a logical 0 has been
	     * transmitted.
	     */
	    const uint bit = (buf[i] >> j) & 0x1;
	    if (bit != decode_prev_bit)
	    {
		/*
		 * Clear the bit (logical 0).
		 */
		buf[i] &= ~(1<<j);
	    } else {
		/*
		 * Set the bit (logical 1).
		 */
		buf[i] &= ~(1<<j);
		buf[i] |= (1<<j);
	    }
	    decode_prev_bit = bit;
	}
    }

    return true;
}

bool print_buf(const unsigned char *buf, uint len)
{
    for (uint i = 0; i < len; i++)
    {
	printf("%02x ", buf[i]&0xff);
	if ((i+1)%8 == 0)
	    printf("\n");
    }
    printf("\n");

    return true;
}

bool print_buf_char(const unsigned char *buf, uint len)
{
    for (uint i = 0; i < len; i++)
    {
	printf("%c ", (buf[i]&0xff)+'0');
	if ((i+1)%8 == 0)
	    printf("\n");
    }
    printf("\n");

    return true;
}

uint rx_bit_offset = 0;
uint rx_byte_offset = 0;
unsigned char rx_frame[1024];
unsigned char rxbyte;

enum unstuff
{
    searching,
    synced
};
enum unstuff state = searching;

bool pick_frame(const unsigned char *buf, uint len)
{
//    printf("frame picked! len: %d\n", len);
//    print_buf(buf, len);

    if (!check_crc_ccitt(buf, len))
    {
	printf("crc error in previous frame\n");
    } else {
	printf("frame picked! len: %d\n", len);
	print_buf(buf, len);

	if (len < 15)
	    return true;

	for (uint i = 7; i < 13; i++)
	{
	    if ((buf[i] & 0xfe) != 0x40)
		printf("%c", buf[i]>>1);
	}
	printf(" to ");
	for (uint i = 0; i < 6; i++)
	{
	    if ((buf[i] & 0xfe) != 0x40)
		printf("%c", buf[i]>>1);
	}
	printf("\n");

    }
    //print_buf_char(rx_frame, rx_byte_offset);
}

uint counter = 0;

bool hdlc_rxbit(unsigned char bit)
{
    rxbyte <<= 1;
    rxbyte |= (bit & 0x1);
/*
    printf("%d ", bit);
    if ((counter+1)%8 == 0)
    {
	printf("\n");
    }
    counter++;
*/
    if ((rxbyte & 0xff) == 0x7e)
    {
	if ((state == synced) && 
	    (rx_byte_offset > 2) &&
	    (rx_bit_offset == 7))
	{
	    pick_frame(rx_frame, rx_byte_offset);
	}

	state = synced;
	rx_bit_offset = 0;
	rx_byte_offset = 0;

	return true;
    }
    if ((rxbyte & 0xff) == 0x7f)
    {
	state = searching;
	return true;
    }

    if (state != synced)
	return true;

    if ((rxbyte & 0x3f) == 0x3e)
	return true;

    // MSB
/*
    rx_frame[rx_byte_offset] &= ~(1 << (7-rx_bit_offset));
    rx_frame[rx_byte_offset] |= (bit << (7-rx_bit_offset));
*/
    // LSB
    rx_frame[rx_byte_offset] &= ~(1 << rx_bit_offset);
    rx_frame[rx_byte_offset] |= (bit << rx_bit_offset);


    rx_bit_offset++;
    if (rx_bit_offset >= 8)
    {
	rx_byte_offset++;
	rx_bit_offset = 0;
    }
    
    return true;
}

bool hdlc_unbitstuff(unsigned char *buf, uint len)
{
    for (uint i = 0; i < len; i++)
    {
	for (int j = 7; j >= 0; j--)
	{
	    hdlc_rxbit((buf[i] >> j)&0x1);
	}
    }
}

bool run_tests()
{
    unsigned char buf[] = {0x00, 0x01, 0x02, 0x03};

    nrzi_encode(buf, sizeof(buf));

    print_buf(buf, sizeof(buf));

    nrzi_decode(buf, sizeof(buf));

    print_buf(buf, sizeof(buf));

    return true;
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
	if(!run_tests())
	    return -1;
    }
    else if (argc == 2)
    {
	int fd = open(argv[1], O_RDONLY, 0666);
	if (fd < 0)
	{
	    perror("open error\n");
	    return -1;
	}

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	while (1)
	{
	    if (select(fd+1, &fds, NULL, NULL, NULL) == -1)
	    {
		perror("select error!\n");
		exit(-1);
	    }

	    unsigned char buf[1024];
	    size_t len = read(fd, buf, sizeof(buf));
	    nrzi_decode(buf, len);
	    hdlc_unbitstuff(buf, len);

	    /*
	     * Check for EOF
	     */
	    if (len == 0)
	    {
		printf("EOF reached\n");
		return 0;
	    }
	}
    }
    else
    {
	return -1;
    }

    return 0;
}

