#pragma once

#include <dsp/block.h>
#include "polyphase.hpp"

/**
 * Symbol timing recovery block.
 * Interpolates the input stream to increase the number of samples per symbol,
 * then applies the Gardner timing recovery algorithm to choose the one the best
 * aligns with the symbol clock.
 */

namespace dsp {
	class GardnerResampler : public generic_block<GardnerResampler> {
	public:
		GardnerResampler() {}
		GardnerResampler(stream<float> *in, float symFreq, float damp, float bw, float maxFreqDelta, float targetSymFreq = 0.125) { init(in, symFreq, damp, bw, maxFreqDelta, targetSymFreq); }
		~GardnerResampler();

		/**
		 * Initialize the timing recovery algorithm with the given parameters
		 *
		 * @param in input stream
		 * @param symFreq symbols per sample in the input stream
		 * @param damp feedback control loop damping
		 * @param bw feedback control loop bandwidth
		 * @param maxFreqDelta maximum allowed deviation from symFreq
		 * @param targetSymFreq maximum number of symbols per sample in the
		 *        internal interpolated stream
		 */
		void init(stream<float> *in, float symFreq, float damp, float bw, float maxFreqDelta, float targetSymFreq = 0.125);
		void setInput(stream<float> *in);
		void setLoopParams(float symFreq, float damp, float bw, float maxFreqDelta, float targetSymFreq = 0.125);

		int run() override;

		stream<float> out;
	private:
		stream<float> *m_in;
		PolyphaseFilter m_flt;
		float m_alpha, m_beta, m_freq, m_centerFreq, m_maxFreqDelta;
		float m_phase;
		int m_state;
		float m_prevSample, m_interSample;
		float m_avgMagnitude, m_avgDC;

		void update_alpha_beta(float damp, float bw);
		int advance_timeslot();
		void retime(float sample);
		float error(float sample);
		void updateEstimate(float error);
	};
};
