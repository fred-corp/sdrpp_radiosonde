#include <string.h>
#include "decoder.hpp"
#include "decode/gps/ecef.h"
#include "decode/gps/time.h"
#include "decode/xdata.hpp"
#include "rs41.h"
#include "utils.h"

/* Pseudorandom sequence, obtained by autocorrelating the extra data found at the end of frames
 * from a radiosonde with ozone sensor */
static const uint8_t _prn[RS41_PRN_PERIOD] = {
	0x96, 0x83, 0x3e, 0x51, 0xb1, 0x49, 0x08, 0x98,
	0x32, 0x05, 0x59, 0x0e, 0xf9, 0x44, 0xc6, 0x26,
	0x21, 0x60, 0xc2, 0xea, 0x79, 0x5d, 0x6d, 0xa1,
	0x54, 0x69, 0x47, 0x0c, 0xdc, 0xe8, 0x5c, 0xf1,
	0xf7, 0x76, 0x82, 0x7f, 0x07, 0x99, 0xa2, 0x2c,
	0x93, 0x7c, 0x30, 0x63, 0xf5, 0x10, 0x2e, 0x61,
	0xd0, 0xbc, 0xb4, 0xb6, 0x06, 0xaa, 0xf4, 0x23,
	0x78, 0x6e, 0x3b, 0xae, 0xbf, 0x7b, 0x4c, 0xc1
};

