//
// LICENSE:
//
// Copyright (c) 2016 -- 2017 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "image.h"
#include <iostream>

// needed for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// needed for image writing
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

vec4f filmic(vec4f pixel);
vec4f gammacorrection(vec4f pixel, bool bDecoded);
vec4f expose(vec4f pixel, float exposure);
vec4f doOver(vec4f pixel, vec4f composedpixel);
vec4f multiply(vec4f pixel);


image4f load_image4f(const std::string& filename) {
	int x, y, n;
	float *image = stbi_loadf(filename.c_str(), &x, &y, &n, 4);
	auto imgHDR = image4f(x, y);
	int i = 0;
	for (std::vector<vec4f>::iterator it = imgHDR.pixels.begin();
			it != imgHDR.pixels.end(); ++it)
	{
		*it = { image[i], image[i + 1], image[i + 2], image[i + 3] };
		i = i + 4;
	}
	stbi_image_free(image);
    return imgHDR;
}

image4b load_image4b(const std::string& filename) {
	int x, y, n;
	unsigned char *data = stbi_load(filename.c_str(), &x, &y, &n, 4);
	auto imgLDR = image4b(x, y);
	int i = 0;
	for (std::vector<vec4b>::iterator it = imgLDR.pixels.begin();
			it != imgLDR.pixels.end(); ++it)
	{
		*it = { data[i], data[i + 1], data[i + 2], data[i + 3] };
		i = i + 4;
	}
	stbi_image_free(data);
	return imgLDR;
}

void save_image(const std::string& filename, const image4f& img) {
	const float *immagine = &img.pixels[0].x;
	stbi_write_hdr(filename.c_str(), img.width, img.height, 4, immagine);
}

void save_image(const std::string& filename, const image4b& img) {
	const unsigned char *immagine = &img.pixels[0].x;
	stbi_write_png(filename.c_str(), img.width, img.height, 4, immagine, img.width*4);
}

image4b tonemap(
	const image4f& hdr, float exposure, bool use_filmic, bool no_srgb)
{
	image4b tonemapped = image4b(hdr.width, hdr.height);
	for (int j = 0; j < hdr.height; ++j)
	{
		for (int i = 0; i < hdr.width; ++i)
		{
			auto pixel = hdr.at(i, j);
			pixel = expose(pixel, exposure);
			if (use_filmic)
			{
				pixel = filmic(pixel);
			}
			if (!no_srgb)
			{
				pixel = gammacorrection(pixel, true);
			}
			tonemapped.at(i, j) = { (unsigned char)(255.0f * min(1.0f ,pixel.x)), (unsigned char)(255.0f * min(1.0f ,pixel.y)), (unsigned char)(255.0f * min(1.0f ,pixel.z)), (unsigned char)(255.0f * min(1.0f ,pixel.w)) };
		}

	}
	return tonemapped;
}

image4b compose(
    const std::vector<image4b>& imgs, bool premultiplied, bool no_srgb) {
	auto composed = imgs[imgs.size() - 1];
	for (int j = 0; j < composed.height; ++j)
	{
		for (int i = 0; i < composed.width; ++i)
		{
			auto composedpixel = composed.at(i, j);
			auto composedpixelf = vec4f{ composedpixel.x / 255.0f, composedpixel.y / 255.0f, composedpixel.z / 255.0f, composedpixel.w / 255.0f };
			if (!no_srgb)
			{
				composedpixelf = gammacorrection(composedpixelf, false);
			}

			if (!premultiplied)
			{
				composedpixelf = multiply(composedpixelf);
			}
			for (int image = imgs.size() - 2; image >= 0; --image)
			{
				auto pixel = imgs[image].at(i, j);
				auto pixelf = vec4f{ pixel.x / 255.0f, pixel.y / 255.0f, pixel.z / 255.0f, pixel.w / 255.0f };
				if (!no_srgb)
				{
					pixelf = gammacorrection(pixelf, false);
				}

				if (!premultiplied)
				{
					pixelf = multiply(pixelf);
				}
				composedpixelf = doOver(pixelf, composedpixelf);
			}
			if (!no_srgb)
			{
				composedpixelf = gammacorrection(composedpixelf, true);
			}
			composed.at(i, j) = { (unsigned char)(255.0f * composedpixelf.x), (unsigned char)(255.0f * composedpixelf.y), (unsigned char)(255.0f * composedpixelf.z), (unsigned char)(255.0f * composedpixelf.w) };
		}
	}
	return composed;
}

vec4f filmic(vec4f pixel)
{
	const float a = 2.51f;
	const float b = 0.03f;
	const float c = 2.43f;
	const float d = 0.59f;
	const float e = 0.14f;
	pixel.x = (((pixel.x * (a * pixel.x + b)) / (pixel.x * (c * pixel.x + d) + e)));
	pixel.y = (((pixel.y * (a  *pixel.y + b)) / (pixel.y * (c * pixel.y + d) + e)));
	pixel.z = (((pixel.z * (a * pixel.z + b)) / (pixel.z * (c * pixel.z + d) + e)));
	return pixel;
}

vec4f gammacorrection(vec4f pixel, bool bDecoded)
{
	const float gamma = 2.2f;
	if (bDecoded)
	{
		pixel.x = pow(pixel.x, (1.0f / gamma));
		pixel.y = pow(pixel.y, (1.0f / gamma));
		pixel.z = pow(pixel.z, (1.0f / gamma));
	}
	else
	{
		pixel.x = pow(pixel.x, gamma);
		pixel.y = pow(pixel.y, gamma);
		pixel.z = pow(pixel.z, gamma);
	}
	return pixel;
}

vec4f expose(vec4f pixel, float exposure)
{
	auto exp = pow(2.0f, exposure);
	pixel.x = pixel.x * exp;
	pixel.y = pixel.y * exp;
	pixel.z = pixel.z * exp;
	return pixel;
}

vec4f doOver(vec4f pixelf, vec4f composedpixelf)
{
	composedpixelf.x = composedpixelf.x + ((1 - composedpixelf.w) * pixelf.x);
	composedpixelf.y = composedpixelf.y + ((1 - composedpixelf.w) * pixelf.y);
	composedpixelf.z = composedpixelf.z + ((1 - composedpixelf.w) * pixelf.z);
	composedpixelf.w = composedpixelf.w + ((1 - composedpixelf.w) * pixelf.w);
	return composedpixelf;
}

vec4f multiply(vec4f pixel)
{
	float alpha = pixel.w;
	pixel = { pixel.x * alpha, pixel.y * alpha, pixel.z * alpha, alpha };
	return pixel;
}
