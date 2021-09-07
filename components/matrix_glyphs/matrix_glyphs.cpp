#include "matrix_glyphs.h"

#define TAG "matrix_glyphs"

using esphome::switch_::Switch;
using std::shared_ptr;
using std::unique_ptr;

namespace esphome
{
    namespace matrix_glyphs
    {
        Controller controller;

        class AnimationGlyph : public Glyph
        {
        public:
            std::vector<shared_ptr<Glyph> > glyphs_;

            virtual void draw(Offset &offset) const
            {
                long interval_in_millis = 200;
                long rawIndex = millis() / interval_in_millis;
                long index = rawIndex % glyphs_.size();

                auto glyph = glyphs_[index];
                glyph->draw(offset);
            }
        };

        class StickySwitch : public Switch
        {
        public:
            virtual void write_state(bool state) override
            {
                ESP_LOGI(TAG, "StickySwitch::write_state %s", state ? "true" : "false");
                publish_state(state);
            }
        };

        BinarySensorWidget::BinarySensorWidget() : sticky_switch_(new StickySwitch())
        {
        }
    }
}

void esphome::matrix_glyphs::SensorWidget::set_sensor(Sensor *source)
{
    source_ = source;
    if (source_->get_unit_of_measurement() == "°C" || source_->get_device_class() == "temperature")
    {
        icon = std::make_shared<MdiGlyph>("thermometer");
    }
}

void esphome::matrix_glyphs::BinarySensorWidget::set_sensor(BinarySensor *source)
{
    _source = source;
    _source->add_on_state_callback([this](bool state)
                                   { this->state_callback(state); });

    if (!on_glyph_ && !off_glyph_)
    {
        const auto &device_class = _source->get_device_class();
        if (device_class == "motion")
        {
            auto animation = std::make_shared<AnimationGlyph>();
            animation->glyphs_.push_back(std::make_shared<MdiGlyph>("run"));
            animation->glyphs_.push_back(std::make_shared<MdiGlyph>("walk"));
            on_glyph_ = animation;
        }
        else
        {
            on_glyph_ = std::make_shared<MdiGlyph>("keyboard-space");
            off_glyph_ = std::make_shared<MdiGlyph>("keyboard-space");
        }
    }
}