/* Sane default calibration data, taken from a live radiosonde {{{ */
static const uint8_t _defaultCalibData[sizeof(RS41Calibration)] = {
	0xec, 0x5c, 0x80, 0x57, 0x03, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x33, 0x32,
	0x32, 0x30, 0x36, 0x35, 0x30, 0xf7, 0x4e, 0x00, 0x00, 0x58, 0x02, 0x12, 0x05, 0xb4, 0x3c, 0xa4,
	0x06, 0x14, 0x87, 0x32, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x03, 0x1e, 0x23,
	0xe8, 0x03, 0x01, 0x04, 0x00, 0x07, 0x00, 0xbf, 0x02, 0x91, 0xb3, 0x00, 0x06, 0x00, 0x80, 0x3b,
	0x44, 0x00, 0x80, 0x89, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x2a, 0xe9, 0x73,
	0xc3, 0x5f, 0x28, 0x40, 0x3e, 0xbb, 0x92, 0x09, 0x37, 0xdd, 0xd6, 0xa0, 0x3f, 0xc5, 0x52, 0xd6,
	0xbd, 0x54, 0xe4, 0xb5, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x99, 0x30, 0x42, 0x6f, 0xd9, 0xa1, 0x40, 0xe1, 0x79, 0x29,
	0xbb, 0x52, 0x98, 0x0f, 0xc0, 0x5f, 0xc4, 0x1e, 0x41, 0xc3, 0x9f, 0x67, 0xc0, 0xe9, 0x6b, 0x59,
	0x42, 0x33, 0x9a, 0xba, 0xc2, 0x8e, 0xd2, 0x4e, 0x42, 0xc3, 0x7b, 0x1b, 0x42, 0xf8, 0x6f, 0x51,
	0x43, 0xf0, 0x37, 0xbd, 0xc3, 0xa8, 0xc5, 0x12, 0x41, 0x93, 0x3d, 0x9c, 0x41, 0xeb, 0x41, 0x16,
	0x43, 0x14, 0xe8, 0x16, 0xc3, 0x45, 0x28, 0x8c, 0xc3, 0x09, 0x4b, 0x36, 0x43, 0x4f, 0xf6, 0x4a,
	0x45, 0x6f, 0x3a, 0x7f, 0x45, 0x86, 0x91, 0x69, 0xc3, 0xf1, 0xaf, 0xac, 0x43, 0x8d, 0x37, 0x48,
	0x43, 0x7b, 0x1f, 0xc2, 0xc3, 0x87, 0x1a, 0x62, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x54, 0xd7, 0x61,
	0x43, 0xf4, 0x0c, 0x69, 0xc3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x20, 0xba, 0xc2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0xe9, 0x73, 0xc3, 0x5f, 0x28, 0x40, 0x3e, 0xbb, 0x92, 0x09,
	0x37, 0x80, 0xda, 0xa5, 0x3f, 0xa6, 0x1d, 0xc0, 0xbc, 0x82, 0x9e, 0xb3, 0x3b, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xc6, 0x00, 0x41, 0x69, 0x30, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xcd, 0xcc, 0xcc, 0x3d, 0xbd, 0xff, 0x4b, 0xbf, 0x47, 0x49, 0x9e, 0xbd, 0x66, 0x36, 0xb1, 0x33,
	0x5b, 0x39, 0x8b, 0xb7, 0x1b, 0x8a, 0xf1, 0x39, 0x00, 0xe0, 0xaa, 0x44, 0xf0, 0x85, 0x49, 0x3c,
	0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x90, 0x40, 0x00, 0x00, 0xa0, 0x3f, 0x00, 0x00, 0x00, 0x00,
	0x33, 0x33, 0x33, 0x3f, 0x68, 0x91, 0x2d, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xe6, 0x96, 0x7e, 0x3f, 0x97, 0x82, 0x9b, 0xb8, 0xaa, 0x39, 0x23, 0x30,
	0xe4, 0x16, 0xcd, 0x29, 0xb5, 0x26, 0x5a, 0xa2, 0xfd, 0xeb, 0x02, 0x1a, 0xec, 0x51, 0x38, 0x3e,
	0x33, 0x33, 0x33, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xf6, 0x7f, 0x74, 0x40, 0x3b, 0x36, 0x82, 0xbf, 0xe5, 0x2f, 0x98, 0x3d, 0x00, 0x01, 0x00, 0x01,
	0xac, 0xac, 0xba, 0xbe, 0x0c, 0xe6, 0xab, 0x3e, 0x00, 0x00, 0x00, 0x40, 0x08, 0x39, 0xad, 0x41,
	0x89, 0x04, 0xaf, 0x41, 0x00, 0x00, 0x40, 0x40, 0xff, 0xff, 0xff, 0xc6, 0xff, 0xff, 0xff, 0xc6,
	0xff, 0xff, 0xff, 0xc6, 0xff, 0xff, 0xff, 0xc6, 0x52, 0x53, 0x34, 0x31, 0x2d, 0x53, 0x47, 0x00,
	0x00, 0x00, 0x52, 0x53, 0x4d, 0x34, 0x31, 0x32, 0x00, 0x00, 0x00, 0x00, 0x53, 0x33, 0x31, 0x31,
	0x30, 0x33, 0x31, 0x34, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00,
	0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x81, 0x23, 0x00,
	0x00, 0x1a, 0x02, 0x00, 0x02, 0x7b, 0xe5, 0xb5, 0x3f, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd5, 0xca, 0xa4, 0x3d, 0x5d, 0xa3, 0x65, 0x39, 0x7f, 0x87,
	0x22, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0xfe, 0xb7, 0xbc, 0xc8, 0x96,
	0xe5, 0x3e, 0x31, 0x99, 0x1a, 0xbf, 0x12, 0xda, 0xda, 0x3e, 0xb6, 0x84, 0x68, 0xc1, 0x67, 0x55,
	0x57, 0x42, 0xd6, 0xc5, 0xaa, 0xc1, 0x84, 0x9e, 0xc7, 0xc1, 0xfd, 0xbc, 0x3e, 0x41, 0x1e, 0x16,
	0x4c, 0xc2, 0x7c, 0xb8, 0x8b, 0x41, 0xbb, 0x32, 0xf4, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x14, 0x00,
	0xc8, 0x00, 0x46, 0x00, 0x3c, 0x00, 0x05, 0x00, 0x3c, 0x00, 0x18, 0x01, 0x9e, 0x62, 0xd5, 0xb8,
	0x6c, 0x9c, 0x07, 0xb1, 0x00, 0x3c, 0x88, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xf3, 0x6a, 0xc0, 0xf1, 0x5b, 0x02, 0x07, 0x00, 0x00, 0x05, 0x6d, 0x01, 0x1b, 0x94, 0x00, 0x00
};
/* }}} */


