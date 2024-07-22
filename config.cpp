#include <string>
#include <string.h>
#include <vector>
#include <unordered_map>
#include "config.h"
#include <fstream>

using namespace std;

void Configuration::read(const char* filename) {
	ifstream myfile(filename);
	string line;
	string key, val;
	if (!myfile.is_open()) {
		printf("Unable to open %s\n", filename);
		return;
	}
	while (getline(myfile, line)) {
		int first_space = line.find(" ");
		if (first_space == 0) {
			continue;
		}
		key = line.substr(0, first_space);
		val = line.substr(first_space + 1);
		dictionary[key] = val;
	}
}
void Configuration::freememory() {
	dictionary.clear();
	delete& dictionary;
}