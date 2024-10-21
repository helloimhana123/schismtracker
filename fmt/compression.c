/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "headers.h"
#include "fmt.h"
#include "bswap.h"

// ------------------------------------------------------------------------------------------------------------
// IT decompression code from itsex.c (Cubic Player) and load_it.cpp (Modplug)
// (I suppose this could be considered a merge between the two.)

static uint32_t it_readbits(int8_t n, uint32_t *bitbuf, uint32_t *bitnum, slurp_t *fp)
{
	uint32_t value = 0;
	uint32_t i = n;

	// this could be better
	while (i--) {
		if (!*bitnum) {
			*bitbuf = slurp_getc(fp);
			*bitnum = 8;
		}
		value >>= 1;
		value |= (*bitbuf) << 31;
		(*bitbuf) >>= 1;
		(*bitnum)--;
	}

	return value >> (32 - n);
}


uint32_t it_decompress8(void *dest, uint32_t len, slurp_t *fp, int it215, int channels)
{
	int8_t *destpos;                // position in destination buffer which will be returned
	uint16_t blklen;                // length of compressed data block in samples
	uint16_t blkpos;                // position in block
	uint8_t width;                  // actual "bit width"
	uint16_t value;                 // value read from file to be processed
	int8_t d1, d2;                  // integrator buffers (d2 for it2.15)
	int8_t v;                       // sample value
	uint32_t bitbuf, bitnum;        // state for it_readbits

	const int64_t startpos = slurp_tell(fp);
	if (startpos < 0)
		return 0; // wat

	const size_t filelen = slurp_length(fp);

	destpos = (int8_t *) dest;

	// now unpack data till the dest buffer is full
	while (len) {
		// read a new block of compressed data and reset variables
		// block layout: word size, <size> bytes data
		{
			int c1 = slurp_getc(fp);
			int c2 = slurp_getc(fp);

			int64_t pos = slurp_tell(fp);
			if (pos < 0)
				return 0;

			if (c1 == EOF || c2 == EOF
				|| pos + (c1 | (c2 << 8)) > filelen)
				return pos - startpos;
		}
		bitbuf = bitnum = 0;

		blklen = MIN(0x8000, len);
		blkpos = 0;

		width = 9; // start with width of 9 bits
		d1 = d2 = 0; // reset integrator buffers

		// now uncompress the data block
		while (blkpos < blklen) {
			if (width > 9) {
				// illegal width, abort
				printf("Illegal bit width %d for 8-bit sample\n", width);
				return slurp_tell(fp);
			}
			value = it_readbits(width, &bitbuf, &bitnum, fp);

			if (width < 7) {
				// method 1 (1-6 bits)
				// check for "100..."
				if (value == 1 << (width - 1)) {
					// yes!
					value = it_readbits(3, &bitbuf, &bitnum, fp) + 1; // read new width
					width = (value < width) ? value : value + 1; // and expand it
					continue; // ... next value
				}
			} else if (width < 9) {
				// method 2 (7-8 bits)
				uint8_t border = (0xFF >> (9 - width)) - 4; // lower border for width chg
				if (value > border && value <= (border + 8)) {
					value -= border; // convert width to 1-8
					width = (value < width) ? value : value + 1; // and expand it
					continue; // ... next value
				}
			} else {
				// method 3 (9 bits)
				// bit 8 set?
				if (value & 0x100) {
					width = (value + 1) & 0xff; // new width...
					continue; // ... and next value
				}
			}

			// now expand value to signed byte
			if (width < 8) {
				uint8_t shift = 8 - width;
				v = (value << shift);
				v >>= shift;
			} else {
				v = (int8_t) value;
			}

			// integrate upon the sample values
			d1 += v;
			d2 += d1;

			// .. and store it into the buffer
			*destpos = it215 ? d2 : d1;
			destpos += channels;
			blkpos++;
		}

		// now subtract block length from total length and go on
		len -= blklen;
	}
	return slurp_tell(fp) - startpos;
}

