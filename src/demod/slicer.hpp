#pragma once

#include <dsp/block.h>
#include <inttypes.h>

namespace dsp {
	class Slicer : public generic_block<Slicer> {
	public:
		Slicer() {};
		Slicer(stream<float> *in);
		~Slicer();

		void init(stream<float> *in);
		void setInput(stream<float> *in);
		int run() override;

		stream<uint8_t> out;
	private:
		uint8_t m_tmp;
		int m_offset;
		stream<float> *m_in;
	};
};
