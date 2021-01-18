#include "PolarSensorConfig.h"
#include "Logger.h"
#include "Utility.h"

// -- Polar Sensor Config
// Bump this version when you are making a breaking config change.
// Simply adding or removing a field is ok and doesn't require a version bump.
const int PolarSensorConfig::CONFIG_VERSION = 1;

const int k_available_acc_sample_rates[] = {25, 50, 100, 200};
const int k_available_ecg_sample_rates[] = {130};
const int k_available_ppg_sample_rates[] = {130};

PolarSensorConfig::PolarSensorConfig(const std::string& fnamebase)
	: HSLConfig(fnamebase)
	, isValid(false)
	, version(CONFIG_VERSION)
	, sampleHistoryDuration(1.f)
	, hrvHistorySize(100)
{
	accSampleRate = k_available_acc_sample_rates[0];
	ecgSampleRate = k_available_ecg_sample_rates[0];
	ppgSampleRate = k_available_ppg_sample_rates[0];
};

int PolarSensorConfig::sanitizeSampleRate(int test_sample_rate, const int* sample_rate_array)
{
	const int* begin = sample_rate_array;
	const int* end = sample_rate_array + ARRAY_SIZE(sample_rate_array);
	const int* it = std::find(begin, end, test_sample_rate);

	return (it != end) ? test_sample_rate : sample_rate_array[0];
}

void PolarSensorConfig::getAvailableCapabilitySampleRates(
	HSLSensorDataStreamFlags flag,
	const int** out_rates,
	int* out_rate_count) const
{
	assert(out_rates);
	assert(out_rate_count);

	switch (flag)
	{
		case HSLStreamFlags_ECGData:
			*out_rates = k_available_ecg_sample_rates;
			*out_rate_count = ARRAY_SIZE(k_available_ecg_sample_rates);
			break;
		case HSLStreamFlags_AccData:
			*out_rates = k_available_acc_sample_rates;
			*out_rate_count = ARRAY_SIZE(k_available_acc_sample_rates);
			break;
		case HSLStreamFlags_PPGData:
			*out_rates = k_available_ppg_sample_rates;
			*out_rate_count = ARRAY_SIZE(k_available_ppg_sample_rates);
			break;
	}
}

bool PolarSensorConfig::setCapabilitySampleRate(HSLSensorDataStreamFlags flag, int sample_rate)
{
	int new_config_value = 0;
	int* config_value_ptr = nullptr;

	switch (flag)
	{
		case HSLStreamFlags_ECGData:
			config_value_ptr = &ecgSampleRate;
			new_config_value = sanitizeSampleRate(sample_rate, k_available_ecg_sample_rates);
			break;
		case HSLStreamFlags_AccData:
			config_value_ptr = &accSampleRate;
			new_config_value = sanitizeSampleRate(sample_rate, k_available_acc_sample_rates);
			break;
		case HSLStreamFlags_PPGData:
			config_value_ptr = &ppgSampleRate;
			new_config_value = sanitizeSampleRate(sample_rate, k_available_ppg_sample_rates);
			break;
	}

	if (config_value_ptr != nullptr && *config_value_ptr != new_config_value)
	{
		*config_value_ptr = new_config_value;

		return true;
	}

	return false;
}

const configuru::Config PolarSensorConfig::writeToJSON()
{
	configuru::Config pt{

		{"is_valid", isValid},
		{"version", PolarSensorConfig::CONFIG_VERSION},
		{"device_name", deviceName},
		{"sample_history_duration", sampleHistoryDuration},
		{"hrv_history_size", hrvHistorySize},
		{"ecg_sample_rate", ecgSampleRate},
		{"ppg_sample_rate", ppgSampleRate},
		{"acc_sample_rate", accSampleRate},
	};

	return pt;
}

void
PolarSensorConfig::readFromJSON(const configuru::Config& pt)
{
	version = pt.get_or<int>("version", 0);

	if (version == PolarSensorConfig::CONFIG_VERSION)
	{
		isValid = pt.get_or<bool>("is_valid", false);

		deviceName = pt.get_or<std::string>("device_name", "unknown");
		sampleHistoryDuration = pt.get_or<float>("sample_history_duration", sampleHistoryDuration);
		hrvHistorySize = pt.get_or<int>("hrv_history_size", hrvHistorySize);

		ecgSampleRate =
			sanitizeSampleRate(
				pt.get_or<int>("ecg_sample_rate", ecgSampleRate),
				k_available_ecg_sample_rates);
		ecgSampleRate =
			sanitizeSampleRate(
				pt.get_or<int>("ppg_sample_rate", ppgSampleRate),
				k_available_ppg_sample_rates);
		accSampleRate =
			sanitizeSampleRate(
				pt.get_or<int>("acc_sample_rate", accSampleRate),
				k_available_acc_sample_rates);
	}
	else
	{
		HSL_LOG_WARNING("PolarSensorConfig") <<
			"Config version " << version << " does not match expected version " <<
			PolarSensorConfig::CONFIG_VERSION << ", Using defaults.";
	}
}