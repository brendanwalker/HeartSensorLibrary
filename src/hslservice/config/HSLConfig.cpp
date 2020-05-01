#include "HSLConfig.h"
#include "DeviceInterface.h"
#include "Logger.h"
#include "Utility.h"
#include <iostream>

// Suppress unhelpful configuru warnings
#ifdef _MSC_VER
		#pragma warning (push)
		#pragma warning (disable: 4996) // This function or variable may be unsafe
		#pragma warning (disable: 4244) // 'return': conversion from 'const int64_t' to 'float', possible loss of data
		#pragma warning (disable: 4715) // configuru::Config::operator[]': not all control paths return a value
#endif
#define CONFIGURU_IMPLEMENTATION 1
#include <configuru.hpp>
#ifdef _MSC_VER
		#pragma warning (pop)
#endif

HSLConfig::HSLConfig(const std::string &fnamebase)
: ConfigFileBase(fnamebase)
{
}

const std::string
HSLConfig::getConfigPath()
{
	std::string home_dir= Utility::get_home_directory();  
	std::string config_path = home_dir + "/HSLSERVICE";
	
	if (!Utility::create_directory(config_path))
	{
		HSL_LOG_ERROR("HSLConfig::getConfigPath") << "Failed to create config directory: " << config_path;
	}

	std::string config_filepath = config_path + "/" + ConfigFileBase + ".json";

	return config_filepath;
}

void
HSLConfig::save()
{
	save(getConfigPath());
}

void 
HSLConfig::save(const std::string &path)
{
	configuru::dump_file(path, writeToJSON(), configuru::JSON);
}

bool
HSLConfig::load()
{
	return load(getConfigPath());
}

bool 
HSLConfig::load(const std::string &path)
{
	bool bLoadedOk = false;

	if (Utility::file_exists( path ) )
	{
			configuru::Config m_config = configuru::parse_file(path, configuru::JSON);
			readFromJSON(m_config);
			bLoadedOk = true;
	}

	return bLoadedOk;
}

void HSLConfig::writeVector3f(
		configuru::Config &pt,
		const char *vector_name,
		const HSLVector3f &v)
{
		pt[vector_name]= configuru::Config::array({v.x, v.y, v.z});
}

void HSLConfig::readVector3f(
		const configuru::Config &pt,
		const char *vector_name,
		HSLVector3f &outVector)
{
		if (pt[vector_name].is_array())
		{
				outVector.x= pt[vector_name][0].as_float();
				outVector.y= pt[vector_name][1].as_float();
				outVector.z= pt[vector_name][2].as_float();
		}
}