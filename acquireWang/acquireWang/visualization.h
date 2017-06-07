// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#pragma once
#pragma warning(push, 0)
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

#include <cstdarg>
#include <sstream>
#include <vector>
#pragma warning(pop)

enum class stream_format : int32_t
{
	any = 0,
	z16 = 1,  ///< 16 bit linear depth values. The depth is meters is equal to depth scale * pixel value
	disparity16 = 2,  ///< 16 bit linear disparity values. The depth in meters is equal to depth scale / pixel value
	xyz32f = 3,  ///< 32 bit floating point 3D coordinates.
	yuyv = 4,
	rgb8 = 5,
	bgr8 = 6,
	rgba8 = 7,
	bgra8 = 8,
	y8 = 9,
	y16 = 10,
	raw10 = 11  ///< Four 10-bit luminance values encoded into a 5-byte macropixel
};

inline void make_depth_histogram(uint8_t rgb_image[640 * 480 * 3], const uint16_t depth_image[], int width, int height)
{
	static uint32_t histogram[0x10000];
	memset(histogram, 0, sizeof(histogram));

	for (int i = 0; i < width*height; ++i) ++histogram[depth_image[i]];
	for (int i = 2; i < 0x10000; ++i) histogram[i] += histogram[i - 1]; // Build a cumulative histogram for the indices in [1,0xFFFF]
	for (int i = 0; i < width*height; ++i)
	{
		if (uint16_t d = depth_image[i])
		{
			int f = histogram[d] * 255 / histogram[0xFFFF]; // 0-255 based on histogram location
			rgb_image[i * 3 + 0] = (uint8_t) (255 - f);
			rgb_image[i * 3 + 1] = 0;
			rgb_image[i * 3 + 2] = (uint8_t) f;
		}
		else
		{
			rgb_image[i * 3 + 0] = 20;
			rgb_image[i * 3 + 1] = 5;
			rgb_image[i * 3 + 2] = 0;
		}
	}
}

//////////////////////////////
// Simple font loading code //
//////////////////////////////

#include "stb_easy_font.h"

inline int get_text_width(const char * text)
{
	return stb_easy_font_width((char *)text);
}

inline void draw_text(int x, int y, const char * text)
{
	char buffer[20000]; // ~100 chars
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 16, buffer);
	glDrawArrays(GL_QUADS, 0, 4 * stb_easy_font_print((float)x, (float)(y - 7), (char *)text, nullptr, buffer, sizeof(buffer)));
	glDisableClientState(GL_VERTEX_ARRAY);
}

////////////////////////
// Image display code //
////////////////////////

class texture_buffer
{
	GLuint texture;
	std::vector<uint8_t> rgb;
public:
	texture_buffer() : texture() {}

	GLuint get_gl_handle() const { return texture; }