RS41Decoder::RS41Decoder()
{
	m_rs = correct_reed_solomon_create(RS41_REEDSOLOMON_POLY,
	                                  RS41_REEDSOLOMON_FIRST_ROOT,
	                                  RS41_REEDSOLOMON_ROOT_SKIP,
	                                  RS41_REEDSOLOMON_T);
}

RS41Decoder::RS41Decoder(dsp::stream<uint8_t> *in, void (*handler)(SondeData *data, void *ctx), void *ctx)
{
	init(in, handler, ctx);
}

RS41Decoder::~RS41Decoder()
{
	if (!generic_block<RS41Decoder>::_block_init) return;
	generic_block<RS41Decoder>::stop();
	generic_block<RS41Decoder>::unregisterInput(m_in);
	generic_block<RS41Decoder>::_block_init = false;
}

void
RS41Decoder::init(dsp::stream<uint8_t> *in, void (*handler)(SondeData *data, void *ctx), void *ctx)
{
	m_in = in;
	m_ctx = ctx;
	m_handler = handler;
	m_calibrated = false;
	m_rs = correct_reed_solomon_create(RS41_REEDSOLOMON_POLY,
	                                  RS41_REEDSOLOMON_FIRST_ROOT,
	                                  RS41_REEDSOLOMON_ROOT_SKIP,
	                                  RS41_REEDSOLOMON_T);
	memcpy(&m_calibData, _defaultCalibData, sizeof(_defaultCalibData));
	memset(m_calibDataBitmap, 0xFF, sizeof(m_calibDataBitmap));
	m_calibDataBitmap[sizeof(m_calibDataBitmap)-1] &= ~((1 << (7 - (RS41_CALIB_FRAGCOUNT-1)%8)) - 1);


	generic_block<RS41Decoder>::registerInput(m_in);
	generic_block<RS41Decoder>::_block_init = true;
}

void
RS41Decoder::setInput(dsp::stream<uint8_t>* in)
{
	generic_block<RS41Decoder>::tempStop();
	generic_block<RS41Decoder>::unregisterInput(m_in);

	m_in = in;
	m_sondeData.init();
	memset(m_calibDataBitmap, 0xFF, sizeof(m_calibDataBitmap));
	m_calibDataBitmap[sizeof(m_calibDataBitmap)-1] &= ~((1 << (7 - (RS41_CALIB_FRAGCOUNT-1)%8)) - 1);
	m_calibrated = false;

	generic_block<RS41Decoder>::registerInput(m_in);
	generic_block<RS41Decoder>::tempStart();
}

int
RS41Decoder::run()
{
	RS41Frame *frame;
	RS41Subframe *subframe;
	float oldPressure;
	int offset, outCount, numFrames, bytesLeft;

	assert(generic_block<RS41Decoder>::_block_init);
	if ((numFrames = m_in->read()) < 0) return -1;

	outCount = 0;
	numFrames /= sizeof(*frame);

	/* For each frame that was received */
	for (int i=0; i<numFrames; i++) {
		frame = (RS41Frame*)(m_in->readBuf + i*sizeof(*frame));

		/* Descramble and error correct */
		descramble(frame);
		if (m_rs) rsCorrect(frame);

		bytesLeft = RS41_DATA_LEN + (frame->extended_flag == RS41_FLAG_EXTENDED ? RS41_XDATA_LEN : 0);
		offset = 0;
		while (offset < bytesLeft) {
			subframe = (RS41Subframe*)&frame->data[offset];
			offset += subframe->len + 4;

			/* Verify that end of the subframe is still within bounds */
			if (offset > bytesLeft) break;

			/* Check subframe checksum */
			if (!crcCheck(subframe)) continue;

			/* Update the generic info struct with the data inside the subframe */
			updateSondeData(&m_sondeData, subframe);
		}

		m_handler(&m_sondeData, m_ctx);
		outCount++;
	}

	m_in->flush();
	return outCount;
}

