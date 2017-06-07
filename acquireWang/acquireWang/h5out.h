#pragma once
#pragma warning(push, 0)
#include "H5Cpp.h" // HDF5
#pragma warning(pop)

#include "acquirer.h"
#include "saver.h"
#include "debug.h"

using namespace H5;

const PredType POINTGREY_H5T = PredType::STD_U8LE;
const PredType KINECT_H5T = PredType::STD_U16LE;
const PredType TIMESTAMP_H5T = PredType::NATIVE_DOUBLE;
const PredType BOOKMARK_H5T = PredType::STD_U64LE;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements an output stream to an HDF5 file, derived from the
 * BaseSaver class. It internally manages the HDF5 file, so it only needs
 * a DCPL and/or DAPL provided if necessary.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class H5Out : public BaseSaver {
private:
	H5File file;
	std::vector<DataSet> datasets; // collection of datasets
	std::vector<DataSet> tsdatasets; // collection of datasets for timestamps
	const std::vector<std::string> dsnames; // collection of dataset names
	const std::vector<PredType> datatypes; // collection of datatypes

	const int ndims = 4;
	std::vector< std::vector<size_t> > frameDims;

public:
	H5Out(std::string& _filename, std::vector<BaseAcquirer>& _acquirers, const size_t _frameChunkSize,
		const std::vector<std::string>& _dsnames, const std::vector<PredType>& _datatypes,
		const FileCreatPropList& _fcpl, const FileAccPropList& _fapl, const std::vector<DSetCreatPropList>& _dcpls) :
			BaseSaver(_filename, _acquirers, _frameChunkSize),
			file(filename, H5F_ACC_TRUNC, _fcpl, _fapl),
			dsnames(_dsnames), datatypes(_datatypes) {
		// Initialize frameDims
		for (int i = 0; i < numStreams; i++) {
			frameDims.push_back(_acquirers[i].getDims());
		}

		// Initialize frame datasets
		for (int i = 0; i < numStreams; i++) {
			// Prepare dataspace
			size_t* dims = new size_t[ndims];
			dims[0] = frameChunkSize;
			dims[1] = frameDims[i][1]; dims[2] = frameDims[i][2]; dims[3] = frameDims[i][3];
			size_t* maxdims = new size_t[ndims];
			maxdims[0] = H5S_UNLIMITED;
			maxdims[1] = frameDims[i][1]; maxdims[2] = frameDims[i][2]; maxdims[3] = frameDims[i][3];
			DataSpace* dataspace = new DataSpace(ndims, dims, maxdims);

			// Create dataset
			datasets.push_back(file.createDataSet(dsnames[i].c_str(), datatypes[i], *dataspace, _dcpls[i]));

			delete dims;
			delete maxdims;
			delete dataspace;
		}
		// Initialize timestamp datasets
		// TODO: timestamps need a dcpl too! Need chunking to be an unlimited dataset
		for (int i = 0; i < numStreams; i++) {
			// Prepare dataspace
			size_t* dims = new size_t[2];
			dims[0] = frameChunkSize;
			dims[1] = 1;
			size_t* maxdims = new size_t[2];
			maxdims[0] = H5S_UNLIMITED;
			maxdims[1] = 1;
			DataSpace* dataspace = new DataSpace(2, dims, maxdims);

			// Create dataset
			tsdatasets.push_back(file.createDataSet((dsnames[i] + "_time").c_str(), TIMESTAMP_H5T, *dataspace));

			delete dims;
			delete maxdims;
			delete dataspace;
		}
	}

	template<typename T> void coutArray(int size, T* arr, const char* name) {
		std::cout << name << ": ";
		for (int i = 0; i < size; i++) {
			std::cout << arr[i] << " ";
		}
		std::cout << std::endl;
	}

	bool writeFrames(size_t numFrames, size_t bufIndex) {
		/* Write frame */
		try {
			// Extend dataset
			size_t* newdims = new size_t[ndims];
			newdims[0] = framesSaved[bufIndex] + numFrames;
			newdims[1] = frameDims[bufIndex][1]; newdims[2] = frameDims[bufIndex][2]; newdims[3] = frameDims[bufIndex][3];
			datasets[bufIndex].extend(newdims);
			DataSpace dataspace = datasets[bufIndex].getSpace();
			// Calculate offset
			size_t* offset = new size_t[ndims]();
			offset[0] = framesSaved[bufIndex];
			// Select hyperslab in dataset
			size_t* selectdims = new size_t[ndims];
			selectdims[0] = numFrames;
			selectdims[1] = frameDims[bufIndex][1]; selectdims[2] = frameDims[bufIndex][2]; selectdims[3] = frameDims[bufIndex][3];
			DataSpace filespace(dataspace);
			filespace.selectHyperslab(H5S_SELECT_SET, selectdims, offset);
			// Define memory space
			DataSpace memspace(ndims, selectdims, NULL);

			// Write
			datasets[bufIndex].write(writeBuffers[bufIndex].data(), datatypes[bufIndex], memspace, filespace);
			framesSaved[bufIndex] += numFrames;

			delete[] newdims;
			delete[] offset;
			delete[] selectdims;
			dataspace.close();
			memspace.close();
			filespace.close();
		}
		catch (...) {
			return false;
		}

		/* Write timestamp */ // TODO: dedup code! Also in constructor
		try {
			// Extend dataset
			size_t* newdims = new size_t[2];
			newdims[0] = framesSaved[bufIndex] + numFrames;
			newdims[1] = 1;
			datasets[bufIndex].extend(newdims);
			DataSpace dataspace = tsdatasets[bufIndex].getSpace();
			// Calculate offset
			size_t* offset = new size_t[2]();
			offset[0] = framesSaved[bufIndex];
			// Select hyperslab in dataset
			size_t* selectdims = new size_t[2];
			selectdims[0] = numFrames;
			selectdims[1] = 1;
			DataSpace filespace(dataspace);
			filespace.selectHyperslab(H5S_SELECT_SET, selectdims, offset);
			// Define memory space
			DataSpace memspace(2, selectdims, NULL);

			// Write
			tsdatasets[bufIndex].write(writeBuffers[bufIndex].data(), datatypes[bufIndex], memspace, filespace);
			framesSaved[bufIndex] += numFrames;

			delete[] newdims;
			delete[] offset;
			delete[] selectdims;
			dataspace.close();
			memspace.close();
			filespace.close();

			return true;
		}
		catch (...) {
			return false;
		}
	}

	~H5Out() {
		for (int i = 0; i < numStreams; i++) datasets[i].close();
		file.close();
	}
};
