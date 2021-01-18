#ifndef POLAR_SENSOR_CONFIG_H
#define POLAR_SENSOR_CONFIG_H

#include "HSLConfig.h"
#include <string>

class PolarSensorConfig : public HSLConfig
{
public:
	static const int CONFIG_VERSION;

	PolarSensorConfig(const std::string& fnamebase = "PolarSensorConfig");

	virtual const configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	static int sanitizeSampleRate(int test_sample_rate, const int* sample_rate_array);
	void getAvailableCapabilitySampleRates(HSLSensorDataStreamFlags flag, const int** out_rates, int* out_rate_count) const;
	bool setCapabilitySampleRate(HSLSensorDataStreamFlags flag, int sample_rate);

	bool isValid;
	long version;

	std::string deviceName;
	float sampleHistoryDuration;
	int hrvHistorySize;

	int accSampleRate;
	int ecgSampleRate;
	int ppgSampleRate;
};

#endif // POLAR_SENSOR_CONFIG_H