#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <linux/fb.h>
#include <stdint.h>
#include <stdlib.h>

/* libpng16 */
# include <png.h>

#define FH_ERROR_OK     0   /* no error occurred */
#define FH_ERROR_FILE   1   /* read/access error */
#define FH_ERROR_FORMAT 2   /* file format error */

/* TODO: Add support for other image types */

class image_loader {
private:
    enum file_type_t {
        none,
        bmp,
        png,
        jpeg
    };
    file_type_t m_file_type;

    int m_x, m_y;
    unsigned int m_width, m_height;

    /* PNG */
    png_byte m_color_type;
    png_byte m_bit_depth;
    png_structp m_png_ptr;
    png_infop m_info_ptr;
    int m_number_of_passes;
    png_bytep *m_row_pointers;

public:

    ///////////////////////////////////////////////////////////

    /* All member functions defined within this class, as it saves file size when compiling
        Listed here are method declarations for reader convenience */

    // image_loader();
    // ~image_loader();

    // void destroy();
    // int copy_to_framebuffer(void *hdc, struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, int posx, int posy);
    // int load_image(const char *file_name);
    // int image_size(const char *file_name, int *x, int *y);
    // int load_png(const char *file_name);
    // int png_size(const char *name, int *x, int *y);

    ///////////////////////////////////////////////////////////

    /* Constructor:
        use of initializer list prevents unnecessary calls to default constructors of member variables */
    image_loader() :
        m_file_type(none), m_png_ptr(0), m_info_ptr(0), m_row_pointers(0)
    {
        // printf("ctor image_loader\n");
    }

    /* Destructor */
    ~image_loader() {
        // printf("dtor ~image_loader\n");
        destroy();
    }


    /* Clean up member files */
    void destroy() {
        if (m_png_ptr)
            png_destroy_read_struct(&m_png_ptr, m_info_ptr?(&m_info_ptr):((png_infopp)NULL), (png_infopp)NULL);
        if (m_row_pointers) {
            for (m_y=0; m_y<static_cast<int>(m_height); m_y++)
                if (m_row_pointers[m_y])
                    free(m_row_pointers[m_y]);
            free(m_row_pointers);
        }
        m_file_type = none;
        m_png_ptr = 0;
        m_info_ptr = 0;
        m_row_pointers = 0;
    }



    /* Copy the loaded image to the framebuffer */
    int copy_to_framebuffer(void *hdc, struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, int posx, int posy) {
        if (m_file_type == png) {
            int x, y;
            for (y = posy, m_y=0; m_y<static_cast<int>(m_height); y++, m_y++) {
                png_byte *row = m_row_pointers[m_y];
                for (x = posx, m_x=0; m_x<static_cast<int>(m_width); x++, m_x++) {
                    png_byte *ptr = &(row[m_x*4]);
                    /* RGBA: ptr[0], ptr[1], ptr[2], ptr[3] */
                    if (x < 0 || y < 0 || x >= static_cast<int>(vinfo->xres) || y >= static_cast<int>(vinfo->yres))
                        continue;
                    long location = (x+static_cast<int>(vinfo->xoffset))*(static_cast<int>(vinfo->bits_per_pixel)/8) + (y+static_cast<int>(vinfo->yoffset))*static_cast<int>(finfo->line_length);
                        uint32_t target = *(reinterpret_cast<uint32_t*>(reinterpret_cast<long>(hdc)+location));
                        /* Blend the text color into the background */
                        /* color = alpha * (src - dest) + dest */
                        unsigned int col_r = (static_cast<float>(ptr[3])/static_cast<float>(0xff))*static_cast<float>(ptr[0]-static_cast<float>((target>>16)&0xff))+((target>>16)&0xff);
                        unsigned int col_g = (static_cast<float>(ptr[3])/static_cast<float>(0xff))*static_cast<float>(ptr[1]-static_cast<float>((target>>8)&0xff))+((target>>8)&0xff);
                        unsigned int col_b = (static_cast<float>(ptr[3])/static_cast<float>(0xff))*static_cast<float>(ptr[2]-static_cast<float>(target&0xff))+(target&0xff);
                        *(reinterpret_cast<uint32_t*>(reinterpret_cast<long>(hdc)+location)) = (col_r<<16)|(col_g<<8)|(col_b);
                }
            }
        }
        destroy();
        return(FH_ERROR_OK);
    }



    /* Load the given image */
    int load_image(const char *file_name) {
        /* TODO: Support other types */
        destroy();
        m_file_type = png;
        return load_png(file_name);
    }

    /* Get the size of the given image */
    int image_size(const char *file_name, int *x, int *y) {
        /* TODO: Support other types */
        m_file_type = png;
        return png_size(file_name, x, y);
    }



