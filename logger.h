#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <string>
#include <string.h>
#include <ctime>
#include <vector>
#include <map>
#include <iostream>
#include <regex>
#include <vector>
#include <variant>
#include <time.h>
#include <iomanip>
#ifdef WIN32
#include <Windows.h>
#endif
using namespace std;

namespace Logger {
#ifdef WIN32
	HANDLE hstdout = NULL;
#endif
	bool DEBUG = FALSE;
	std::map<std::string, int> COLORS = {
		{"red", 31},
		{"green", 32},
		{"yellow", 33},
		{"blue", 34},
		{"light_yellow", 93},
		{"white", 97},
		{"clear", -1},
		{"cyan", 36},
	};
	std::map<int, int> COLORS_32 = {
		{31, 12},
		{-1, -1},
		{32, 10},
		{97, 15},
		{33, 14},
		{34, 11},
		{36, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY}
	};
	std::map<std::string, int> LEVELS = {
		{"INFO", COLORS["white"]},
		{"SUCCESS", COLORS["green"]},
		{"ERROR", COLORS["red"]},
		{"WARNING", COLORS["yellow"]},
		{"DEBUG", COLORS["blue"]},
	};
	std::string defaultPrefix = std::string("<cyan>[VCMPAPISocket]</cyan> ");
	std::string FORMAT = defaultPrefix + std::string("<white>[%datetime%]</white> <level>[%level%]: %message%\n");
	std::regex REGEXP = std::regex("<(/?)[a-z]+>|%[a-zA-Z0-9-_]+%");
	std::string format_number(unsigned long n, int length = 0) {
		std::string str = std::to_string(n);
		for (int i = int(str.length()); i < length; i++) {
			str = "0" + str;
		}
		return str;
	}
	std::string getFormattedTime() {
		std::time_t t = std::time(nullptr);
		std::tm localTime;
		if (localtime_s(&localTime, &t) == 0) {
			char buffer[20];
			sprintf_s(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
				localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
				localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
			return std::string(buffer);
		}
		return "1970-01-01 00:00:00";
	}

	std::vector<std::string> parseLogMessage(const std::string& level, const std::string& message) {
		std::string format = std::string(FORMAT.c_str());
		std::smatch matches;
		std::vector<std::string> parsedResults;
		while (regex_search(format, matches, REGEXP, regex_constants::match_any))
		{
			std::string result = matches.str();
			parsedResults.push_back("0" + format.substr(0, matches.position())); // std::variant<std::string, int>
			if (result.substr(0, 1) == "%" && result.substr(result.length() - 1) == "%") {
				result = result.substr(1, result.length() - 2);
				if (result == "datetime") {
					parsedResults.push_back("0" + getFormattedTime());
				}
				else if (result == "level") {
					parsedResults.push_back("0" + std::string(level.c_str()));

				}
				else if (result == "message") {
					parsedResults.push_back("0" + std::string(message.c_str()));
				}
			}
			else {
				std::string color = result.substr(1, result.length() - 2);
				if (color.substr(0, 1) == "/") {
					parsedResults.push_back("1-1");
				} else if (COLORS.find(color) != COLORS.end()) {
					parsedResults.push_back("1" + std::to_string(COLORS[color]));
				}
				else if (LEVELS.find(level) != LEVELS.end()) {
					parsedResults.push_back("1" + std::to_string(LEVELS[level]));
				}
			}
			format = format.substr(matches.length() + matches.position());
		}
		parsedResults.push_back("0" + format);
		return parsedResults;
	}
	void rawLogger(const std::string& level, const std::string& message) {
		if (level == "DEBUG" && !DEBUG) {
			return;
		}
		std::vector<std::string> parsed = parseLogMessage(level, message);
		std::vector<int> lastColors = {
			COLORS["clear"]
		};
		for (std::string& str : parsed) {
			if (str.substr(0, 1) == "1") {
				int number = std::stoi(str.substr(1));
				if (number == -1) {
					lastColors.pop_back();
				}
				else lastColors.push_back(number);
				continue;
			}
			const std::string text = str.substr(1);
			const int color = lastColors.at(lastColors.size() - 1);
#ifdef WIN32
			if (hstdout)
			{
				//Credits: https://bitbucket.org/stormeus/0.4-squirrel/src/master/ConsoleUtils.cpp
				CONSOLE_SCREEN_BUFFER_INFO csbBefore;
				GetConsoleScreenBufferInfo(hstdout, &csbBefore);
				SetConsoleTextAttribute(hstdout, COLORS_32[color]);
				fputs(text.c_str(), stdout);
				SetConsoleTextAttribute(hstdout, csbBefore.wAttributes);
			}
			else
				printf("%s", text.c_str());
#else

			printf("%c[%s%sm%s%c[0m", 27, (COLORS_32[color] & 8) == 8 ? "1;" : "", color, text.c_str(), 27);

#endif
		}
	}
	void rawLogger(const std::string& level, const char* message) {
		return rawLogger(level, std::string(message));
	}
	void rawLogger(const char* level, const char* message) {
		return rawLogger(std::string(level), std::string(message));
	}
	void info(const char* message) {
		rawLogger("INFO", message);
	}
	void info(std::string message) {
		rawLogger("INFO", message);
	}
	void error(const char* message) {
		rawLogger("ERROR", message);
	}
	void error(std::string message) {
		rawLogger("ERROR", message);
	}
	void debug(const char* message) {
		rawLogger("DEBUG", message);
	}
	void debug(std::string message) {
		rawLogger("DEBUG", message);
	}
	void success(const char* message) {
		rawLogger("SUCCESS", message);
	}
	void success(std::string message) {
		rawLogger("SUCCESS", message);
	}
	void warning(const char* message) {
		rawLogger("WARNING", message);
	}
	void warning(std::string message) {
		rawLogger("WARNING", message);
	}
}