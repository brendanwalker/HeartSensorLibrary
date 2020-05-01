#ifndef HSL_CONFIG_H
#define HSL_CONFIG_H

//-- includes -----
#include "HSLClient_CAPI.h"
#include <string>

#ifdef _MSC_VER
		#pragma warning (push)
		#pragma warning (disable: 4996) // This function or variable may be unsafe
		#pragma warning (disable: 4244) // 'return': conversion from 'const int64_t' to 'float', possible loss of data
		#pragma warning (disable: 4715) // configuru::Config::operator[]': not all control paths return a value
#endif
#include <configuru.hpp>
#ifdef _MSC_VER
		#pragma warning (pop)
#endif


//-- definitions -----
/*
Note that HSLConfig is an abstract class because it has 2 pure virtual functions.
Child classes must add public member variables that store the config data,
as well as implement writeToJSON and readFromJSON that use pt[key]= value and
pt.get_or<type>(), respectively, to convert between member variables and the
property tree. See tests/test_config.cpp for an example.
*/
class HSLConfig {
public:
	HSLConfig(const std::string &fnamebase = std::string("HSLConfig"));
	void save();
	void save(const std::string &path);
	bool load();
	bool load(const std::string &path);
		
	std::string ConfigFileBase;

	virtual const configuru::Config writeToJSON() = 0;  // Implement by each device class' own Config
	virtual void readFromJSON(const configuru::Config &pt) = 0;  // Implement by each device class' own Config
		
	static void writeVector3f(
		configuru::Config &pt,
		const char *vector_name,
		const HSLVector3f &vector);
	static void readVector3f(
		const configuru::Config &pt,
		const char *vector_name,
		HSLVector3f &outVector);

private:
		const std::string getConfigPath();
};

#endif // HSL_CONFIG_H
