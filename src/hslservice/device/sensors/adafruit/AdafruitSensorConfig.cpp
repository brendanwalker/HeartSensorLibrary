#include "AdafruitSensorConfig.h"
#include "Logger.h"
#include "Utility.h"

// -- Adafruit Sensor Config
// Bump this version when you are making a breaking config change.
// Simply adding or removing a field is ok and doesn't require a version bump.
const int AdafruitSensorConfig::CONFIG_VERSION = 1;

const int k_available_gsr_sample_rates[] = { 10 };

AdafruitSensorConfig::AdafruitSensorConfig(const std::string& fnamebase)
	: HSLConfig(fnamebase)
	, isValid(false)
	, version(CONFIG_VERSION)
	, sampleHistoryDuration(1.f)
	, gsrSampleRate(10)
{
};

const configuru::Config AdafruitSensorConfig::writeToJSON()
{
	configuru::Config pt{

		{"is_valid", isValid},
		{"version", AdafruitSensorConfig::CONFIG_VERSION},
		{"device_name", deviceName},
		{"sample_history_duration", sampleHistoryDuration},
		{"gsr_sample_rate", gsrSampleRate}
	};

	return pt;
}

void AdafruitSensorConfig::readFromJSON(const configuru::Config& pt)
{
	version = pt.get_or<int>("version", 0);

	if (version == AdafruitSensorConfig::CONFIG_VERSION)
	{
		isValid = pt.get_or<bool>("is_valid", false);

		deviceName = pt.get_or<std::string>("device_name", "unknown");
		sampleHistoryDuration = pt.get_or<float>("sample_history_duration", sampleHistoryDuration);
		gsrSampleRate = pt.get_or<int>("gsr_sample_rate", gsrSampleRate);
	}
	else
	{
		HSL_LOG_WARNING("AdafruitSensorConfig") <<
			"Config version " << version << " does not match expected version " <<
			AdafruitSensorConfig::CONFIG_VERSION << ", Using defaults.";
	}
}

int AdafruitSensorConfig::sanitizeSampleRate(int test_sample_rate, const int* sample_rate_array)
{
	const int* begin = sample_rate_array;
	const int* end = sample_rate_array + ARRAY_SIZE(sample_rate_array);
	const int* it = std::find(begin, end, test_sample_rate);

	return (it != end) ? test_sample_rate : sample_rate_array[0];
}

void AdafruitSensorConfig::getAvailableCapabilitySampleRates(
	HSLSensorDataStreamFlags flag,
	const int** out_rates,
	int* out_rate_count) const
{
	assert(out_rates);
	assert(out_rate_count);

	switch (flag)
	{
	case HSLStreamFlags_GSRData:
		*out_rates = k_available_gsr_sample_rates;
		*out_rate_count = ARRAY_SIZE(k_available_gsr_sample_rates);
		break;
	}
}

bool AdafruitSensorConfig::setCapabilitySampleRate(HSLSensorDataStreamFlags flag, int sample_rate)
{
	int new_config_value = 0;
	int* config_value_ptr = nullptr;

	switch (flag)
	{
	case HSLStreamFlags_GSRData:
		config_value_ptr = &gsrSampleRate;
		new_config_value = sanitizeSampleRate(sample_rate, k_available_gsr_sample_rates);
		break;
	}

	if (config_value_ptr != nullptr && *config_value_ptr != new_config_value)
	{
		*config_value_ptr = new_config_value;

		return true;
	}

	return false;
}