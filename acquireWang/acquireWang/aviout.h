#pragma once
#pragma warning(push, 0)
#include "AVIRecorder.h"
#pragma warning(pop)

#include "acquirer.h"
#include "saver.h"
#include "debug.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements an output stream to an AVI file, derived from the
 * BaseSaver class. It internally manages the AVI recorder.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class H5Out : public BaseSaver {
private:
	Spinnaker::AVIRecorder recorder;
	// TODO: make a small class so that we have just one vector of that class (for cleanliness)
	std::vector<DataSet> datasets; // collection of datasets
	std::vector<DataSet> tsdatasets; // collection of datasets for timestamps
	const std::vector<std::string> dsnames; // collection of dataset names
	const std::vector<PredType> datatypes; // collection of datatypes

	const int ndims = 4;
	std::vector< std::vector<size_t> > frameDims;

	// TODO: Methods to de-duplicate code
	//void initDataset(std::string& dsname) {
	//}

public:
	H5Out(std::string& _filename, BaseAcquirer* _acquirers, const size_t _frameChunkSize) :
			BaseSaver(_filename, _acquirers, _frameChunkSize) {
		// Initialize time DCPL
		DSetCreatPropList time_dcpl;
		const int time_ndims = 2;
		size_t time_chunk_dims[time_ndims] = { frameChunkSize, 1 };
		time_dcpl.setChunk(time_ndims, time_chunk_dims);

		// Initialize frameDims
		for (int i = 0; i < numStreams; i++) {
			frameDims.push_back(_acquirers[i]->getDims());
		}

		// Initialize frame datasets
		for (int i = 0; i < numStreams; i++) {
			// Prepare dataspace
			size_t* dims = new size_t[ndims];
			dims[0] = frameChunkSize;
			dims[1] = frameDims[i][0]; dims[2] = frameDims[i][0]; dims[3] = frameDims[i][0];
			size_t* maxdims = new size_t[ndims];
			maxdims[0] = H5S_UNLIMITED;
			maxdims[1] = frameDims[i][0]; maxdims[2] = frameDims[i][1]; maxdims[3] = frameDims[i][2];
			DataSpace* dataspace = new DataSpace(ndims, dims, maxdims);

			// Create dataset
			datasets.push_back(file.createDataSet(dsnames[i].c_str(), datatypes[i], *dataspace, _dcpls[i]));

			delete dims;
			delete maxdims;
			delete dataspace;
		}
		// Initialize timestamp datasets
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
			tsdatasets.push_back(file.createDataSet((dsnames[i] + "_time").c_str(), TIMESTAMP_H5T, *dataspace, time_dcpl));

			delete dims;
			delete maxdims;
			delete dataspace;
		}
	}

	~H5Out() {
		debugMessage("~H5Out", DEBUG_HIDDEN_INFO);
		for (int i = 0; i < numStreams; i++) datasets[i].close();
		file.close();
	}

	// This does not modify the contents of the write buffer
	virtual bool writeFrames(size_t numFrames, size_t bufIndex) {
		bool success = true;
		/* Write frame */
		try {
			// Extend dataset
			size_t* newdims = new size_t[ndims];
			newdims[0] = framesSaved[bufIndex] + numFrames;
			newdims[1] = frameDims[bufIndex][0]; newdims[2] = frameDims[bufIndex][1]; newdims[3] = frameDims[bufIndex][2];
			debugMessage("newdims = [" + std::to_string(newdims[0]) + ", " + std::to_string(newdims[1]) + ", " + std::to_string(newdims[2]) + ", " + std::to_string(newdims[3]) + "]", DEBUG_HIDDEN_INFO);
			datasets[bufIndex].extend(newdims);
			DataSpace dataspace = datasets[bufIndex].getSpace();
			// Calculate offset
			size_t* offset = new size_t[ndims]();
			offset[0] = framesSaved[bufIndex];
			// Select hyperslab in dataset
			size_t* selectdims = new size_t[ndims];
			selectdims[0] = numFrames;
			selectdims[1] = frameDims[bufIndex][0]; selectdims[2] = frameDims[bufIndex][1]; selectdims[3] = frameDims[bufIndex][2];
			DataSpace filespace(dataspace);
			filespace.selectHyperslab(H5S_SELECT_SET, selectdims, offset);
			// Define memory space
			DataSpace memspace(ndims, selectdims, NULL);

			// Write
			size_t frameBytes = acquirers[bufIndex]->getFrameBytes();
			char* buffer = new char[frameBytes * numFrames];
			for (size_t i = 0; i < numFrames; i++) {
				writeBuffers[bufIndex][i].copyDataToBuffer(buffer + i * frameBytes);
			}
			timers.start(4);
			datasets[bufIndex].write(buffer, datatypes[bufIndex], memspace, filespace);
			timers.pause(4);
			//framesSaved[bufIndex] += numFrames;

			delete[] buffer;
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

		/* Write timestamp */ // TODO: dedup code! Also in constructor (i.e. make separate functions)
		try {
			// Extend dataset
			size_t* newdims = new size_t[2];
			newdims[0] = framesSaved[bufIndex] + numFrames;
			newdims[1] = 1;
			tsdatasets[bufIndex].extend(newdims);
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
			double* buffer = new double[numFrames];
			for (size_t i = 0; i < numFrames; i++) {
				*(buffer + i) = writeBuffers[bufIndex][i].getTimestamp();
			}
			timers.start(4);
			tsdatasets[bufIndex].write(buffer, TIMESTAMP_H5T, memspace, filespace);
			timers.pause(4);
			framesSaved[bufIndex] += numFrames;

			delete[] buffer;
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

	// Write scalar attribute to root group
	void writeScalarAttribute(std::string name, int value) {
		H5::Group root = file.openGroup("/");
		int attr_data[1] = { value };
		const H5::PredType datatype = H5::PredType::STD_I32LE;
		H5::DataSpace attr_dataspace = H5::DataSpace(H5S_SCALAR);
		H5::Attribute attribute = root.createAttribute(name, datatype, attr_dataspace);
		attribute.write(datatype, attr_data);
	}
	void writeScalarAttribute(std::string name, size_t value) {
		H5::Group root = file.openGroup("/");
		size_t attr_data[1] = { value };
		const H5::PredType datatype = H5::PredType::STD_U64LE;
		H5::DataSpace attr_dataspace = H5::DataSpace(H5S_SCALAR);
		H5::Attribute attribute = root.createAttribute(name, datatype, attr_dataspace);
		attribute.write(datatype, attr_data);
	}
	void writeScalarAttribute(std::string name, double value) {
		H5::Group root = file.openGroup("/");
		double attr_data[1] = { value };
		const H5::PredType datatype = H5::PredType::NATIVE_DOUBLE;
		H5::DataSpace attr_dataspace = H5::DataSpace(H5S_SCALAR);
		H5::Attribute attribute = root.createAttribute(name, datatype, attr_dataspace);
		attribute.write(datatype, attr_data);
	}
};