/* Private methods {{{ */
void
RS41Decoder::descramble(RS41Frame *frame)
{
	int i, j;
	uint8_t tmp;
	uint8_t *rawFrame = (uint8_t*) frame;

	/* Reorder bits in the frame and XOR with PRN sequence */
	for (i=0; i<sizeof(*frame); i++) {
		tmp = 0;
		for (int j=0; j<8; j++) {
			tmp |= ((rawFrame[i] >> (7-j)) & 0x1) << j;
		}
		rawFrame[i] = 0xFF ^ tmp ^ _prn[i % RS41_PRN_PERIOD];
	}
}

bool
RS41Decoder::rsCorrect(RS41Frame *frame)
{
	bool valid;
	int i, block, chunk_len;
	uint8_t rsBlock[RS41_REEDSOLOMON_N];

	if (frame->extended_flag != RS41_FLAG_EXTENDED) {
		chunk_len = (RS41_DATA_LEN + 1) / RS41_REEDSOLOMON_INTERLEAVING;
		memset(rsBlock, 0, RS41_REEDSOLOMON_N);
	} else {
		chunk_len = RS41_REEDSOLOMON_K;
	}

	valid = true;
	for (block=0; block<RS41_REEDSOLOMON_INTERLEAVING; block++) {
		/* Deinterleave */
		for (i=0; i<chunk_len; i++) {
			rsBlock[i] = frame->data[RS41_REEDSOLOMON_INTERLEAVING*i + block - 1];
		}
		for (i=0; i<RS41_REEDSOLOMON_T; i++) {
			rsBlock[RS41_REEDSOLOMON_K+i] = frame->rs_checksum[i + RS41_REEDSOLOMON_T*block];
		}

		/* Error correct */
		if (correct_reed_solomon_decode(m_rs, rsBlock, RS41_REEDSOLOMON_N, rsBlock) < 0) valid = false;

		/* Reinterleave */
		for (i=0; i<chunk_len; i++) {
			frame->data[RS41_REEDSOLOMON_INTERLEAVING*i + block - 1] = rsBlock[i];
		}
		for (i=0; i<RS41_REEDSOLOMON_T; i++) {
			frame->rs_checksum[i + RS41_REEDSOLOMON_T*block] = rsBlock[RS41_REEDSOLOMON_K+i];
		}
	}

	return valid;
}

bool
RS41Decoder::crcCheck(RS41Subframe *subframe)
{
	uint16_t checksum = crc16(CCITT_FALSE_POLY, CCITT_FALSE_INIT, subframe->data, subframe->len);
	uint16_t expected = subframe->data[subframe->len] | subframe->data[subframe->len+1] << 8;

	return checksum == expected;
}

void
RS41Decoder::updateSondeData(SondeData *info, RS41Subframe *subframe)
{
	RS41Subframe_Status *status;
	RS41Subframe_PTU *ptu;
	RS41Subframe_GPSInfo *gpsinfo;
	RS41Subframe_GPSPos *gpspos;
	RS41Subframe_XDATA *xdata;

	int i;
	float x, y, z, dx, dy, dz;
	bool hasPressureSensor = false;

	switch (subframe->type) {
		case RS41_SFTYPE_INFO:
			status = (RS41Subframe_Status*)subframe;
			updateCalibData(status);

			info->calibrated = m_calibrated;
			info->serial = status->serial;
			info->serial[RS41_SERIAL_LEN] = 0;
			info->burstkill = m_calibData.burstkill_timer == 0xFFFF ? -1 : m_calibData.burstkill_timer;
			info->seq = status->frame_seq;
			break;
		case RS41_SFTYPE_PTU:
			ptu = (RS41Subframe_PTU*)subframe;

			info->temp = temp(ptu);
			info->rh = rh(ptu);
			if (pressure(ptu) > 0) {
				hasPressureSensor = true;
				info->pressure = pressure(ptu);  /* Pressure sensor is optional */
			}
			info->dewpt = dewpt(info->temp, info->rh);
			break;
		case RS41_SFTYPE_GPSPOS:
			gpspos = (RS41Subframe_GPSPos*)subframe;
			x = gpspos->x / 100.0;
			y = gpspos->y / 100.0;
			z = gpspos->z / 100.0;
			dx = gpspos->dx / 100.0;
			dy = gpspos->dy / 100.0;
			dz = gpspos->dz / 100.0;

			ecef_to_lla(&info->lat, &info->lon, &info->alt, x, y, z);
			ecef_to_spd_hdg(&info->spd, &info->hdg, &info->climb, info->lat, info->lon, dx, dy, dz);

			if (!hasPressureSensor) info->pressure = altitude_to_pressure(info->alt);

			break;
		case RS41_SFTYPE_GPSINFO:
			gpsinfo = (RS41Subframe_GPSInfo*)subframe;
			info->time = gps_time_to_utc(gpsinfo->week, gpsinfo->ms);
			break;
		case RS41_SFTYPE_XDATA:
			xdata = (RS41Subframe_XDATA*)subframe;
			info->auxData = decodeXDATA(info, xdata->ascii_data, xdata->len);
			break;
		case RS41_SFTYPE_GPSRAW:
		case RS41_SFTYPE_EMPTY:
		default:
			break;
	}
}

