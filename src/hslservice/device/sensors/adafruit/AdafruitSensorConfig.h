#ifndef ADAFRUIT_SENSOR_CONFIG_H
#define ADAFRUIT_SENSOR_CONFIG_H

#include "HSLConfig.h"
#include <string>

class AdafruitSensorConfig : public HSLConfig
{
public:
	static const int CONFIG_VERSION;

	AdafruitSensorConfig(const std::string& fnamebase = "AdafruitSensorConfig");

	virtual const configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	static int sanitizeSampleRate(int test_sample_rate, const int* sample_rate_array);
	void getAvailableCapabilitySampleRates(HSLSensorDataStreamFlags flag, const int** out_rates, int* out_rate_count) const;
	bool setCapabilitySampleRate(HSLSensorDataStreamFlags flag, int sample_rate);

	bool isValid;
	long version;

	std::string deviceName;
	float sampleHistoryDuration;

	int gsrSampleRate;
};

#endif // ADAFRUIT_SENSOR_CONFIG_H