// Mostly the same as above.
uint32_t it_decompress16(void *dest, uint32_t len, slurp_t *fp, int it215, int channels)
{
	int16_t *destpos;               // position in destination buffer which will be returned
	uint16_t blklen;                // length of compressed data block in samples
	uint16_t blkpos;                // position in block
	uint8_t width;                  // actual "bit width"
	uint32_t value;                 // value read from file to be processed
	int16_t d1, d2;                 // integrator buffers (d2 for it2.15)
	int16_t v;                      // sample value
	uint32_t bitbuf, bitnum;        // state for it_readbits

	const int64_t startpos = slurp_tell(fp);
	if (startpos < 0)
		return 0; // wat

	const size_t filelen = slurp_length(fp);

	destpos = (int16_t *) dest;

	// now unpack data till the dest buffer is full
	while (len) {
		// read a new block of compressed data and reset variables
		// block layout: word size, <size> bytes data
		{
			int c1 = slurp_getc(fp);
			int c2 = slurp_getc(fp);

			int64_t pos = slurp_tell(fp);
			if (pos < 0)
				return 0;

			if (c1 == EOF || c2 == EOF
				|| pos + (c1 | (c2 << 8)) > filelen)
				return pos;
		}

		bitbuf = bitnum = 0;

		blklen = MIN(0x4000, len); // 0x4000 samples => 0x8000 bytes again
		blkpos = 0;

		width = 17; // start with width of 17 bits
		d1 = d2 = 0; // reset integrator buffers

		// now uncompress the data block
		while (blkpos < blklen) {
			if (width > 17) {
				// illegal width, abort
				printf("Illegal bit width %d for 16-bit sample\n", width);
				return slurp_tell(fp);
			}
			value = it_readbits(width, &bitbuf, &bitnum, fp);

			if (width < 7) {
				// method 1 (1-6 bits)
				// check for "100..."
				if (value == (uint32_t) 1 << (width - 1)) {
					// yes!
					value = it_readbits(4, &bitbuf, &bitnum, fp) + 1; // read new width
					width = (value < width) ? value : value + 1; // and expand it
					continue; // ... next value
				}
			} else if (width < 17) {
				// method 2 (7-16 bits)
				uint16_t border = (0xFFFF >> (17 - width)) - 8; // lower border for width chg
				if (value > border && value <= (uint32_t) (border + 16)) {
					value -= border; // convert width to 1-8
					width = (value < width) ? value : value + 1; // and expand it
					continue; // ... next value
				}
			} else {
				// method 3 (17 bits)
				// bit 16 set?
				if (value & 0x10000) {
					width = (value + 1) & 0xff; // new width...
					continue; // ... and next value
				}
			}

			// now expand value to signed word
			if (width < 16) {
				uint8_t shift = 16 - width;
				v = (value << shift);
				v >>= shift;
			} else {
				v = (int16_t) value;
			}

			// integrate upon the sample values
			d1 += v;
			d2 += d1;

			// .. and store it into the buffer
			*destpos = it215 ? d2 : d1;
			destpos += channels;
			blkpos++;
		}

		// now subtract block length from total length and go on
		len -= blklen;
	}
	return slurp_tell(fp) - startpos;
}

// ------------------------------------------------------------------------------------------------------------
// MDL sample decompression

static inline uint16_t mdl_read_bits(uint32_t *bitbuf, uint32_t *bitnum, slurp_t *fp, int8_t n)
{
	uint16_t v = (uint16_t)((*bitbuf) & ((1 << n) - 1) );
	(*bitbuf) >>= n;
	(*bitnum) -= n;
	if ((*bitnum) <= 24) {
		(*bitbuf) |= (((uint32_t)slurp_getc(fp)) << (*bitnum));
		(*bitnum) += 8;
	}
	return v;
}

uint32_t mdl_decompress8(void *dest, uint32_t len, slurp_t *fp)
{
	const int64_t startpos = slurp_tell(fp);
	if (startpos < 0)
		return 0; // wat

	const size_t filelen = slurp_length(fp);

	uint32_t bitnum = 32;
	uint8_t dlt = 0;

	// first 4 bytes indicate packed length
	uint32_t v;
	if (slurp_read(fp, &v, sizeof(v)) != sizeof(v))
		return 0;
	v = bswapLE32(v);
	v = MIN(v, filelen - startpos) + 4;

	uint32_t bitbuf;
	if (slurp_read(fp, &bitbuf, sizeof(bitbuf)) != sizeof(bitbuf))
		return 0;
	bitbuf = bswapLE32(bitbuf);

	uint8_t *data = dest;

	for (uint32_t j=0; j<len; j++) {
		uint8_t sign = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 1);

		uint8_t hibyte;
		if (mdl_read_bits(&bitbuf, &bitnum, fp, 1)) {
			hibyte = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 3);
		} else {
			hibyte = 8;
			while (!mdl_read_bits(&bitbuf, &bitnum, fp, 1)) hibyte += 0x10;
			hibyte += mdl_read_bits(&bitbuf, &bitnum, fp, 4);
		}

		if (sign)
			hibyte = ~hibyte;

		dlt += hibyte;

		data[j] = dlt;
	}

	slurp_seek(fp, startpos + v, SEEK_SET);

	return v;
}

uint32_t mdl_decompress16(void *dest, uint32_t len, slurp_t *fp)
{
	const int64_t startpos = slurp_tell(fp);
	if (startpos < 0)
		return 0; // wat

	const size_t filelen = slurp_length(fp);

	// first 4 bytes indicate packed length
	uint32_t bitnum = 32;
	uint8_t dlt = 0, lowbyte = 0;

	uint32_t v;
	slurp_read(fp, &v, sizeof(v));
	v = bswapLE32(v);
	v = MIN(v, filelen - startpos) + 4;

	uint32_t bitbuf;
	slurp_read(fp, &bitbuf, sizeof(bitbuf));
	bitbuf = bswapLE32(bitbuf);

	uint8_t *data = dest;

	for (uint32_t j=0; j<len; j++) {
		uint8_t hibyte;
		uint8_t sign;
		lowbyte = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 8);
		sign = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 1);
		if (mdl_read_bits(&bitbuf, &bitnum, fp, 1)) {
			hibyte = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 3);
		} else {
			hibyte = 8;
			while (!mdl_read_bits(&bitbuf, &bitnum, fp, 1)) hibyte += 0x10;
			hibyte += mdl_read_bits(&bitbuf, &bitnum, fp, 4);
		}
		if (sign) hibyte = ~hibyte;
		dlt += hibyte;
#ifdef WORDS_BIGENDIAN
		data[j<<1] = dlt;
		data[(j<<1)+1] = lowbyte;
#else
		data[j<<1] = lowbyte;
		data[(j<<1)+1] = dlt;
#endif
	}

	slurp_seek(fp, startpos + v, SEEK_SET);

	return v;
}
