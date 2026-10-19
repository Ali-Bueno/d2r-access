#include "wall_tones.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace D2RAccess::WallTones {
namespace {
using Complex = std::complex<double>;
// Two band-pass stages at this Q give a band about half an octave wide.
constexpr double BandQ = 1.8;
// Rolls off what the band-pass skirts let through above the bands.
constexpr double WarmthCutoffHz = 1800;
constexpr double NegligibleMagnitude = 1e-9;

// In-place iterative radix-2 FFT without 1/N scaling (the loop is renormalised).
void Fft(std::vector<Complex>& data, bool inverse) {
	const size_t n = data.size();
	for (size_t i = 1, j = 0; i < n; ++i) {
		size_t bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(data[i], data[j]);
	}
	for (size_t length = 2; length <= n; length <<= 1) {
		const double angle = 2 * std::numbers::pi / static_cast<double>(length) * (inverse ? 1 : -1);
		const Complex step(std::cos(angle), std::sin(angle));
		for (size_t start = 0; start < n; start += length) {
			Complex twiddle(1, 0);
			for (size_t k = 0; k < length / 2; ++k) {
				const Complex even = data[start + k], odd = data[start + k + length / 2] * twiddle;
				data[start + k] = even + odd;
				data[start + k + length / 2] = even - odd;
				twiddle *= step;
			}
		}
	}
}
// Magnitude of the chain at `hz`: RBJ band-pass (constant 0 dB peak) twice, then a one-pole low-pass.
double Magnitude(double centreHz, double hz) {
	const double w0 = 2 * std::numbers::pi * centreHz / SampleRate, alpha = std::sin(w0) / (2 * BandQ), a0 = 1 + alpha;
	const double b0 = alpha / a0, a1 = -2 * std::cos(w0) / a0, a2 = (1 - alpha) / a0;
	const double w = 2 * std::numbers::pi * hz / SampleRate;
	const Complex z1 = std::polar(1.0, -w), z2 = std::polar(1.0, -2 * w);
	const double band = std::abs(b0 * (1.0 - z2) / (1.0 + a1 * z1 + a2 * z2));
	const double lowpass = 1 / std::hypot(1.0, hz / WarmthCutoffHz);
	return band * band * lowpass;
}
uint32_t NextRandom(uint32_t& state) { // xorshift32
	state ^= state << 13; state ^= state >> 17; state ^= state << 5;
	return state;
}
}

double BodyRadius(int bodyPattern) { return bodyPattern == 0 ? 0.0 : 1.0; }

double Clearance(const Radar::World& world, Radar::Direction direction, double range) {
	if (!(range > 0)) return 0;
	const auto axis = Radar::Axes[static_cast<size_t>(direction)];
	const double originX = world.player.x + world.subtileFraction.x, originY = world.player.y + world.subtileFraction.y;
	// Unknown is an obstacle here: a cell in no loaded room near the player lies outside the map, which the game blocks (0x27).
	const auto blocked = [&](double x, double y) {
		return world.Cell(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), Radar::Layer::Terrain) != Radar::State::Open;
	};
	// Without the player's own cell there is nothing to measure from.
	if (world.Cell(static_cast<int>(world.player.x), static_cast<int>(world.player.y), Radar::Layer::Terrain) == Radar::State::Unknown) return range;
	const double radius = BodyRadius(world.bodyPattern);
	double best = 0;
	for (int lane = 0; lane < LaneCount; ++lane) {
		const double offset = (lane == 0 ? 0 : lane == 1 ? 1 : -1) * radius;
		const double x = originX - axis.y * offset, y = originY + axis.x * offset; // lane offset along the axis rotated a quarter turn
		double depth = range;
		for (double along = RayStep; along <= range; along += RayStep)
			if (blocked(x + along * axis.x, y + along * axis.y)) { depth = along; break; }
		best = std::max(best, depth);
	}
	return best;
}

std::array<double, Radar::DirectionCount> Clearances(const Radar::World& world, double range) {
	std::array<double, Radar::DirectionCount> result{};
	for (size_t i = 0; i < result.size(); ++i) result[i] = Clearance(world, static_cast<Radar::Direction>(i), range);
	return result;
}

float Gain(double distance, double range) {
	if (!std::isfinite(distance) || !std::isfinite(range) || range <= 0 || distance >= range) return 0;
	const double closeness = 1 - std::max(0.0, distance) / range;
	return static_cast<float>(closeness * closeness);
}

float Glide(float current, float target, double dtMs, double glideMs) {
	if (!(dtMs > 0)) return current;
	if (!(glideMs > 0)) return target;
	return current + (target - current) * static_cast<float>(1 - std::exp(-dtMs / glideMs));
}

double Pan(Radar::Direction direction) { return Radar::ScreenAxes[static_cast<size_t>(direction)].x; }

StereoGains PanGains(double pan) {
	const double angle = (std::clamp(pan, -1.0, 1.0) + 1) * std::numbers::pi / 4;
	return {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
}

float Trim(Radar::Direction direction) {
	return direction == Radar::Direction::Down ? static_cast<float>(std::pow(10, BehindTrimDb / 20)) : 1.0f;
}

std::vector<float> BuildLoop(double centreHz, uint32_t seed) {
	const size_t n = LoopSamples;
	std::vector<Complex> spectrum(n);
	uint32_t state = seed ? seed : 1;
	for (size_t k = 1; k < n / 2; ++k) {
		const double hz = static_cast<double>(k) * SampleRate / static_cast<double>(n);
		const double magnitude = Magnitude(centreHz, hz);
		if (magnitude < NegligibleMagnitude) continue;
		const double phase = 2 * std::numbers::pi * (NextRandom(state) / 4294967296.0);
		spectrum[k] = std::polar(magnitude, phase);
		spectrum[n - k] = std::conj(spectrum[k]);
	}
	Fft(spectrum, true);
	std::vector<float> loop(n);
	double energy = 0;
	for (const auto& value : spectrum) energy += value.real() * value.real();
	const double scale = energy > 0 ? 1 / std::sqrt(energy / static_cast<double>(n)) : 0;
	for (size_t i = 0; i < n; ++i) loop[i] = static_cast<float>(spectrum[i].real() * scale);
	return loop;
}
}
