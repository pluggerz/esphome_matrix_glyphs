#pragma once

#include <map>
#include <memory>

#include "esphome/core/log.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/display/display_buffer.h"

#define MATRIX_GLYPHS_TAG "matrix_glyphs"

namespace esphome
{

    namespace matrix_glyphs
    {
        using esphome::binary_sensor::BinarySensor;
        using namespace esphome::display;
        using esphome::display::Image;
        using esphome::sensor::Sensor;
        using esphome::switch_::Switch;

        class Offset
        {
            int value = 0;

        public:
            DisplayBuffer &buffer;
            Font *font;

            Offset(DisplayBuffer &buffer, Font *font) : buffer(buffer), font(font) {}
            int &operator*() { return value; }
        };

        class Glyph
        {
        public:
            virtual void draw(Offset &offset) const = 0;
        };

        class GlyphOutput
        {
        private:
            static void vprintf_(Offset &offset, int y, Font *font, Color color, TextAlign align, const char *format, va_list arg)
            {
                char buffer[256];
                int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
                if (ret > 0)
                    print(offset, y, font, color, align, buffer);
            }

        public:
            static void print(Offset &offset, int y, Font *font, Color color, const char *text)
            {
                print(offset, y, font, color, TextAlign::TOP_LEFT, text);
            }
            static void print(Offset &offset, int y, Font *font, TextAlign align, const char *text)
            {
                print(offset, y, font, COLOR_ON, align, text);
            }
            static void print(Offset &offset, int y, Font *font, const char *text)
            {
                print(offset, y, font, COLOR_ON, TextAlign::TOP_LEFT, text);
            }
            static void printf(Offset &offset, int y, Font *font, Color color, TextAlign align, const char *format, ...)
            {
                va_list arg;
                va_start(arg, format);
                vprintf_(offset, y, font, color, align, format, arg);
                va_end(arg);
            }
            static void printf(Offset &offset, int y, Font *font, Color color, const char *format, ...)
            {
                va_list arg;
                va_start(arg, format);
                vprintf_(offset, y, font, color, TextAlign::TOP_LEFT, format, arg);
                va_end(arg);
            }
            static void printf(Offset &offset, int y, Font *font, TextAlign align, const char *format, ...)
            {
                va_list arg;
                va_start(arg, format);
                vprintf_(offset, y, font, COLOR_ON, align, format, arg);
                va_end(arg);
            }
            static void printf(Offset &offset, int y, Font *font, const char *format, ...)
            {
                va_list arg;
                va_start(arg, format);
                vprintf_(offset, y, font, COLOR_ON, TextAlign::TOP_LEFT, format, arg);
                va_end(arg);
            }

            static void print(Offset &offset, int y, Font *font, Color color, TextAlign align, const char *text)
            {
                int x = *offset;
                DisplayBuffer *thisBuffer = &offset.buffer;
                int x_start, y_start;
                int width, height;
                thisBuffer->get_text_bounds(x, y, text, font, align, &x_start, &y_start, &width, &height);
                thisBuffer->print(x, y, font, color, align, text);
                *offset = x_start + width;
            }
        };

        class ImageGlyph : public virtual Glyph
        {
        private:
            Image *image_{nullptr};

        public:
            ImageGlyph(Image *image) : image_(image) {}

            virtual void draw(Offset &offset) const override
            {
                if (image_ == nullptr)
                {
                    // no image
                    return;
                }
                offset.buffer.image(*offset, 0, image_);
                *offset += image_->get_width() + 1;
            }
        };

        class Widget : public virtual Glyph
        {
        };

        class Group : virtual Glyph
        {
            std::shared_ptr<Glyph> glyph_;
            std::vector<Widget *> widgets;

        public:
            void add(Widget *widget) { widgets.push_back(widget); }
            void set_image(Image *image) { glyph_ = std::make_shared<ImageGlyph>(image); }
            void set_image(const std::shared_ptr<Glyph> &glyph) { glyph_ = glyph; }

            virtual void draw(Offset &offset) const override
            {
                if (glyph_)
                {
                    glyph_->draw(offset);
                    *offset += 1;
                }
                for (auto it = std::begin(widgets); it != std::end(widgets); ++it)
                {
                    (*it)->draw(offset);
                }
            }
        };

