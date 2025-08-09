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

#include <fstream>
#include <vector>

#define STR_ENDS_WITH(S, E) (stricmp(S + strlen(S) - (sizeof(E)-1), E) == 0)

int main(int argc, char **argv) {
	if (argc < 3) {
		puts("Usage: qoiconv <infile> <outfile>");
		puts("Examples:");
		puts("  qoiconv input.png output.qoi");
		puts("  qoiconv input.png output.qoi2");
		puts("  qoiconv input.qoi output.png");
		puts("  qoiconv input.qoi output.qoi2");
		exit(1);
	}

	void *pixels = NULL;
	int w, h, channels;
	if (STR_ENDS_WITH(argv[1], ".png")) {
		if(!stbi_info(argv[1], &w, &h, &channels)) {
			printf("Couldn't read header %s\n", argv[1]);
			exit(1);
		}

		// Force all odd encodings to be RGBA
		if(channels != 3) {
			channels = 4;
		}

		pixels = (void *)stbi_load(argv[1], &w, &h, NULL, channels);
	}
	else if (STR_ENDS_WITH(argv[1], ".qoi")) {
		qoi_desc desc;
		pixels = qoi_read(argv[1], &desc, 0);
		channels = desc.channels;
		w = desc.width;
		h = desc.height;
	}
	else if (STR_ENDS_WITH(argv[1], ".qoi2")) {
		std::ifstream inFile(argv[1], std::ios_base::binary);

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
	}

	if (pixels == NULL) {
		printf("Couldn't load/decode %s\n", argv[1]);
		exit(1);
	}

	int encoded = 0;
	if (STR_ENDS_WITH(argv[2], ".png")) {
		encoded = stbi_write_png(argv[2], w, h, channels, pixels, 0);
	}
	else if (STR_ENDS_WITH(argv[2], ".qoi")) {
		qoi_desc desc = {
			.width = (unsigned int)w,
			.height = (unsigned int)h,
			.channels = (unsigned char)channels,
			.colorspace = QOI_SRGB
		};
		encoded = qoi_write(argv[2], pixels, &desc);
	}
	else if (STR_ENDS_WITH(argv[2], ".qoi2")) {
		unsigned char* data = NULL;
		size_t data_size;
		qoi2_desc desc = {
			.width = (unsigned int)w,
			.height = (unsigned int)h,
			.channels = (unsigned char)channels,
			.colorspace = QOI2_SRGB
		};
		encoded = qoi2_encode((const unsigned char*)pixels, channels, &desc, &data, &data_size);
		if (encoded) {
			std::ofstream outfile(argv[2], std::ios::out | std::ios::binary);
			outfile.write((const char*)data, data_size);
		}
		free(data);
	}

	if (!encoded) {
		printf("Couldn't write/encode %s\n", argv[2]);
		exit(1);
	}

	free(pixels);
	return 0;
}
