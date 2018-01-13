#pragma once
#include "MemoryMapped/MemoryMapped.h"
#include "../Log.h"
#include "../RVS.h"
#include "../Request.h"

#include <iostream>  
#include <vector>
#include <string>
#include <algorithm>
#include <map>

#include "../Eigen/Dense"
using Eigen::MatrixXd;
using Eigen::VectorXd;

struct GenotypeLikelihood {
	bool missing = false;
	double L00;
	double L01;
	double L11;
};

struct VCFLine {
	std::string chr;
	int loc;
	std::string ref;
	std::string alt;
	std::string filter;
	std::vector<GenotypeLikelihood> likelihood;
	VectorXd P;
	VectorXd expectedGenotype;

	bool valid = true;
	std::string errorMessage;
	inline bool isValid(){ return valid; }
	inline void setInvalid(std::string message) {
		valid = false; 
		errorMessage = message;
	}
	std::string getErrorMessage() { return errorMessage; }

	inline bool operator<(VCFLine& line) {
		if (this->chr == line.chr)
			return this->loc < line.loc;
		else
			return this->chr < line.chr;
	}

	inline std::string toString() {
		std::string t = "\t";
		return chr + t + std::to_string(loc) + t + ref + t + alt;
	}

	inline void print() {
		std::cout << toString() + "\n";
	}
};

inline bool lineCompare(VCFLine lhs, VCFLine rhs) { return lhs < rhs; }

//ParserTools.cpp
std::string extractString(MemoryMapped &charArray, int start, int end);
std::string trim(std::string str);
std::vector<std::string> split(std::string s, char sep);
std::vector<VCFLine> calculateExpectedGenotypes(std::vector<VCFLine> &variants);
VectorXd calcEG(std::vector<GenotypeLikelihood> &likelihood, VectorXd &p);
VectorXd calcEM(std::vector<GenotypeLikelihood> &likelihood);

//VCFParser.cpp
std::vector<VCFLine> parseVCFLines(std::string vcfDir);
std::map<std::string, int> getSampleIDMap(std::string vcfDir);

//SampleParser.cpp
void parseSampleLines(Request req, std::map<std::string, int> &IDmap,
	VectorXd &Y, MatrixXd &Z, VectorXd &G, std::map<int, int> &readGroup);

//BEDParser.cpp
std::vector<std::vector<int>> parseBEDLines(std::string bedDir, std::vector<VCFLine> variants, 
	bool collapseCoding, bool collapseExon);

//VariantFilter.cpp
std::vector<VCFLine> filterVariants(Request req, std::vector<VCFLine> &variants, VectorXd &G);
std::vector<VCFLine> removeDuplicates(std::vector<VCFLine> variants);
std::vector<VCFLine> filterHomozygousVariants(std::vector<VCFLine> &variants);
std::vector<VCFLine> filterMinorAlleleFrequency(std::vector<VCFLine> &variants, double mafCutoff, bool common);

struct File {
	MemoryMapped mmap;
	int pos;
	uint64_t maxPos;
	int lineNumber;

	inline void open(std::string directory) {
		try {
			mmap.open(directory);
			pos = 0;
			lineNumber = 0;
			maxPos = mmap.size();
		}
		catch (...) {
			throwError("file struct", "Cannot open file from provided directory.", directory);
		}
	}

	inline void close() {
		mmap.close();
	}

	inline std::string nextLine() {
		int start = pos;

		while (true) {
			pos++;
			if (pos >= maxPos || mmap[pos] == '\n')
				break;
		}
		pos++;
		lineNumber++;

		return extractString(mmap, start, pos - 1);
	}

	inline int getLineNumber() {
		return lineNumber;
	}

	inline bool hasNext() {
		return pos < maxPos;
	}
};
