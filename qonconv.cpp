/*

Copyright (c) 2025, Roger Sanders

Based on qoiconv.c from QOI
Copyright (c) 2021, Dominic Szablewski - https://phoboslab.org

SPDX-License-Identifier: MIT


Command line tool to convert between png <> qoi format

Requires:
	-"stb_image.h" (https://github.com/nothings/stb/blob/master/stb_image.h)
	-"stb_image_write.h" (https://github.com/nothings/stb/blob/master/stb_image_write.h)
	-"qoi.h" (https://github.com/phoboslab/qoi/blob/master/qoi.h)
	-"qoi2.h" (https://github.com/RogerSanders/qon/blob/main/qoi2.h)

*/


#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define QOI_IMPLEMENTATION
#include "qoi.h"

#define QOI2_IMPLEMENTATION
#include "qoi2.h"

#define QON_IMPLEMENTATION
#include "qon.h"

#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <cstdint>
#include <utility>

#define STR_ENDS_WITH(S, E) (stricmp(S + strlen(S) - (sizeof(E)-1), E) == 0)

int main(int argc, char **argv) {
	// Set our operation defaults
	bool isUnpack = false;
	bool useInterFrameCompression = false;
	bool loopAnimation = false;
	unsigned int delay = 0;
	std::string unpackFormat;
	std::string infilePath;
	std::string outfilePath;

	// Process our command line args
	bool commandLineError = false;
	int nextArgToCheck = 1;
	if ((nextArgToCheck < (argc - 3)) && (stricmp(argv[nextArgToCheck], "unpack") == 0)) {
		isUnpack = true;
		unpackFormat = std::string(".") + argv[++nextArgToCheck];
		infilePath = argv[++nextArgToCheck];
		outfilePath = argv[++nextArgToCheck];
		if ((stricmp(unpackFormat.c_str(), ".png") != 0) && (stricmp(unpackFormat.c_str(), ".qoi") != 0) && (stricmp(unpackFormat.c_str(), ".qoi2") != 0)) {
			commandLineError = true;
		}
	}
	else if ((nextArgToCheck < (argc - 3)) && (stricmp(argv[nextArgToCheck], "pack") == 0)) {
		isUnpack = false;
		while (++nextArgToCheck < (argc - 2)) {
			if ((argv[nextArgToCheck][0] != '-') && (argv[nextArgToCheck][0] != '/')) {
				commandLineError = true;
				break;
			}
			if (argv[nextArgToCheck][1] == 'i') {
				useInterFrameCompression = true;
			}
			else if (argv[nextArgToCheck][1] == 'd') {
				delay = std::stoi(argv[++nextArgToCheck]);
			}
			else if (argv[nextArgToCheck][1] == 'l') {
				loopAnimation = true;
			}
			else {
				commandLineError = true;
				break;
			}
		}
		infilePath = argv[nextArgToCheck++];
		outfilePath = argv[nextArgToCheck++];
	}
	else {
		commandLineError = true;
	}

	// Print usage if the command line parameters are not well formed
	if (commandLineError) {
		puts("Usage: qonconv <operation>");
		puts("Operations:");
		puts("  pack [options] <infile.txt> <outfile.qon>");
		puts("     Packs a list of source files listed in <infile.txt> into <outfile.qon>");
		puts("       [options]:");
		puts("         -i: Use inter-frame compression where it results in a smaller file");
		puts("         -d <microseconds>: Delay between successive frames in microseconds");
		puts("         -l: Loop the animation sequence");
		puts("  unpack <format> <infile.qon> <outdir>");
		puts("     Unpacks each frame in <infile.qon> into the directory <outdir> in <format>");
		puts("       <format>: One of the following");
		puts("         -qoi");
		puts("         -qoi2");
		puts("         -png");
		puts("Examples:");
		puts("  qonconv pack -i -d 100000 InputFileList.txt output.qon");
		puts("  qonconv unpack png input.qon C:\\outdir");
		return 1;
	}

	if (isUnpack) {
		// Open the input file
		std::ifstream infile(infilePath, std::ios::in | std::ios::binary);
		if (!infile) {
			std::cout << "Error opening input file " << infilePath << std::endl;
			return 1;
		}

		// Read the QON header from the input file
		std::vector<unsigned char> encodedHeaderBuffer(QON_BARE_HEADER_SIZE);
		infile.read((char*)encodedHeaderBuffer.data(), encodedHeaderBuffer.size());
		qon_desc qonHeader;
		if (!qon_decode_header(encodedHeaderBuffer.data(), encodedHeaderBuffer.size(), &qonHeader)) {
			printf("Failed to decode header for input file %s\n", outfilePath.c_str());
			return 1;
		}

		// Build a QOI2 header to describe each frame
		qoi2_desc qoi2Header;
		qoi2Header.width = qonHeader.width;
		qoi2Header.height = qonHeader.height;
		qoi2Header.channels = qonHeader.channels;
		qoi2Header.colorspace = qonHeader.colorspace;

		// Read the frame index from the input file
		std::vector<unsigned char> frameIndexBuffer;
		frameIndexBuffer.resize(QON_INDEX_SIZE_PER_ENTRY * qonHeader.frame_count);
		infile.read((char*)frameIndexBuffer.data(), frameIndexBuffer.size());

		// Decode each frame in the QON file to a separate output file
		size_t frameDataFileOffset = encodedHeaderBuffer.size() + frameIndexBuffer.size();
		void* lastFramePixels = nullptr;
		for (size_t frameIndex = 0; frameIndex < qonHeader.frame_count; ++frameIndex) {
			// Retrieve the index entry for the next frame
			size_t frameOffsetAfterIndex;
			unsigned short frameFlags;
			qon_decode_index_entry(frameIndexBuffer.data(), frameIndex, &frameOffsetAfterIndex, &frameFlags);

			// Read in the raw compressed frame data from the QON file
			unsigned char frameSizeBuffer[QON_FRAME_SIZE_SIZE];
			infile.seekg(frameDataFileOffset + frameOffsetAfterIndex);
			infile.read((char*)&frameSizeBuffer[0], QON_FRAME_SIZE_SIZE);
			std::vector<unsigned char> frameBuffer;
			frameBuffer.resize(qon_decode_frame_size(&frameSizeBuffer[0]));
			infile.read((char*)frameBuffer.data(), frameBuffer.size());

			// Allocate a new pixel buffer
			void* pixels = malloc(qonHeader.width * qonHeader.height * qonHeader.channels);

			// Decode the frame
			const unsigned char* previousFrameReferenceData = ((qonHeader.flags & QON_FLAGS_USES_INTERFRAME_COMPRESSION) != 0) && ((frameFlags & QON_FRAME_FLAGS_INTERFRAME_COMPRESSION) != 0) && (frameIndex > 0) ? (const unsigned char*)lastFramePixels : nullptr;
			if (!qoi2_decode_data(frameBuffer.data(), frameBuffer.size(), &qoi2Header, previousFrameReferenceData, (unsigned char*)pixels, qoi2Header.channels)) {
				std::cout << "Failed to decode frame " << frameIndex << " in input file " << infilePath << std::endl;
				return 0;
			}

			// Build the output file path
			size_t frameDigitCount = 8;
			std::string frameIndexAsString = std::to_string(frameIndex);
			auto fileName = std::string(frameDigitCount - std::min(frameDigitCount, frameIndexAsString.length()), '0') + frameIndexAsString;
			std::string filePath = outfilePath + fileName + unpackFormat;

			// Write the frame to the target output file
			int encoded = 0;
			if (STR_ENDS_WITH(unpackFormat.c_str(), ".png")) {
				encoded = stbi_write_png(filePath.c_str(), qonHeader.width, qonHeader.height, qonHeader.channels, pixels, 0);
			}
			else if (STR_ENDS_WITH(unpackFormat.c_str(), ".qoi")) {
				qoi_desc desc = {
					.width = (unsigned int)qonHeader.width,
					.height = (unsigned int)qonHeader.height,
					.channels = (unsigned char)qonHeader.channels,
					.colorspace = qonHeader.colorspace
				};
				encoded = qoi_write(filePath.c_str(), pixels, &desc);
			}
			else if (STR_ENDS_WITH(unpackFormat.c_str(), ".qoi2")) {
				unsigned char* data = NULL;
				size_t data_size;
				qoi2_desc desc = {
					.width = (unsigned int)qonHeader.width,
					.height = (unsigned int)qonHeader.height,
					.channels = (unsigned char)qonHeader.channels,
					.colorspace = qonHeader.colorspace
				};
				encoded = qoi2_encode((const unsigned char*)pixels, qonHeader.channels, &desc, &data, &data_size);
				if (encoded) {
					std::ofstream outfile(filePath.c_str(), std::ios::out | std::ios::binary);
					outfile.write((const char*)data, data_size);
				}
				free(data);
			}
			if (!encoded) {
				printf("Couldn't write/encode %s\n", filePath.c_str());
				return 1;
			}

			// Push this pixel buffer into the last frame pixel buffer
			free(lastFramePixels);
			lastFramePixels = pixels;
		}
		free(lastFramePixels);
	}
	else {
		// Build our list of input files
		std::vector<std::string> infilePaths;
		std::ifstream listFile(infilePath);
		if (!listFile) {
			std::cout << "Error opening list file " << infilePath << std::endl;
			return 1;
		}
		std::string fileLine;
		while (std::getline(listFile, fileLine)) {
			infilePaths.push_back(fileLine);
		}
		listFile.close();

		// Create the initial QON header structure in memory
		qon_desc qonHeader;
		qonHeader.frame_count = infilePaths.size();
		qonHeader.flags = (loopAnimation ? QON_FLAGS_LOOP_ANIMATION : 0);
		qonHeader.frame_duration_in_microseconds = delay;

		// Create the empty frame index in memory
		std::vector<unsigned char> frameIndexBuffer(qonHeader.frame_count * QON_INDEX_SIZE_PER_ENTRY);

		// Open the output file, and seek to the start of the frame data region.
		std::ofstream outfile(outfilePath, std::ios::out | std::ios::binary);
		if (!outfile) {
			std::cout << "Error opening output file " << outfilePath << std::endl;
			return 1;
		}
		outfile.seekp(QON_BARE_HEADER_SIZE + frameIndexBuffer.size());

		// Add each input file to the output file
		bool addedInterFrameCompressedData = false;
		void* lastFramePixels = nullptr;
		size_t nextFrameNo = 0;
		size_t currentFileOffsetAfterHeader = 0;
		qoi2_desc qoi2Header;
		size_t maxCompressedFrameSize = 0;
		for (const auto& filePath : infilePaths) {
			// Decode the input file to a pixel array
			void* pixels = nullptr;
			int w, h, channels;
			char colorSpace;
			if (STR_ENDS_WITH(filePath.c_str(), ".png")) {
				if (!stbi_info(filePath.c_str(), &w, &h, &channels)) {
					printf("Couldn't read header %s\n", filePath.c_str());
					return 1;
				}

				// Force all odd encodings to be RGBA
				if (channels != 3) {
					channels = 4;
				}

				pixels = (void*)stbi_load(filePath.c_str(), &w, &h, NULL, channels);
				//##TODO## Allow this to be specified
				colorSpace = QOI2_SRGB;
			}
			else if (STR_ENDS_WITH(filePath.c_str(), ".qoi")) {
				qoi_desc desc;
				pixels = qoi_read(filePath.c_str(), &desc, 0);
				channels = desc.channels;
				w = desc.width;
				h = desc.height;
				colorSpace = desc.colorspace;
			}
			else if (STR_ENDS_WITH(filePath.c_str(), ".qoi2")) {
				std::ifstream inFile(filePath, std::ios_base::binary);

				inFile.seekg(0, std::ios_base::end);
				size_t length = inFile.tellg();
				inFile.seekg(0, std::ios_base::beg);

				std::vector<char> buffer;
				buffer.reserve(length);
				std::copy(std::istreambuf_iterator<char>(inFile), std::istreambuf_iterator<char>(), std::back_inserter(buffer));

				qoi2_desc desc;
				int qoi2_decode_success = qoi2_decode((unsigned char*)buffer.data(), buffer.size(), &desc, (unsigned char**)&pixels, 0);
				channels = desc.channels;
				w = desc.width;
				h = desc.height;
				colorSpace = desc.colorspace;
			}
			if (pixels == NULL) {
				printf("Couldn't load/decode %s\n", filePath.c_str());
				return 1;
			}

			// If we've just loaded the first frame, latch its image properties, otherwise validate the loaded image
			// information against the latched information.
			if (nextFrameNo == 0) {
				qonHeader.width = w;
				qonHeader.height = h;
				qonHeader.channels = channels;
				qonHeader.colorspace = colorSpace;
				qoi2Header.width = qonHeader.width;
				qoi2Header.height = qonHeader.height;
				qoi2Header.channels = qonHeader.channels;
				qoi2Header.colorspace = qonHeader.colorspace;
				maxCompressedFrameSize = qoi2_max_encoded_data_size_without_header(&qoi2Header);
			}
			else if ((w != qonHeader.width) || (h != qonHeader.height) || (channels != qonHeader.channels) || (colorSpace != qonHeader.colorspace)) {
				printf("Mismatched image properties on input file %s\n", filePath.c_str());
				return 1;
			}

			// Build a keyframe version of the image, and an optional interframe version.
			bool generatedInterFrameImage = false;
			std::vector<unsigned char> keyFrameBuffer(maxCompressedFrameSize);
			size_t keyFrameWrittenBytes = keyFrameBuffer.size();
			if (!qoi2_encode_data((const unsigned char*)pixels, channels, &qoi2Header, nullptr, &keyFrameBuffer[0], &keyFrameWrittenBytes)) {
				printf("Failed to encode input file %s\n", filePath.c_str());
				return 1;
			}
			std::vector<unsigned char> interFrameBuffer;
			size_t interFrameWrittenBytes = 0;
			if (useInterFrameCompression && (nextFrameNo > 0)) {
				interFrameBuffer.resize(maxCompressedFrameSize);
				interFrameWrittenBytes = interFrameBuffer.size();
				if (!qoi2_encode_data((const unsigned char*)pixels, channels, &qoi2Header, (const unsigned char*)lastFramePixels, &interFrameBuffer[0], &interFrameWrittenBytes)) {
					printf("Failed to encode input file %s\n", filePath.c_str());
					return 1;
				}
				generatedInterFrameImage = true;
			}

			// Write the smallest version of the frame to the file. with the leading frame size.
			unsigned char frameSizeBuffer[QON_FRAME_SIZE_SIZE];
			bool wroteInterFrameImage = false;
			size_t bytesWritten = 0;
			if (generatedInterFrameImage && (interFrameWrittenBytes < keyFrameWrittenBytes)) {
				qon_encode_frame_size(&frameSizeBuffer[0], interFrameWrittenBytes);
				outfile.write((const char*)&frameSizeBuffer[0], QON_FRAME_SIZE_SIZE);
				outfile.write((const char*)interFrameBuffer.data(), interFrameWrittenBytes);
				bytesWritten = QON_FRAME_SIZE_SIZE + interFrameWrittenBytes;
				wroteInterFrameImage = true;
				addedInterFrameCompressedData = true;
			}
			else {
				qon_encode_frame_size(&frameSizeBuffer[0], keyFrameWrittenBytes);
				outfile.write((const char*)&frameSizeBuffer[0], QON_FRAME_SIZE_SIZE);
				outfile.write((const char*)keyFrameBuffer.data(), keyFrameWrittenBytes);
				bytesWritten = QON_FRAME_SIZE_SIZE + keyFrameWrittenBytes;
				wroteInterFrameImage = false;
			}

			// Write the index entry for the frame
			unsigned short frameFlags = (wroteInterFrameImage ? QON_FRAME_FLAGS_INTERFRAME_COMPRESSION : 0);
			qon_encode_index_entry(&frameIndexBuffer[0], nextFrameNo, currentFileOffsetAfterHeader, frameFlags);

			// Push this pixel buffer into the last frame pixel buffer, and advance the frame counter.
			free(lastFramePixels);
			lastFramePixels = pixels;
			currentFileOffsetAfterHeader += bytesWritten;
			++nextFrameNo;
		}
		free(lastFramePixels);

		// Write the frame index to the output file
		outfile.seekp(QON_BARE_HEADER_SIZE);
		outfile.write((const char*)frameIndexBuffer.data(), frameIndexBuffer.size());

		// If we ended up using interframe compression, record it in the main header flags.
		if (addedInterFrameCompressedData) {
			qonHeader.flags |= (useInterFrameCompression ? QON_FLAGS_USES_INTERFRAME_COMPRESSION : 0);
		}

		// Write the header to the output file
		std::vector<unsigned char> encodedHeaderBuffer(QON_BARE_HEADER_SIZE);
		if (!qon_encode_header(&qonHeader, &encodedHeaderBuffer[0])) {
			printf("Failed to encode header for output file %s\n", outfilePath.c_str());
			return 1;
		}
		outfile.seekp(0);
		outfile.write((const char*)encodedHeaderBuffer.data(), encodedHeaderBuffer.size());
	}

	return 0;
}
