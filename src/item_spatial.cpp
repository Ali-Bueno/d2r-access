#include "item_spatial.h"
#include <windows.h>
#include <x3daudio.h>

namespace D2RAccess::ItemSpatial {
std::array<float, 2> StereoMatrix(const GroundItems::Voice& voice) {
	struct Spatial {
		X3DAUDIO_HANDLE handle{};
		bool ready = SUCCEEDED(X3DAudioInitialize(SPEAKER_STEREO, X3DAUDIO_SPEED_OF_SOUND, handle));
	};
	static const Spatial spatial;
	std::array<float, 2> matrix{};
	if (!spatial.ready) return matrix;
	X3DAUDIO_LISTENER listener{};
	listener.OrientFront = {0, 0, 1}; listener.OrientTop = {0, 1, 0};
	// A constant distance curve leaves attenuation to GroundItems::Spatialize.
	X3DAUDIO_DISTANCE_CURVE_POINT points[] = {{0, 1}, {1, 1}};
	X3DAUDIO_DISTANCE_CURVE curve{points, 2};
	X3DAUDIO_EMITTER emitter{};
	emitter.OrientFront = {0, 0, 1}; emitter.OrientTop = {0, 1, 0};
	emitter.Position = {voice.right, 0, voice.front}; emitter.ChannelCount = 1;
	emitter.CurveDistanceScaler = 1; emitter.pVolumeCurve = &curve;
	X3DAUDIO_DSP_SETTINGS dsp{};
	dsp.SrcChannelCount = 1; dsp.DstChannelCount = 2; dsp.pMatrixCoefficients = matrix.data();
	X3DAudioCalculate(spatial.handle, &listener, &emitter, X3DAUDIO_CALCULATE_MATRIX, &dsp);
	return matrix;
}
}
