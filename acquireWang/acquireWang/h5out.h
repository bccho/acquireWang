#pragma once
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#pragma warning(push, 0)
#include "H5Cpp.h" // HDF5
#pragma warning(pop)

using namespace std;
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
template <typename T> class H5Out : public BaseSaver<T> {
	DataSet dataset;
	const PredType datatype;

	const string dsname;
	const int ndims;
	const vector<size_t> elementDims;
	const size_t elementsPerChunk;
	size_t numElementsWritten;

public:
	H5Out(string& _name, H5File& _hdf5file, vector<size_t> _elementDims, const PredType& _datatype, DSetCreatPropList& dcpl, size_t _elementsPerChunk) :
		dsname(_name), ndims(_elementDims.size() + 1), elementDims(_elementDims), datatype(_datatype),
		elementsPerChunk(_elementsPerChunk), numElementsWritten(0) {
		// Prepare dataspace
		size_t* dims = new size_t[ndims];
		memcpy(dims + 1, elementDims.data(), (ndims - 1) * sizeof(size_t));
		dims[0] = elementsPerChunk;
		size_t* maxdims = new size_t[ndims];
		memcpy(maxdims + 1, elementDims.data(), (ndims - 1) * sizeof(size_t));
		maxdims[0] = H5S_UNLIMITED;
		DataSpace* dataspace = new DataSpace(ndims, dims, maxdims);

		// Create dataset
		dataset = _hdf5file.createDataSet(dsname.c_str(), _datatype, *dataspace, dcpl);

		delete dims;
		delete maxdims;
		delete dataspace;
	}

	template<typename T> void coutArray(int size, T* arr, const char* name) {
		cout << name << ": ";
		for (int i = 0; i < size; i++) {
			cout << arr[i] << " ";
		}
		cout << endl;
	}

	bool writeElements(size_t numElements, T* buffer) {
		try {
			//cout << dsname << endl;
			// Extend dataset
			size_t* newdims = new size_t[ndims];
			memcpy(newdims + 1, elementDims.data(), (ndims - 1) * sizeof(size_t));
			newdims[0] = numElementsWritten + numElements;
			dataset.extend(newdims);
			//coutArray(ndims, newdims, "newdims");
			DataSpace dataspace = dataset.getSpace();
			// Calculate offset
			size_t* offset = new size_t[ndims]();
			offset[0] = numElementsWritten;
			//coutArray(ndims, offset, "offset");
			// Select hyperslab in dataset
			size_t* selectdims = new size_t[ndims];
			memcpy(selectdims + 1, elementDims.data(), (ndims - 1) * sizeof(size_t));
			selectdims[0] = numElements;
			//coutArray(ndims, selectdims, "selectdims");
			DataSpace filespace(dataspace);
			filespace.selectHyperslab(H5S_SELECT_SET, selectdims, offset);
			// Define memory space
			DataSpace memspace(ndims, selectdims, NULL);

			// Write
			dataset.write(buffer, datatype, memspace, filespace);
			numElementsWritten += numElements;

			delete[] newdims;
			delete[] offset;
			delete[] selectdims;
			//delete[] memdims;
			dataspace.close();
			memspace.close();
			filespace.close();

			return true;
		}
		catch (...) {
			return false;
		}
	}

	~SaveStream() {
		dataset.close();
	}
};