void
RS41Decoder::updateCalibData(RS41Subframe_Status* status)
{
	size_t frag_offset;
	int num_segments;
	size_t i;

	/* Copy the fragment and update the bitmap of the fragments left */
	frag_offset = status->frag_seq * RS41_CALIB_FRAGSIZE;
	memcpy((uint8_t*)&m_calibData + frag_offset, status->frag_data, RS41_CALIB_FRAGSIZE);
	m_calibDataBitmap[status->frag_seq/8] &= ~(1 << (7-status->frag_seq%8));

	/* Check if we have all the sub-segments populated */
	for (i=0; i<sizeof(m_calibDataBitmap); i++) {
		if (m_calibDataBitmap[i]) return;
	}
	m_calibrated = true;
}


float
RS41Decoder::temp(RS41Subframe_PTU *ptu)
{
	const float adc_main = (uint32_t)ptu->temp_main[0]
	                      | (uint32_t)ptu->temp_main[1] << 8
	                      | (uint32_t)ptu->temp_main[2] << 16;
	const float adc_ref1 = (uint32_t)ptu->temp_ref1[0]
	                      | (uint32_t)ptu->temp_ref1[1] << 8
	                      | (uint32_t)ptu->temp_ref1[2] << 16;
	const float adc_ref2 = (uint32_t)ptu->temp_ref2[0]
	                      | (uint32_t)ptu->temp_ref2[1] << 8
	                      | (uint32_t)ptu->temp_ref2[2] << 16;

	float adc_raw, r_raw, r_t, t_uncal, t_cal;
	int i;

	/* If no reference or no calibration data, retern */
	if (adc_ref2 - adc_ref1 == 0) return NAN;

	/* Compute ADC gain and bias */
	adc_raw = (adc_main - adc_ref1) / (adc_ref2 - adc_ref1);

	/* Compute resistance */
	r_raw = m_calibData.t_ref[0] + (m_calibData.t_ref[1] - m_calibData.t_ref[0])*adc_raw;
	r_t = r_raw * m_calibData.t_calib_coeff[0];

	/* Compute temperature based on corrected resistance */
	t_uncal = m_calibData.t_temp_poly[0]
	     + m_calibData.t_temp_poly[1]*r_t
	     + m_calibData.t_temp_poly[2]*r_t*r_t;

	t_cal = 0;
	for (i=6; i>0; i--) {
		t_cal *= t_uncal;
		t_cal += m_calibData.t_calib_coeff[i];
	}
	t_cal += t_uncal;

	return t_cal;
}

