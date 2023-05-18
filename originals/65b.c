static int16_t decodeSample(ms_adpcm_state &state,
	uint8_t code, const int16_t *coefficient)
{
	int linearSample = (state.sample1 * coefficient[0] +
		state.sample2 * coefficient[1]) >> 8;

	linearSample += ((code & 0x08) ? (code - 0x10) : code) * state.delta;

	linearSample = clamp(linearSample, MIN_INT16, MAX_INT16);

	int delta = (state.delta * adaptationTable[code]) >> 8;
	if (delta < 16)
		delta = 16;

	state.delta = delta;
	state.sample2 = state.sample1;
	state.sample1 = linearSample;

	return static_cast<int16_t>(linearSample);
}