	void upload(const void * data, int width, int height, stream_format format)
	{
		// If the frame timestamp has changed since the last time show(...) was called, re-upload the texture
		if (!texture) glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		switch (format)
		{
		case stream_format::any:
			throw std::runtime_error("not a valid format");
		case stream_format::z16:
		case stream_format::disparity16:
			rgb.resize(width * height * 3);
			make_depth_histogram(rgb.data(), reinterpret_cast<const uint16_t *>(data), width, height);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
			break;
		case stream_format::xyz32f:
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_FLOAT, data);
			break;
		case stream_format::yuyv: // Display YUYV by showing the luminance channel and packing chrominance into ignored alpha channel
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);
			break;
		case stream_format::rgb8: case stream_format::bgr8: // Display both RGB and BGR by interpreting them RGB, to show the flipped byte ordering. Obviously, GL_BGR could be used on OpenGL 1.2+
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			break;
		case stream_format::rgba8: case stream_format::bgra8: // Display both RGBA and BGRA by interpreting them RGBA, to show the flipped byte ordering. Obviously, GL_BGRA could be used on OpenGL 1.2+
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			break;
		case stream_format::y8:
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
			break;
		case stream_format::y16:
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, data);
			break;
		case stream_format::raw10:
			// Visualize Raw10 by performing a naive downsample. Each 2x2 block contains one red pixel, two green pixels, and one blue pixel, so combine them into a single RGB triple.
			rgb.clear(); rgb.resize(width / 2 * height / 2 * 3);
			auto out = rgb.data(); auto in0 = reinterpret_cast<const uint8_t *>(data), in1 = in0 + width * 5 / 4;
			for (int y = 0; y<height; y += 2)
			{
				for (int x = 0; x<width; x += 4)
				{
					*out++ = in0[0]; *out++ = (in0[1] + in1[0]) / 2; *out++ = in1[1]; // RGRG -> RGB RGB
					*out++ = in0[2]; *out++ = (in0[3] + in1[2]) / 2; *out++ = in1[3]; // GBGB 
					in0 += 5; in1 += 5;
				}
				in0 = in1; in1 += width * 5 / 4;
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width / 2, height / 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
			break;
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void show(float rx, float ry, float rw, float rh) const
	{
		glBindTexture(GL_TEXTURE_2D, texture);
		glEnable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2f(rx, ry);
		glTexCoord2f(1, 0); glVertex2f(rx + rw, ry);
		glTexCoord2f(1, 1); glVertex2f(rx + rw, ry + rh);
		glTexCoord2f(0, 1); glVertex2f(rx, ry + rh);
		glEnd();
		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void show(const void * data, int width, int height, stream_format format, const std::string & caption, int rx, int ry, int rw, int rh)
	{
		if (!data) return;

		upload(data, width, height, format);

		float h = (float)rh, w = (float)rh * width / height;
		if (w > rw)
		{
			float scale = rw / w;
			w *= scale;
			h *= scale;
		}

		show(rx + (rw - w) / 2, ry + (rh - h) / 2, w, h);

		std::ostringstream ss; ss << caption << ": " << width << " x " << height;
		glColor3f(0, 0, 0);
		draw_text(rx + 9, ry + 17, ss.str().c_str());
		glColor3f(1, 1, 1);
		draw_text(rx + 8, ry + 16, ss.str().c_str());
	}
};

inline void draw_depth_histogram(const uint16_t depth_image[], int width, int height)
{
	static uint8_t rgb_image[640 * 480 * 3];
	make_depth_histogram(rgb_image, depth_image, width, height);
	glDrawPixels(width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb_image);
}

//////////////////
// GUI elements //
//////////////////

struct int2 { int x, y; };
struct rect {
	int x0, y0, x1, y1;
	bool contains(const int2 & p) const { return x0 <= p.x && y0 <= p.y && p.x < x1 && p.y < y1; }
	rect shrink(int amt) const { return{ x0 + amt, y0 + amt, x1 - amt, y1 - amt }; }
};
struct color { float r, g, b; };

class GUI {
public:
	static void label(const int2 & p, const color & c, const char * format, ...)
	{
		va_list args;
		va_start(args, format);
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);
		glColor3f(c.r, c.g, c.b);
		draw_text(p.x, p.y, buffer);
	}

	static void fill_rect(const rect & r, const color & c)
	{
		glBegin(GL_QUADS);
		glColor3f(c.r, c.g, c.b);
		glVertex2i(r.x0, r.y0);
		glVertex2i(r.x0, r.y1);
		glVertex2i(r.x1, r.y1);
		glVertex2i(r.x1, r.y0);
		glEnd();
	}

	static void progress_bar(const rect & r, const double progress, const std::string & label) {
		fill_rect(r, { 0.9f, 0.9f, 0.9f });
		rect r_inside = r.shrink(2);
		r_inside.x1 = (int)(r_inside.x0 + (r_inside.x1 - r_inside.x0) * progress);
		fill_rect(r_inside, { 0, 0.8f, 0 });
		glColor3f(0, 0, 0);
		draw_text(r.x0 + 4, r.y1 - 8, label.c_str());
		glColor3f(1, 1, 1);
	}
};