    /* Load a png image */
    int load_png(const char *file_name) {
        char header[8];    // 8 is the maximum size that can be checked

        /* open file and test for it being a png */
        FILE *fp;
        if(!(fp=fopen(file_name,"rb"))) return(FH_ERROR_FILE);

        fread(header, 1, 8, fp);
        if (png_sig_cmp(reinterpret_cast<png_const_bytep>(&header[0]), 0, 8)) {
            fclose(fp);
            return(FH_ERROR_FORMAT);
        }

        /* initialize stuff */
        m_png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!m_png_ptr) {
            fclose(fp);
            return(FH_ERROR_FORMAT);
        }

        m_info_ptr = png_create_info_struct(m_png_ptr);
        if (!m_info_ptr) {
            png_destroy_read_struct(&m_png_ptr, (png_infopp)NULL, (png_infopp)NULL);
            fclose(fp);
            m_png_ptr = 0;
            return(FH_ERROR_FORMAT);
        }

        if (setjmp(png_jmpbuf(m_png_ptr))) {
            png_destroy_read_struct(&m_png_ptr, &m_info_ptr, (png_infopp)NULL);
            fclose(fp);
            m_png_ptr = 0;
            m_info_ptr = 0;
            return(FH_ERROR_FORMAT);
        }

        png_init_io(m_png_ptr, fp);
        png_set_sig_bytes(m_png_ptr, 8);

        png_read_info(m_png_ptr, m_info_ptr);

        m_width = png_get_image_width(m_png_ptr, m_info_ptr);
        m_height = png_get_image_height(m_png_ptr, m_info_ptr);
        m_color_type = png_get_color_type(m_png_ptr, m_info_ptr);
        m_bit_depth = png_get_bit_depth(m_png_ptr, m_info_ptr);

        /* read file */
        if (setjmp(png_jmpbuf(m_png_ptr))) {
            png_destroy_read_struct(&m_png_ptr, &m_info_ptr, (png_infopp)NULL);
            m_png_ptr = 0;
            m_info_ptr = 0;
            fclose(fp);
            return(FH_ERROR_FORMAT);
        }

        // printf("   width: %d, height: %d, color_type: %d, bit_depth: %d\n", static_cast<int>(m_width), static_cast<int>(m_height), static_cast<int>(m_color_type), static_cast<int>(m_bit_depth));

        /* See: http://www.vias.org/pngguide/chapter13_08.html */

        if (m_color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_expand(m_png_ptr);
        if (m_color_type == PNG_COLOR_TYPE_GRAY && m_bit_depth < 8)
            png_set_expand(m_png_ptr);
        if (png_get_valid(m_png_ptr, m_info_ptr, PNG_INFO_tRNS))
            png_set_expand(m_png_ptr);

        if (m_bit_depth == 16)
            png_set_strip_16(m_png_ptr);
        if (m_color_type == PNG_COLOR_TYPE_GRAY || m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(m_png_ptr);

        double gamma;
        if (png_get_gAMA(m_png_ptr, m_info_ptr, &gamma))
            png_set_gamma(m_png_ptr, 2.2, gamma); // display_exponent assumed 2.2 for now...

        m_number_of_passes = png_set_interlace_handling(m_png_ptr);
        png_read_update_info(m_png_ptr, m_info_ptr);

        m_row_pointers = static_cast<png_bytep*>(malloc(sizeof(png_bytep) * m_height));
        for (m_y=0; m_y<static_cast<int>(m_height); m_y++)
            m_row_pointers[m_y] = static_cast<png_byte*>(malloc(png_get_rowbytes(m_png_ptr,m_info_ptr)));

        png_read_image(m_png_ptr, m_row_pointers);
        png_read_end(m_png_ptr, NULL);

        fclose(fp);
        return(FH_ERROR_OK);
    }



    /* Get the size of the given png image */
    int png_size(const char *name, int *x, int *y) {
        png_structp png_ptr;
        png_infop info_ptr;
        png_uint_32 width, height;
        int bit_depth, color_type, interlace_type;
        FILE *fh;

        if(!(fh=fopen(name,"rb"))) return(FH_ERROR_FILE);

        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr) {
            fclose(fh);
            return(FH_ERROR_FORMAT);
        }
        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
            fclose(fh);
            m_png_ptr = 0;
            return(FH_ERROR_FORMAT);
        }
        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
            fclose(fh);
            m_png_ptr = 0;
            m_info_ptr = 0;
            return(FH_ERROR_FORMAT);
        }

        png_init_io(png_ptr,fh);
        png_read_info(png_ptr, info_ptr);
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

        *x = static_cast<int>(width);
        *y = static_cast<int>(height);

        fclose(fh);
        return(FH_ERROR_OK);
    }
};

#endif
