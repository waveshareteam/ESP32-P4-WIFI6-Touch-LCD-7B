#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "esp_dsp.h"
#include "board.h"

namespace esp_brookesia::apps
{
    class SpecAnalyzer : public systems::phone::App
    {
    public:
        static SpecAnalyzer *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
        ~SpecAnalyzer();

        using systems::phone::App::endRecordResource;
        using systems::phone::App::startRecordResource;

    protected:
        SpecAnalyzer(bool use_status_bar, bool use_navigation_bar);

        bool run(void) override;
        bool back(void) override;
        bool close(void) override;
        bool init(void) override;
        bool deinit(void) override;
        bool pause(void) override;
        bool resume(void) override;

    private:
        static SpecAnalyzer *_instance;

        // Capture complete ES7210 voice frames; the board microphone layout says which samples are microphones.
        static constexpr uint16_t N_SAMPLES = 1024;
        static constexpr uint16_t CAPTURE_CHANNELS = CODEC_VOICE_INPUT_CHANNELS;
        // The spectrum view has room for two channels; extra board microphones are not shown.
        static constexpr uint16_t MAX_MIC_COUNT = 2;
        static constexpr uint16_t STRIPE_COUNT = 48;
        // Horizontal space kept clear on each side of the bars.
        static constexpr int SIDE_MARGIN = 60;
        static constexpr int CHANNEL_GAP = 24;

        // Microphones shown, from board_audio_get_mic_layout().
        uint16_t _mic_count;
        uint8_t _mic_channel_index[MAX_MIC_COUNT];
        // Canvas geometry follows the panel resolution.
        int _canvas_width;
        int _canvas_height;
        int _channel_width;

        lv_obj_t *_canvas;
        lv_obj_t *_mic_labels[MAX_MIC_COUNT];
        lv_timer_t *_timer;
        TaskHandle_t _audio_task_handle;
        bool _audio_task_running;

        // Microphone visualization buffers.
        __attribute__((aligned(16))) int16_t _raw_data[N_SAMPLES * CAPTURE_CHANNELS]; // Packed microphone samples
        __attribute__((aligned(16))) float _audio_buffer[MAX_MIC_COUNT][N_SAMPLES];   // Normalized microphone samples
        __attribute__((aligned(16))) float _wind[N_SAMPLES];                      // Shared Hann window
        __attribute__((aligned(16))) float _fft_buffer[MAX_MIC_COUNT][N_SAMPLES * 2]; // Complex FFT input
        __attribute__((aligned(16))) float _spectrum[MAX_MIC_COUNT][N_SAMPLES / 2];   // Spectrum in dB
        float _display_spectrum[MAX_MIC_COUNT][STRIPE_COUNT];                         // Mapped spectrum for bars
        float _peak[MAX_MIC_COUNT][STRIPE_COUNT];                                     // Peak marker position
        float _smooth_spectrum[MAX_MIC_COUNT][STRIPE_COUNT];                          // Smoothed display values

        lv_color_t *_draw_buf; // PSRAM-preferred canvas draw buffer

        // nullptr shows the per-channel names; any other text replaces them on every label.
        void setMicLabelText(const char *status);
        // Read the microphone layout and size the canvas; false if the board has no usable microphone.
        bool loadLayout();

        static void audio_fft_task(void *pvParameters);
        static void timer_cb(lv_timer_t *timer);
    };
}
