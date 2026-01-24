#pragma once

#include <cassert>
#include <iostream>
#include <visage/app.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if VISAGE_MAC || VISAGE_LINUX || VISAGE_EMSCRIPTEN
#include <sys/mman.h>
#define USE_MMAP 1
#endif

struct Image
{
    const unsigned char* data {};
    size_t size {};

    Image() = default;

    Image (std::string path)
    {
        int fd = open (path.c_str(), O_RDONLY);
        if (fd == -1)
            assert (false);

        struct stat sb;
        if (fstat (fd, &sb) == -1)
            assert (false);

        size = sb.st_size;

#if USE_MMAP
        data = static_cast<const unsigned char*> (mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0u));
        if (data == MAP_FAILED)
            assert (false);
#else
        auto* fdata = malloc (size);
        const auto bytes_read = read (fd, fdata, size);
        assert (bytes_read == size);
        data = static_cast<const unsigned char*> (fdata);
#endif

        close (fd);
    }

    ~Image()
    {
        if (data != nullptr)
        {
#if USE_MMAP
            munmap (const_cast<unsigned char*> (data), size);
#else
            free (const_cast<unsigned char*> (data));
#endif
        }
    }
};

struct Slide_Params
{
    Image* background_image {};
    visage::Color background_color {};
};

struct Slide : visage::Frame
{
    Slide_Params params {};

    Slide (Slide_Params slide_params) : params { slide_params }
    {
    }

    ~Slide()
    {
        if (params.background_image != nullptr)
            delete params.background_image;
    }

    void resized() override
    {
        redrawAll();
    }

    void draw (visage::Canvas& canvas) override
    {
        canvas.setColor (params.background_color);
        canvas.fill (0, 0, width(), height());

        if (params.background_image != nullptr)
        {
            canvas.setColor (0xffffffff);
            canvas.image (params.background_image->data, params.background_image->size, 0, 0, width(), height());
        }
    }

    bool keyPress (const visage::KeyEvent& key) override
    {
        return false;
    }
};

struct Slideshow : visage::Frame
{
    std::vector<Slide*> slides {};
    size_t active_slide = 0;

    explicit Slideshow (std::initializer_list<Slide*> init_slides)
        : slides { init_slides.begin(), init_slides.end() }
    {
        for (auto* slide : slides)
        {
            addChild (slide, false);
        }
        if (! slides.empty())
        {
            active_slide = 0;
            slides[active_slide]->setVisible (true);
        }

        // requestKeyboardFocus();
        setAcceptsKeystrokes(true);
    }

    ~Slideshow() override
    {
        for (auto* slide : slides)
            delete slide;
    }

    void resized() override
    {
        using namespace visage::dimension;
        for (auto* slide : slides)
            slide->layout().setDimensions (100_vw, 100_vh);
    }

    void previous_slide()
    {
        if (active_slide == 0)
            return;
        slides[active_slide]->setVisible (false);
        active_slide--;
        slides[active_slide]->setVisible (true);
    }

    void next_slide()
    {
        if (active_slide == slides.size() - 1)
            return;
        slides[active_slide]->setVisible (false);
        active_slide++;
        slides[active_slide]->setVisible (true);
    }

    bool keyPress (const visage::KeyEvent& key) override
    {
        if (key.keyCode() == visage::KeyCode::Right)
        {
            next_slide();
            return true;
        }
        if (key.keyCode() == visage::KeyCode::Left)
        {
            previous_slide();
            return true;
        }

        return false;
    }
};