float
RS41Decoder::rh(RS41Subframe_PTU *ptu)
{
	float adc_main = (uint32_t)ptu->humidity_main[0]
	                       | (uint32_t)ptu->humidity_main[1] << 8
	                       | (uint32_t)ptu->humidity_main[2] << 16;
	float adc_ref1 = (uint32_t)ptu->humidity_ref1[0]
	                       | (uint32_t)ptu->humidity_ref1[1] << 8
	                       | (uint32_t)ptu->humidity_ref1[2] << 16;
	float adc_ref2 = (uint32_t)ptu->humidity_ref2[0]
	                       | (uint32_t)ptu->humidity_ref2[1] << 8
	                       | (uint32_t)ptu->humidity_ref2[2] << 16;

	int i, j;
	float f1, f2;
	float adc_raw, c_raw, c_cal, rh_uncal, rh_cal, rh_temp_uncal, rh_temp_cal, t_temp;

	if (adc_ref2 - adc_ref1 == 0) return NAN;

	/* Get RH sensor temperature and actual temperature */
	rh_temp_uncal = rh_temp(ptu);
	t_temp = temp(ptu);

	/* Compute RH calibrated temperature */
	rh_temp_cal = 0;
	for (i=6; i>0; i--) {
		rh_temp_cal *= rh_temp_uncal;
		rh_temp_cal += m_calibData.th_calib_coeff[i];
	}
	rh_temp_cal += rh_temp_uncal;

	/* Get raw capacitance of the RH sensor */
	adc_raw = (adc_main - adc_ref1) / (adc_ref2 - adc_ref1);
	c_raw = m_calibData.rh_ref[0] + adc_raw * (m_calibData.rh_ref[1] - m_calibData.rh_ref[0]);
	c_cal = (c_raw / m_calibData.rh_cap_calib[0] - 1) * m_calibData.rh_cap_calib[1];

	/* Derive raw RH% from capacitance and temperature response */
	rh_uncal = 0;
	rh_temp_cal = (rh_temp_cal - 20) / 180;
	f1 = 1;
	for (i=0; i<7; i++) {
		f2 = 1;
		for (j=0; j<6; j++) {
			rh_uncal += f1 * f2 * m_calibData.rh_calib_coeff[i][j];
			f2 *= rh_temp_cal;
		}
		f1 *= c_cal;
	}

	/* Account for different temperature between air and RH sensor */
	rh_cal = rh_uncal * wv_sat_pressure(rh_temp_uncal) / wv_sat_pressure(t_temp);
	return fmax(0.0, fmin(100.0, rh_cal));
}

float
RS41Decoder::rh_temp(RS41Subframe_PTU *ptu)
{
	const float adc_main = (uint32_t)ptu->temp_humidity_main[0]
	                      | (uint32_t)ptu->temp_humidity_main[1] << 8
	                      | (uint32_t)ptu->temp_humidity_main[2] << 16;
	const float adc_ref1 = (uint32_t)ptu->temp_humidity_ref1[0]
	                      | (uint32_t)ptu->temp_humidity_ref1[1] << 8
	                      | (uint32_t)ptu->temp_humidity_ref1[2] << 16;
	const float adc_ref2 = (uint32_t)ptu->temp_humidity_ref2[0]
	                      | (uint32_t)ptu->temp_humidity_ref2[1] << 8
	                      | (uint32_t)ptu->temp_humidity_ref2[2] << 16;

	float adc_raw, r_raw, r_t, t_uncal;

	/* If no reference or no calibration data, retern */
	if (adc_ref2 - adc_ref1 == 0) return NAN;
	if (!m_calibData.t_ref[0] || !m_calibData.t_ref[1]) return NAN;

	/* Compute ADC gain and bias */
	adc_raw = (adc_main - adc_ref1) / (adc_ref2 - adc_ref1);

	/* Compute resistance */
	r_raw = m_calibData.t_ref[0] + adc_raw * (m_calibData.t_ref[1] - m_calibData.t_ref[0]);
	r_t = r_raw * m_calibData.th_calib_coeff[0];

	/* Compute temperature based on corrected resistance */
	t_uncal = m_calibData.th_temp_poly[0]
	     + m_calibData.th_temp_poly[1]*r_t
	     + m_calibData.th_temp_poly[2]*r_t*r_t;

	return t_uncal;
}

float
RS41Decoder::pressure(RS41Subframe_PTU *ptu)
{
	/* TODO */
	return 0;
}
/* }}} */