        class Controller : virtual Glyph
        {
        protected:
            std::map<std::string, std::string> mdi_codes_;
            std::vector<Group *> groups;
            Font *mdi_font_;
            std::string empty_string{""};

        public:
            void add(Group *group) { groups.push_back(group); }

            void set_mdi_font(Font *font) { mdi_font_ = font; }
            Font *get_mdi_font() const { return mdi_font_; }
            const std::string &get_mdi_code(const std::string alias) const
            {
                auto it = mdi_codes_.find(alias);
                if (it != mdi_codes_.end())
                    return it->second;
                return empty_string;
            }
            void add_mdi_code(const std::string &alias, const std::string &code) { mdi_codes_[alias] = code; }

            void draw_all_icons(Offset &offset) const
            {
                GlyphOutput::print(offset, 0, offset.font, "(");
                for (auto it = std::begin(mdi_codes_); it != std::end(mdi_codes_); ++it)
                {
                    auto key = it->first;
                    auto value = it->second;

                    // GlyphOutput::print(offset, 0, offset.font, key.c_str());
                    GlyphOutput::print(offset, 0, mdi_font_, value.c_str());
                    GlyphOutput::print(offset, 0, offset.font, ",");
                }
                GlyphOutput::print(offset, 0, offset.font, ")");
            }

            virtual void draw(Offset &offset) const override
            {
                for (auto it = std::begin(groups); it != std::end(groups); ++it)
                {
                    (*it)->draw(offset);
                }
            }
        };

        extern Controller controller;

        class SensorWidget : public Widget
        {
        protected:
            Sensor *source_ = nullptr;
            std::shared_ptr<Glyph> icon{};

        public:
            void set_sensor(Sensor *source);

            virtual void draw(Offset &offset) const override
            {
                if (icon)
                {
                    icon->draw(offset);
                }
                if (source_ == nullptr)
                    GlyphOutput::print(offset, 0, offset.font, "null");
                else if (!source_->has_state())
                    GlyphOutput::printf(offset, 0, offset.font, "nan%s",
                                        source_->get_unit_of_measurement().c_str());
                else
                    GlyphOutput::printf(offset, 0, offset.font, "%4.1f%s",
                                        source_->get_state(),
                                        source_->get_unit_of_measurement().c_str());
            }
        };

        class BinarySensorWidget : public Widget
        {
        protected:
            BinarySensor *_source{nullptr};
            BinarySensor *alert_sensor_{new BinarySensor()};

            Switch *sticky_switch_;
            std::shared_ptr<Glyph> on_glyph_{};
            std::shared_ptr<Glyph> off_glyph_{};

            bool is_sticky() const
            {
                return sticky_switch_->state;
            }

            void state_callback(bool state) const
            {
                if (!is_sticky())
                {
                    ESP_LOGI(MATRIX_GLYPHS_TAG, "BinarySensorWidget::state_callback->publish non sticky - %s", sticky_switch_->get_object_id().c_str());
                    alert_sensor_->publish_state(state);
                }
                else if (alert_sensor_->has_state() && alert_sensor_->state == state)
                {
                    // no change
                    return;
                }
                else if (state)
                {
                    ESP_LOGI(MATRIX_GLYPHS_TAG, "BinarySensorWidget::state_callback->publish sticky - %s", sticky_switch_->get_object_id().c_str());
                    alert_sensor_->publish_state(state);
                }
            }

        public:
            BinarySensorWidget();

            // void set_image(Image *image) { glyph_ = new ImageGlyph(image); }
            void set_sensor(BinarySensor *source);

            BinarySensor *get_alert_sensor() const { return alert_sensor_; }
            Switch *get_sticky_switch() const { return sticky_switch_; }

            virtual void draw(Offset &offset) const override
            {
                if (_source != nullptr && _source->has_state()) {
                    // the initial state is ignored :S, so lets update always
                    state_callback(_source->state);
                }

                const auto &glyph = alert_sensor_->state ? on_glyph_ : off_glyph_;
                if (glyph)
                {
                    glyph->draw(offset);
                    *offset += 1;
                }
            }
        };

        class MdiGlyph : public Glyph
        {
            std::string mdi_code;

        public:
            MdiGlyph(const std::string &str)
            {
                mdi_code = controller.get_mdi_code(str);
            }

            virtual void draw(Offset &offset) const
            {
                GlyphOutput::print(offset, 0, controller.get_mdi_font(), mdi_code.c_str());
            }
        };
    }
}